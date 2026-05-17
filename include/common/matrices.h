#ifndef MATRICES_H
#define MATRICES_H

float **crear_matriz(int filas, int cols);
void   liberar_matriz(float **matriz, int filas);

float **leer_matriz_bin(const char *ruta, int filas, int cols);
int    cargar_matrices(const char *dir, int m, int n, float ***out_A, float ***out_Z);
void   guardar_snapshot(float **mat, int n, int iter, const char *outdir);

#endif
