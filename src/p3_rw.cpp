#include "utils.h"
#include <pthread.h>
#include <vector>
#include <iostream>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cassert>

using clk = std::chrono::steady_clock;

struct Shared {

    long value = 0;

    // Locks
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
};

// -------------------------------
// Hilo para la corrida con MUTEX
// -------------------------------
struct ArgsMutex {
    Shared* s;
    bool is_writer;
    long ops;
    // métricas
    long reads_done = 0;
    long writes_done = 0;
};

void* worker_mutex(void* p) {
    auto* a = static_cast<ArgsMutex*>(p);
    for (long i = 0; i < a->ops; ++i) {
        if (a->is_writer) {
            pthread_mutex_lock(&a->s->mtx);
            a->s->value += 1;        // escritura simple
            pthread_mutex_unlock(&a->s->mtx);
            a->writes_done++;
        } else {
            pthread_mutex_lock(&a->s->mtx);
            volatile long x = a->s->value; // simulamos lectura
            (void)x;
            pthread_mutex_unlock(&a->s->mtx);
            a->reads_done++;
        }
    }
    return nullptr;
}

// ---------------------------------
// Hilo para la corrida con RWLOCK
// ---------------------------------
struct ArgsRW {
    Shared* s;
    bool is_writer;
    long ops;
    long reads_done = 0;
    long writes_done = 0;
};

void* worker_rw(void* p) {
    auto* a = static_cast<ArgsRW*>(p);
    for (long i = 0; i < a->ops; ++i) {
        if (a->is_writer) {
            pthread_rwlock_wrlock(&a->s->rw);
            a->s->value += 1;
            pthread_rwlock_unlock(&a->s->rw);
            a->writes_done++;
        } else {
            pthread_rwlock_rdlock(&a->s->rw);
            volatile long x = a->s->value;
            (void)x;
            pthread_rwlock_unlock(&a->s->rw);
            a->reads_done++;
        }
    }
    return nullptr;
}

static inline double ms_since(clk::time_point t0, clk::time_point t1) {
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main(int argc, char** argv) {
    print_banner("Practica 3 - Lectores/Escritores (mutex vs rwlock)");

    if (argc < 4) {
        std::cerr << "Uso: " << argv[0] << " <hilos_totales> <porc_escritores 0..100> <ops_por_hilo>\n";
        return 1;
    }
    const int  T   = arg_to_int_or(argv[1], 8);
    const int  WP  = arg_to_int_or(argv[2], 25); // porcentaje de escritores
    const long OPS = arg_to_int_or(argv[3], 200000);

    if (T <= 0 || WP < 0 || WP > 100 || OPS <= 0) {
        std::cerr << "Parámetros inválidos.\n";
        return 1;
    }

    const int W = std::max(0, std::min(T, (int)std::round(T * (WP / 100.0))));
    const int R = T - W;

    std::cout << "T=" << T << "  writers=" << W << "  readers=" << R
              << "  ops/hilo=" << OPS << "\n\n";

    // -------------------------
    // 1) Corrida con MUTEX
    // -------------------------
    {
        Shared S;
        std::vector<pthread_t> th(T);
        std::vector<ArgsMutex> args(T);

        // Asignar roles: primeros W = escritores, resto lectores
        for (int i = 0; i < T; ++i) {
            args[i] = { &S, i < W, OPS, 0, 0 };
        }

        auto t0 = clk::now();
        for (int i = 0; i < T; ++i) {
            pthread_create(&th[i], nullptr, worker_mutex, &args[i]);
        }
        for (int i = 0; i < T; ++i) pthread_join(th[i], nullptr);
        auto t1 = clk::now();

        long total_reads = 0, total_writes = 0;
        for (auto& a : args) { total_reads += a.reads_done; total_writes += a.writes_done; }

        std::cout << "[MUTEX]\n";
        std::cout << "  valor final=" << S.value
                  << "  writes=" << total_writes
                  << "  reads="  << total_reads
                  << "  tiempo=" << ms_since(t0, t1) << " ms\n\n";
    }

    // -------------------------
    // 2) Corrida con RWLOCK
    // -------------------------
    {
        Shared S;
        std::vector<pthread_t> th(T);
        std::vector<ArgsRW> args(T);

        for (int i = 0; i < T; ++i) {
            args[i] = { &S, i < W, OPS, 0, 0 };
        }

        auto t0 = clk::now();
        for (int i = 0; i < T; ++i) {
            pthread_create(&th[i], nullptr, worker_rw, &args[i]);
        }
        for (int i = 0; i < T; ++i) pthread_join(th[i], nullptr);
        auto t1 = clk::now();

        long total_reads = 0, total_writes = 0;
        for (auto& a : args) { total_reads += a.reads_done; total_writes += a.writes_done; }

        std::cout << "[RWLOCK]\n";
        std::cout << "  valor final=" << S.value
                  << "  writes=" << total_writes
                  << "  reads="  << total_reads
                  << "  tiempo=" << ms_since(t0, t1) << " ms\n";
    }

    return 0;
}
