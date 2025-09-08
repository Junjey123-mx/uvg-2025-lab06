#include "utils.h"
#include <pthread.h>
#include <queue>
#include <iostream>
#include <vector>

using namespace std;

struct Buffer {
    queue<long> q;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
    pthread_cond_t not_full  = PTHREAD_COND_INITIALIZER;
    size_t max_size = 8;
};

// Inserta en el buffer
void put(Buffer* b, long val) {
    pthread_mutex_lock(&b->mtx);
    while (b->q.size() >= b->max_size)
        pthread_cond_wait(&b->not_full, &b->mtx);

    b->q.push(val);

    pthread_cond_signal(&b->not_empty);
    pthread_mutex_unlock(&b->mtx);
}

// Saca del buffer
long get(Buffer* b) {
    pthread_mutex_lock(&b->mtx);
    while (b->q.empty())
        pthread_cond_wait(&b->not_empty, &b->mtx);

    long val = b->q.front();
    b->q.pop();

    pthread_cond_signal(&b->not_full);
    pthread_mutex_unlock(&b->mtx);
    return val;
}

// ===== Etapas del pipeline =====

struct Args {
    Buffer* in;
    Buffer* out;
    long n;
};

// Generador
void* producer(void* arg) {
    Args* a = (Args*)arg;
    for (long i = 1; i <= a->n; i++)
        put(a->out, i);
    put(a->out, -1); // fin
    return nullptr;
}

// Multiplica por 2
void* stage1(void* arg) {
    Args* a = (Args*)arg;
    while (true) {
        long x = get(a->in);
        if (x < 0) { put(a->out, -1); break; }
        put(a->out, x * 2);
    }
    return nullptr;
}

// Suma 3
void* stage2(void* arg) {
    Args* a = (Args*)arg;
    while (true) {
        long x = get(a->in);
        if (x < 0) { put(a->out, -1); break; }
        put(a->out, x + 3);
    }
    return nullptr;
}

// Consumidor final
void* consumer(void* arg) {
    Args* a = (Args*)arg;
    while (true) {
        long x = get(a->in);
        if (x < 0) break;
        // Simular trabajo
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " N\n";
        return 1;
    }
    long N = atol(argv[1]);

    Buffer b1, b2, b3;
    pthread_t p, s1, s2, c;

    Args Aprod {nullptr, &b1, N};
    Args A1 {&b1, &b2, N};
    Args A2 {&b2, &b3, N};
    Args Acons {&b3, nullptr, N};

    pthread_create(&p, nullptr, producer, &Aprod);
    pthread_create(&s1, nullptr, stage1, &A1);
    pthread_create(&s2, nullptr, stage2, &A2);
    pthread_create(&c, nullptr, consumer, &Acons);

    pthread_join(p, nullptr);
    pthread_join(s1, nullptr);
    pthread_join(s2, nullptr);
    pthread_join(c, nullptr);

    cout << "Pipeline terminado con N=" << N << endl;
    return 0;
}
