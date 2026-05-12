#include <stdio.h>
#include "perfil.h"

void print_tamano(const char *nombre, int filas, int cols) {
    double mb = (double)filas * cols * sizeof(float) / (1024.0 * 1024.0);
    printf("  %-4s : %d x %d = %.2f MB\n", nombre, filas, cols, mb);
}

void print_memoria(int m, int n) {
    double mb_A = (double)m * m * sizeof(float) / (1024.0 * 1024.0);
    double mb_Z = (double)m * n * sizeof(float) / (1024.0 * 1024.0);
    printf("\n  Memoria estimada:\n");
    printf("    A           : %7.2f MB\n", mb_A);
    printf("    Z (x2 buf)  : %7.2f MB\n", mb_Z * 2.0);
    printf("    Total       : %7.2f MB\n", mb_A + mb_Z * 2.0);
}
