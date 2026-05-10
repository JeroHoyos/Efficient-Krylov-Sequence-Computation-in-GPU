#ifndef GEN_H
#define GEN_H

/* Genera las matrices aleatorias y las guarda en archivos */
void generar_matrices(
    const char *dir, // carpeta donde se guardan las matrices
    int m,           // tamaño de la matriz A (m x m)
    int n            // número de columnas de la matriz Z (m x n)
);

/* Guarda los parámetros del experimento en params.txt */
void guardar_parametros(
    const char *dir, // carpeta donde se guarda params.txt
    int input,       // exponente usado para calcular m = 2^input
    int m,           // tamaño de la matriz A
    int n,           // número de columnas de Z
    int l            // parámetro derivado usado en el benchmark
);

#endif
