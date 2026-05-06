#ifndef MATRICES_H
#define MATRICES_H

#include "parametros.h"

/* Reserva e inicializa una matriz [filas x cols] con valores aleatorios entre 0 y 1. */
float **crear_matriz(int filas, int cols);

/* Inicializa las matrices A (m×m) y Z (m×n), imprime sus tamaños y los retorna por puntero. */
float inicializar_matrices(Parametros p, float ***out_A, float ***out_Z);

/* Imprime el nombre, dimensiones y tamaño en bytes (con unidad apropiada) de una matriz. */
void print_tamano(const char *nombre, int filas, int cols);

/* Libera la memoria de cada fila y del arreglo de punteros de la matriz. */
void liberar_matriz(float **matriz, int filas);

/* Crea y retorna una copia profunda de una matriz cuadrada de n×n. */
float **copiar_snapshot(float **src, int n);

/* Multiplica A [filas_A x cols_A] por B [cols_A x cols_B] y retorna la matriz resultado C. */
float **multiplicar_matrices(float **A, int filas_A, int cols_A, float **B, int cols_B);

/* Guarda la matriz mat [filas x cols] en un archivo de texto dentro de outdir. */
void guardar_resultado_txt(float **mat, int filas, int cols, int iter, const char *outdir);

/* Lee una matriz [filas x cols] desde un archivo de texto con floats separados por espacios. */
float **leer_matriz_txt(const char *ruta, int filas, int cols);

/* Carga A (m×m) y Z (m×n) desde A.txt y Z.txt dentro de dir. Retorna 0 en éxito, -1 en error. */
int cargar_matrices(const char *dir, int m, int n, float ***out_A, float ***out_Z);

/* Libera el arreglo de l snapshots, donde cada snapshot tiene n filas. */
void liberar_resultado(float ***resultado, int l, int n);

#endif
