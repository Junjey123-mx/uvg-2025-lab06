#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

//Imprimir un banner
void print_banner(const std::string& title);

// Parsea entero seguro desde argv.
int  arg_to_int_or(const char* s, int fallback);

// Crea un vector con n elementos inicializados en v.
std::vector<int> make_vec(int n, int v);

#endif // UTILS_H

