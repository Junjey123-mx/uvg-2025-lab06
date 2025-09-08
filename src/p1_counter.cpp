#include "utils.h"

#include <pthread.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

using Clock = std::chrono::steady_clock;
using ms    = std::chrono::duration<double, std::milli>;

struct Args {
    long iters;
    long* global;
    pthread_mutex_t* mtx; // puede ser null si no se usa
};

// -------------------- Trabajadores --------------------

// 1) Naive: RACE INTENCIONADA
void* worker_naive(void* p) {
    auto* a = static_cast<Args*>(p);
    for (long i = 0; i < a->iters; ++i) {
        (*a->global)++; // sin protección
    }
    return nullptr;
}

// 2) Con mutex
void* worker_mutex(void* p) {
    auto* a = static_cast<Args*>(p);
    for (long i = 0; i < a->iters; ++i) {
        pthread_mutex_lock(a->mtx);
        (*a->global)++;
        pthread_mutex_unlock(a->mtx);
    }
    return nullptr;
}

// 3) Sharded: cada hilo acumula local y luego reducimos
struct ShardArgs {
    long iters;
    long* out_local; // apunta al slot de este hilo
};
void* worker_sharded(void* p) {
    auto* a = static_cast<ShardArgs*>(p);
    long local = 0;
    for (long i = 0; i < a->iters; ++i) local++;
    *a->out_local = local;
    return nullptr;
}

// 4) Con std::atomic<long>
struct AtomicArgs {
    long iters;
    std::atomic<long>* ag;
};
void* worker_atomic(void* p) {
    auto* a = static_cast<AtomicArgs*>(p);
    for (long i = 0; i < a->iters; ++i) {
        a->ag->fetch_add(1, std::memory_order_relaxed);
    }
    return nullptr;
}

// -------------------- Helpers --------------------

template <class F, class A>
double run_threads(int T, F fn, A mkArgs, long& /*out_value*/) {
    std::vector<pthread_t> th(T);

    auto t0 = Clock::now();
    for (int i = 0; i < T; ++i) {
        void* arg = mkArgs(i);
        pthread_create(&th[i], nullptr, fn, arg);
    }
    for (int i = 0; i < T; ++i) pthread_join(th[i], nullptr);
    auto t1 = Clock::now();

    return std::chrono::duration_cast<ms>(t1 - t0).count();
}


int main(int argc, char** argv) {
    print_banner("Practica 1 - Contador con hilos");

    // Argumentos: T (hilos) e iters (unidades de trabajo por hilo)
    int  T     = (argc > 1) ? arg_to_int_or(argv[1], 4) : 4;
    long iters = (argc > 2) ? arg_to_int_or(argv[2], 1000000) : 1000000;

    if (T <= 0) { std::cerr << "T debe ser > 0\n"; return 1; }
    if (iters <= 0) { std::cerr << "iters debe ser > 0\n"; return 1; }

    std::cout << "T=" << T << "  iters=" << iters << "\n\n";

    // --------------------------------------------------
    // 1) NAIVE (con race)
    long global_naive = 0;
    std::vector<Args> args_naive(T, Args{iters, &global_naive, nullptr});

    double t_naive = run_threads(
        T,
        worker_naive,
        [&](int i) -> void* { return &args_naive[i]; },
        global_naive
    );

    std::cout << "[NAIVE]   valor=" << global_naive
              << "  esperado=" << (long)T * iters
              << "  tiempo=" << std::fixed << std::setprecision(2) << t_naive << " ms\n";

    // --------------------------------------------------
    // 2) Con MUTEX
    long global_mutex = 0;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    std::vector<Args> args_mutex(T, Args{iters, &global_mutex, &mtx});

    double t_mutex = run_threads(
        T,
        worker_mutex,
        [&](int i) -> void* { return &args_mutex[i]; },
        global_mutex
    );

    std::cout << "[MUTEX]   valor=" << global_mutex
              << "  esperado=" << (long)T * iters
              << "  tiempo=" << std::fixed << std::setprecision(2) << t_mutex << " ms\n";

    // --------------------------------------------------
    // 3) SHARDED + REDUCE
    long global_sharded = 0;
    std::vector<long> partials(T, 0);
    std::vector<ShardArgs> args_sharded(T);
    for (int i = 0; i < T; ++i) {
        args_sharded[i].iters     = iters;
        args_sharded[i].out_local = &partials[i];
    }

    double t_sharded = run_threads(
        T,
        worker_sharded,
        [&](int i) -> void* { return &args_sharded[i]; },
        global_sharded
    );
    // reduce
    global_sharded = std::accumulate(partials.begin(), partials.end(), 0L);

    std::cout << "[SHARDED] valor=" << global_sharded
              << "  esperado=" << (long)T * iters
              << "  tiempo=" << std::fixed << std::setprecision(2) << t_sharded << " ms\n";

    // --------------------------------------------------
    // 4) ATOMIC (comparativa)
    std::atomic<long> global_atomic{0};
    struct AtomicArgs aarg { iters, &global_atomic };
    std::vector<AtomicArgs> args_atomic(T, aarg);

    double t_atomic = run_threads(
        T,
        worker_atomic,
        [&](int i) -> void* { return &args_atomic[i]; },
        *reinterpret_cast<long*>(&global_atomic) // no se usa
    );

    std::cout << "[ATOMIC]  valor=" << global_atomic.load()
              << "  esperado=" << (long)T * iters
              << "  tiempo=" << std::fixed << std::setprecision(2) << t_atomic << " ms\n";

    // --------------------------------------------------
    std::cout << "\nResumen (ms): naive=" << t_naive
              << "  mutex=" << t_mutex
              << "  sharded=" << t_sharded
              << "  atomic=" << t_atomic << "\n";

    return 0;
}
