#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "matrices.h"

float **crear_matriz(int filas, int cols) {

    float **m = malloc(filas * sizeof(float *));

    for (int i = 0; i < filas; i++) {
        m[i] = malloc(cols * sizeof(float));
    }

    return m;
}

void liberar_matriz(float **matriz, int filas) {

    for (int i = 0; i < filas; i++) {
        free(matriz[i]);
    }

    free(matriz);
}


void copiar_snapshot_en(float **src, int n, float **dst) {

    for (int i = 0; i < n; i++) {
        memcpy(dst[i], src[i], n * sizeof(float));
    }
}

void guardar_resultado_txt(float **mat, int filas, int cols,
                           int iter, const char *outdir) {
    char nombre[256];
    snprintf(nombre, sizeof(nombre), "%s/resultado_%04d.txt", outdir, iter);

    FILE *f = fopen(nombre, "w");
    if (!f) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", nombre);
        return;
    }

    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < cols; j++)
            fprintf(f, j < cols - 1 ? "%f " : "%f", mat[i][j]);
        fprintf(f, "\n");
    }

    fclose(f);
    printf("  Guardado: %s\n", nombre);
}

float **leer_matriz_bin(const char *ruta, int filas, int cols) {

    FILE *f = fopen(ruta, "rb");
    if (!f) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", ruta);
        return NULL;
    }

    float **mat = crear_matriz(filas, cols);

    for (int i = 0; i < filas; i++)
        fread(mat[i], sizeof(float), cols, f);

    fclose(f);
    return mat;
}

int cargar_matrices(const char *dir, int m, int n,
                    float ***out_A, float ***out_Z) {
    char ruta[256];

    snprintf(ruta, sizeof(ruta), "%s/A.bin", dir);
    printf("  Cargando A...\n");
    
    *out_A = leer_matriz_bin(ruta, m, m);

    if (!*out_A) {return -1;}

    snprintf(ruta, sizeof(ruta), "%s/Z.bin", dir);
    printf("  Cargando Z...\n");
    *out_Z = leer_matriz_bin(ruta, m, n);

    if (!*out_Z) {
        liberar_matriz(*out_A, m);
        return -1;
    }

    return 0;
}