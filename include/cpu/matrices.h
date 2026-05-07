#ifndef MATRICES_H
#define MATRICES_H

#include "parametros.h"

/* Reserva una matriz [filas x cols] con valores aleatorios entre 0 y 1. */
float **crear_matriz(int filas, int cols);

/* Imprime el nombre, dimensiones y tamaño en bytes de una matriz. */
void print_tamano(const char *nombre, int filas, int cols);

/* Libera la memoria de cada fila y del arreglo de punteros de la matriz. */
void liberar_matriz(float **matriz, int filas);

/* Copia las primeras n filas de src en dst (ambos pre-asignados como n×n). */
void copiar_snapshot_en(float **src, int n, float **dst);

/* Multiplica A [filas_A x cols_A] por B [cols_A x cols_B] y escribe en C pre-asignado. */
void multiplicar_en(float **A, int filas_A, int cols_A, float **B, int cols_B, float **C);

/* Guarda la matriz mat [filas x cols] en un archivo de texto dentro de outdir. */
void guardar_resultado_txt(float **mat, int filas, int cols, int iter, const char *outdir);

/* Lee una matriz [filas x cols] desde un archivo binario row-major de floats. */
float **leer_matriz_bin(const char *ruta, int filas, int cols);

/* Carga A (m×m) y Z (m×n) desde A.bin y Z.bin dentro de dir. Retorna 0 en éxito, -1 en error. */
int cargar_matrices(const char *dir, int m, int n, float ***out_A, float ***out_Z);

/* Lee MemAvailable de /proc/meminfo en KB. Retorna -1 si no está disponible. */
long long leer_ram_disponible_kb(void);

/* Calcula BS = sqrt(ram_kb * 1024 * pct / 24), redondeado al múltiplo de 64 inferior. */
int calcular_block_size(long long ram_kb, double pct);

/* Retorna 1 si A(m×m) + Z(m×n) + resultado(m×n) + snapshot(n×n) caben en ram_kb * pct bytes. */
int matrices_caben_en_ram(int m, int n, long long ram_kb, double pct);

#endif
