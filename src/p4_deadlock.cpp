// src/p4_deadlock.cpp
#include "utils.h"            // No lo usamos directamente, pero sirve para la regla del Makefile
#include <pthread.h>
#include <unistd.h>           // usleep
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct Locks {
    pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
};

struct Args {
    int id;                   // 0 o 1
    long iters;
    int pause_us;             // pausa entre el 1er y 2do lock (microseg)
    const char* mode;         // "deadlock" | "trylock" | "ordered"
    Locks* lks;
    std::atomic<long>* hits;  // contador por hilo
    std::atomic<long>* global_hits;
};

static void* worker_deadlock(void* p) {
    auto* a = static_cast<Args*>(p);
    Locks* L = a->lks;

    for (long i = 0; i < a->iters; ++i) {
        if (a->id == 0) {
            pthread_mutex_lock(&L->A);
            if (a->pause_us > 0) usleep(a->pause_us);
            pthread_mutex_lock(&L->B);           // <- aquí se puede quedar bloqueado si el hilo 1 tomó B primero
        } else {
            pthread_mutex_lock(&L->B);
            if (a->pause_us > 0) usleep(a->pause_us);
            pthread_mutex_lock(&L->A);           // <- aquí se puede quedar bloqueado si el hilo 0 tomó A primero
        }

        // Sección crítica simulada
        a->hits[a->id].fetch_add(1, std::memory_order_relaxed);
        a->global_hits->fetch_add(1, std::memory_order_relaxed);

        // Liberar en orden inverso
        if (a->id == 0) {
            pthread_mutex_unlock(&L->B);
            pthread_mutex_unlock(&L->A);
        } else {
            pthread_mutex_unlock(&L->A);
            pthread_mutex_unlock(&L->B);
        }
    }
    return nullptr;
}

static void* worker_trylock(void* p) {
    auto* a = static_cast<Args*>(p);
    Locks* L = a->lks;

    pthread_mutex_t* first = (a->id == 0) ? &L->A : &L->B;
    pthread_mutex_t* second = (a->id == 0) ? &L->B : &L->A;

    // Backoff exponencial suave
    const int base_backoff = a->pause_us > 0 ? a->pause_us : 100;
    int backoff = base_backoff;

    for (long i = 0; i < a->iters; ++i) {
        // Tomar el primero (bloqueante)
        pthread_mutex_lock(first);
        if (a->pause_us > 0) usleep(a->pause_us);

        // Intentar el segundo sin bloquear
        while (pthread_mutex_trylock(second) != 0) {
            // No se pudo -> liberar el primero y retroceder
            pthread_mutex_unlock(first);
            usleep(backoff);
            // Aumentar un poco el backoff (máx ~50ms para no exagerar)
            backoff = backoff < 50000 ? backoff * 2 : 50000;

            // Reintentar: volver a tomar el primero
            pthread_mutex_lock(first);
            if (a->pause_us > 0) usleep(a->pause_us);
        }
        // Ya tenemos ambos

        a->hits[a->id].fetch_add(1, std::memory_order_relaxed);
        a->global_hits->fetch_add(1, std::memory_order_relaxed);

        pthread_mutex_unlock(second);
        pthread_mutex_unlock(first);

        // Relajar el backoff tras una pasada exitosa
        backoff = base_backoff;
    }
    return nullptr;
}

// Estrategia "ordered": imponer orden global por dirección de los mutex
static void* worker_ordered(void* p) {
    auto* a = static_cast<Args*>(p);
    Locks* L = a->lks;

    pthread_mutex_t* m1 = &L->A;
    pthread_mutex_t* m2 = &L->B;
    // Asegurar un orden total (por dirección de memoria, por ejemplo)
    if (m2 < m1) std::swap(m1, m2);

    for (long i = 0; i < a->iters; ++i) {
        pthread_mutex_lock(m1);
        if (a->pause_us > 0) usleep(a->pause_us);
        pthread_mutex_lock(m2);

        a->hits[a->id].fetch_add(1, std::memory_order_relaxed);
        a->global_hits->fetch_add(1, std::memory_order_relaxed);

        pthread_mutex_unlock(m2);
        pthread_mutex_unlock(m1);
    }
    return nullptr;
}

