#include <stdio.h>
#include <stdlib.h>
#include "gen.h"

/* Reserva e inicializa una matriz [filas x cols] con valores aleatorios entre 0 y 1. */
static float **crear_matriz(int filas, int cols) {

    // Reservar arreglo de punteros a filas
    float **mat = malloc(filas * sizeof(float *));

    for (int i = 0; i < filas; i++) {

        // Reservar memoria para cada fila
        mat[i] = malloc(cols * sizeof(float));

        for (int j = 0; j < cols; j++)
            // Asignar valor aleatorio entre 0 y 1
            mat[i][j] = (float)rand() / RAND_MAX;
    }
    return mat;
}

/* Guarda la matriz mat [filas x cols] como texto plano en la ruta indicada. */
static void guardar_txt(float **mat, int filas, int cols, const char *ruta) {

    // Abrir el archivo para escritura
    FILE *f = fopen(ruta, "w");
    if (!f) { fprintf(stderr, "Error: no se pudo abrir %s\n", ruta); return; }

    // Escribir cada elemento separado por espacios, con salto de línea al final de cada fila
    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < cols; j++)
            fprintf(f, j < cols - 1 ? "%f " : "%f", mat[i][j]);
        fprintf(f, "\n");
    }

    // Cerrar el archivo e informar la ruta guardada
    fclose(f);
    printf("  Guardada: %s\n", ruta);
}

/* Libera la memoria de cada fila y del arreglo de punteros de la matriz. */
static void liberar_matriz(float **mat, int filas) {

    // Liberar memoria de cada fila
    for (int i = 0; i < filas; i++) free(mat[i]);

    // Liberar arreglo de punteros a filas
    free(mat);
}

/* Genera A (m×m) normalizada por filas y Z (m×n) aleatorias, y las guarda en dir. */
void generar_matrices(const char *dir, int m, int n) {
    char ruta[256];

    // Crear la matriz A cuadrada con valores aleatorios
    printf("Generando A [%d x %d]...\n", m, m);
    float **A = crear_matriz(m, m);

    // Normalizar cada fila de A para que sume 1 (matriz estocástica)
    for (int i = 0; i < m; i++) {
        float suma = 0.0f;
        for (int j = 0; j < m; j++) suma += A[i][j];
        for (int j = 0; j < m; j++) A[i][j] /= suma;
    }

    // Guardar A en disco y liberar su memoria
    snprintf(ruta, sizeof(ruta), "%s/A.txt", dir);
    guardar_txt(A, m, m, ruta);
    liberar_matriz(A, m);

    // Crear la matriz Z rectangular con valores aleatorios
    printf("Generando Z [%d x %d]...\n", m, n);
    float **Z = crear_matriz(m, n);

    // Guardar Z en disco y liberar su memoria
    snprintf(ruta, sizeof(ruta), "%s/Z.txt", dir);
    guardar_txt(Z, m, n, ruta);
    liberar_matriz(Z, m);
}
