#!/bin/bash
mkdir -p bin obj
g++ -Wall -pthread src/*.cpp -o bin/lab06
echo "Compilación terminada. Ejecuta ./bin/lab06"