static void usage(const char* prog) {
    std::fprintf(stderr,
        "Uso: %s <mode> [iters] [pause_us]\n"
        "  <mode>      : deadlock | trylock | ordered\n"
        "  [iters]     : iteraciones por hilo (def=100000)\n"
        "  [pause_us]  : microsegundos entre el 1er y 2do lock (def=1000)\n"
        "\n"
        "Ejemplos:\n"
        "  %s deadlock 100000 5000\n"
        "  %s trylock  200000 10000\n"
        "  %s ordered  200000 0\n",
        prog, prog, prog, prog);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    const std::string mode = argv[1];
    long iters = (argc >= 3) ? std::strtol(argv[2], nullptr, 10) : 100000;
    int pause_us = (argc >= 4) ? std::atoi(argv[3]) : 1000;

    if (mode != "deadlock" && mode != "trylock" && mode != "ordered") {
        usage(argv[0]);
        return 1;
    }

    std::printf("=============================\n");
    std::printf("  Practica 4 - Deadlock\n");
    std::printf("=============================\n");
    std::printf("mode=%s  iters=%ld  pause_us=%d\n\n", mode.c_str(), iters, pause_us);

    Locks L;
    std::atomic<long> hits_per_thread[2];
    hits_per_thread[0] = 0;
    hits_per_thread[1] = 0;
    std::atomic<long> global_hits{0};

    Args a0{0, iters, pause_us, mode.c_str(), &L, hits_per_thread, &global_hits};
    Args a1{1, iters, pause_us, mode.c_str(), &L, hits_per_thread, &global_hits};

    pthread_t t0, t1;

    auto start = std::chrono::steady_clock::now();

    void* (*fn)(void*) = nullptr;
    if (mode == "deadlock") fn = &worker_deadlock;
    else if (mode == "trylock") fn = &worker_trylock;
    else fn = &worker_ordered;

    pthread_create(&t0, nullptr, fn, &a0);
    pthread_create(&t1, nullptr, fn, &a1);


    bool reported = false;
    if (mode == "deadlock") {
        long last = -1;
        for (;;) {
            usleep(200000); // 200 ms
            long cur = global_hits.load(std::memory_order_relaxed);
            if (cur == last) {
                // Sin progreso en ~200 ms; probablemente atascado
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                std::fprintf(stderr, "[WATCHDOG] Posible deadlock detectado tras %lld ms. "
                                     "Termina con Ctrl+C.\n",
                             (long long)elapsed_ms);
                reported = true;
                break;
            }
            last = cur;


            if (cur >= iters * 2) break;
        }
    }

    // En modos no-deadlock, esperamos normalmente
    if (mode != "deadlock") {
        pthread_join(t0, nullptr);
        pthread_join(t1, nullptr);
    } else {

        usleep(1000000);
    }

    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    long h0 = hits_per_thread[0].load();
    long h1 = hits_per_thread[1].load();
    long total = global_hits.load();

    std::printf("\nResumen:\n");
    std::printf("  hits[hilo 0]=%ld  hits[hilo 1]=%ld  total=%ld\n", h0, h1, total);
    std::printf("  tiempo=%.2f ms\n", ms);
    if (mode == "deadlock") {
        if (reported) {
            std::printf("  Estado: se detectó falta de progreso (deadlock). "
                        "Prueba los modos 'trylock' u 'ordered' para evitarlo.\n");
        } else {
            std::printf("  Estado: finalizado (raro en deadlock) o interrumpido.\n");
        }
    } else if (mode == "trylock") {
        std::printf("  Estrategia: trylock + backoff para esquivar interbloqueos.\n");
    } else {
        std::printf("  Estrategia: orden global de locks (jerarquía) para prevenir deadlock.\n");
    }

    return 0;
}
