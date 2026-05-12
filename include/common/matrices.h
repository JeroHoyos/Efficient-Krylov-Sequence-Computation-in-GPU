#ifndef MATRICES_H
#define MATRICES_H

float **crear_matriz(int filas, int cols);
void   liberar_matriz(float **matriz, int filas);
void   copiar_snapshot_en(float **src, int n, float **dst);
void   guardar_resultado_txt(float **mat, int filas, int cols,
                             int iter, const char *outdir);

float **leer_matriz_bin(const char *ruta, int filas, int cols);
int    cargar_matrices(const char *dir, int m, int n,
                       float ***out_A, float ***out_Z);

#endif
