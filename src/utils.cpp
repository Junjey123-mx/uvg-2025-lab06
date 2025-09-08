#include "utils.h"
#include <iostream>
#include <string>
#include <vector>

void print_banner(const std::string& title) {
    std::cout << "=============================\n";
    std::cout << "  " << title << "\n";
    std::cout << "=============================\n";
}

int arg_to_int_or(const char* s, int fallback) {
    try {
        return std::stoi(std::string{s});
    } catch (...) {
        return fallback;
    }
}

std::vector<int> make_vec(int n, int v) {
    return std::vector<int>(n, v);
}
