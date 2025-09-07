#include <pthread.h>
#include <iostream>
using namespace std;

void* run(void*) {
    cout << "Hola desde un hilo!\n";
    return nullptr;
}

int main() {
    pthread_t t;
    pthread_create(&t, nullptr, run, nullptr);
    pthread_join(t, nullptr);
    cout << "Listo.\n";
    return 0;
}
