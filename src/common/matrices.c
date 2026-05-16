//! matrices.c - Funciones para manejo de matrices de floats, incluyendo lectura, escritura y liberación de memoria.
//! 
//! Este módulo contiene funciones para crear, leer y liberar matrices de floats, así como guardar snapshots de resultados.
//! La función main() se encuentra en benchmark.c, que organiza el flujo general del programa.
//!
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "matrices.h"

/// Libera la memoria de una matriz de floats dada su cantidad de filas.
///
/// Argumentos:
/// - `matriz`: Matriz a liberar.
/// - `filas`: Cantidad de filas de la matriz.
///
void liberar_matriz(float **matriz, int filas) {

    for (int i = 0; i < filas; i++) {
        free(matriz[i]);
    }

    free(matriz);
}

/// Crea una matriz de floats con las dimensiones dadas, reservando memoria para cada fila.
///
/// Argumentos:
/// - `filas`, `cols`: Dimensiones de la matriz a leer.
///
float **crear_matriz(int filas, int cols) {

    float **m = malloc(filas * sizeof(float *));

    for (int i = 0; i < filas; i++) {
        m[i] = malloc(cols * sizeof(float));
    }

    return m;
}

/// Lee y guarda una matriz de un archivo binario dado su ruta y dimensiones.
///
/// Argumentos:
/// - `ruta`: Ruta del archivo binario que contiene la matriz.
/// - `filas`, `cols`: Dimensiones de la matriz a leer.
///
float **leer_matriz_bin(const char *ruta, int filas, int cols) {

    FILE *f = fopen(ruta, "rb");
    if (!f) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", ruta);
        return NULL;
    }

    float **mat = crear_matriz(filas, cols);

    // Lee la matriz fila por fila.
    for (int i = 0; i < filas; i++) {
        fread(mat[i], sizeof(float), cols, f);
    }

    fclose(f);
    return mat;
}

/// Carga matrices A y Z desde archivos binarios en el directorio dado.
///
/// Argumentos:
/// - `dir`: Directorio donde se encuentran A.bin y Z.bin.
/// - `m`, `n`: Dimensiones de las matrices.
/// - `out_A`, `out_Z`: Punteros de salida para las matrices cargadas.
///
int cargar_matrices(const char *dir, int m, int n,float ***out_A, float ***out_Z) {
    char ruta[256];

    snprintf(ruta, sizeof(ruta), "%s/A.bin", dir);
    printf("  Cargando A...\n");
    
    *out_A = leer_matriz_bin(ruta, m, m);
    if (!*out_A) {
        return -1;
    }

    snprintf(ruta, sizeof(ruta), "%s/Z.bin", dir);
    printf("  Cargando Z...\n");

    *out_Z = leer_matriz_bin(ruta, m, n);
    if (!*out_Z) {
        liberar_matriz(*out_A, m);
        return -1;
    }

    return 0;
}

/// Toma una snapshot de las n=128 primeras filas de la matriz src y la copia a dst.
///
/// Argumentos:
/// - `src`: Matriz de origen.
/// - `dst`: Matriz de destino.
///
void guardar_snapshot(float **mat, int n, int iter, const char *outdir) {

    char nombre[256];
    snprintf(nombre, sizeof(nombre), "%s/resultado_%04d.txt", outdir, iter);

    FILE *f = fopen(nombre, "w");
    if (!f) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", nombre);
        return;
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            fprintf(f, j < n - 1 ? "%f " : "%f", mat[i][j]);
        fprintf(f, "\n");
    }

    fclose(f);
    printf("  Guardado: %s\n", nombre);
}
