// src/p2_ring.cpp
#include "utils.h"
#include <pthread.h>
#include <vector>
#include <iostream>

struct Ring {
    int n;                 // # hilos
    long rounds;           // # de vueltas del token
    long pass = 0;         // pases hechos (0..n*rounds-1)
    long total = 0;        // n * rounds
    int current = 0;       // a quién le toca (id)
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  cv  = PTHREAD_COND_INITIALIZER;
    std::vector<long> hits; // cuántas veces recibió el token cada hilo
};

struct Args {
    Ring* ring;
    int id;
};

void* worker(void* p) {
    auto* a = static_cast<Args*>(p);
    Ring* r = a->ring;
    const int id = a->id;

    while (true) {
        pthread_mutex_lock(&r->mtx);

        while (r->current != id && r->pass < r->total)
            pthread_cond_wait(&r->cv, &r->mtx);

        if (r->pass >= r->total) {
            pthread_mutex_unlock(&r->mtx);
            break;
        }

        // Tengo el token
        r->hits[id]++;
        r->pass++;
        r->current = (r->current + 1) % r->n;

        pthread_cond_broadcast(&r->cv);
        pthread_mutex_unlock(&r->mtx);
    }
    return nullptr;
}

int main(int argc, char** argv) {
    print_banner("Practica 2 - Anillo de hilos (token)");

    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <hilos> <vueltas>\n";
        return 1;
    }
    int n      = arg_to_int_or(argv[1], 4);
    long rounds = arg_to_int_or(argv[2], 5);
    if (n <= 0 || rounds <= 0) {
        std::cerr << "Parámetros deben ser positivos.\n";
        return 1;
    }

    Ring ring;
    ring.n = n;
    ring.rounds = rounds;
    ring.total = rounds * 1L * n;
    ring.hits.assign(n, 0);

    std::vector<pthread_t> th(n);
    std::vector<Args> args(n);
    for (int i = 0; i < n; ++i) {
        args[i] = { &ring, i };
        pthread_create(&th[i], nullptr, worker, &args[i]);
    }
    for (int i = 0; i < n; ++i) pthread_join(th[i], nullptr);

    std::cout << "T=" << n << "  vueltas=" << rounds
              << "  pases=" << ring.pass << "\n\n";
    long sum = 0;
    for (int i = 0; i < n; ++i) {
        std::cout << "hilo[" << i << "] recibió " << ring.hits[i] << " veces\n";
        sum += ring.hits[i];
    }
    std::cout << "\nVerificación: sum(hits) = " << sum
              << " (esperado " << ring.total << ")\n";
    return 0;
}
