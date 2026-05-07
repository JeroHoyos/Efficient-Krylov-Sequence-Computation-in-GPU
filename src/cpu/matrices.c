#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "matrices.h"

/* Reserva e inicializa una matriz de [filas x cols] con valores aleatorios entre 0 y 1. */
float **crear_matriz(int filas, int cols) {
    float **matriz = malloc(filas * sizeof(float *));
    for (int i = 0; i < filas; i++) {
        matriz[i] = malloc(cols * sizeof(float));
        for (int j = 0; j < cols; j++)
            matriz[i][j] = (float)rand() / RAND_MAX;
    }
    return matriz;
}

/* Imprime el nombre, dimensiones y tamaño en bytes (con unidad apropiada) de una matriz. */
void print_tamano(const char *nombre, int filas, int cols) {
    long long bytes = (long long)filas * cols * sizeof(float);
    printf("  Matriz %s [%d x %d]: ", nombre, filas, cols);
    if      (bytes >= 1024LL * 1024 * 1024 * 1024)
        printf("%lld bytes (%.2f TB)\n", bytes, bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024LL * 1024 * 1024)
        printf("%lld bytes (%.2f GB)\n", bytes, bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024 * 1024)
        printf("%lld bytes (%.2f MB)\n", bytes, bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        printf("%lld bytes (%.2f KB)\n", bytes, bytes / 1024.0);
    else
        printf("%lld bytes\n", bytes);
}

/* Libera la memoria de cada fila y del arreglo de punteros de la matriz. */
void liberar_matriz(float **matriz, int filas) {
    for (int i = 0; i < filas; i++) free(matriz[i]);
    free(matriz);
}

/* Copia las primeras n filas de src (de m filas) en dst (n×n, pre-asignado). */
void copiar_snapshot_en(float **src, int n, float **dst) {
    for (int i = 0; i < n; i++)
        memcpy(dst[i], src[i], n * sizeof(float));
}

/* Multiplica A [filas_A x cols_A] por B [cols_A x cols_B] y escribe el resultado en C pre-asignado. */
void multiplicar_en(float **A, int filas_A, int cols_A, float **B, int cols_B, float **C) {
    for (int i = 0; i < filas_A; i++) {
        for (int j = 0; j < cols_B; j++) {
            float suma = 0.0f;
            for (int k = 0; k < cols_A; k++)
                suma += A[i][k] * B[k][j];
            C[i][j] = suma;
        }
    }
}

/* Guarda la matriz mat [filas x cols] como texto plano en outdir/resultado_<iter>.txt. */
void guardar_resultado_txt(float **mat, int filas, int cols, int iter, const char *outdir) {
    char nombre[256];
    snprintf(nombre, sizeof(nombre), "%s/resultado_%d.txt", outdir, iter);
    FILE *f = fopen(nombre, "w");
    if (!f) { fprintf(stderr, "Error: no se pudo abrir %s\n", nombre); return; }
    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < cols; j++)
            fprintf(f, j < cols - 1 ? "%f " : "%f", mat[i][j]);
        fprintf(f, "\n");
    }
    fclose(f);
    printf("  Guardado: %s\n", nombre);
}

/* Lee una matriz [filas x cols] desde un archivo binario row-major de floats. */
float **leer_matriz_bin(const char *ruta, int filas, int cols) {
    FILE *f = fopen(ruta, "rb");
    if (!f) { fprintf(stderr, "Error: no se pudo abrir %s\n", ruta); return NULL; }
    float **mat = malloc(filas * sizeof(float *));
    for (int i = 0; i < filas; i++) {
        mat[i] = malloc(cols * sizeof(float));
        fread(mat[i], sizeof(float), cols, f);
    }
    fclose(f);
    return mat;
}

/* Carga A (m×m) y Z (m×n) desde A.bin y Z.bin dentro de dir. Retorna 0 en éxito, -1 en error. */
int cargar_matrices(const char *dir, int m, int n, float ***out_A, float ***out_Z) {
    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/A.bin", dir);
    printf("  Cargando A desde %s...\n", ruta);
    *out_A = leer_matriz_bin(ruta, m, m);
    if (!*out_A) return -1;
    snprintf(ruta, sizeof(ruta), "%s/Z.bin", dir);
    printf("  Cargando Z desde %s...\n", ruta);
    *out_Z = leer_matriz_bin(ruta, m, n);
    if (!*out_Z) { liberar_matriz(*out_A, m); return -1; }
    return 0;
}

/* Lee MemAvailable de /proc/meminfo en KB. Retorna -1 si no está disponible. */
long long leer_ram_disponible_kb(void) {
#ifdef __linux__
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    char linea[128];
    long long kb = -1;
    while (fgets(linea, sizeof(linea), f))
        if (sscanf(linea, "MemAvailable: %lld kB", &kb) == 1) break;
    fclose(f);
    return kb;
#else
    return -1;
#endif
}

/* Calcula BS = sqrt(ram_kb * 1024 * pct / 24), redondeado al múltiplo de 64 inferior. */
int calcular_block_size(long long ram_kb, double pct) {
    double bs = sqrt((double)(ram_kb * 1024LL) * pct / 24.0);
    int BS = (int)(bs / 64.0) * 64;
    return BS < 64 ? 64 : BS;
}

/* Retorna 1 si A(m×m) + Z(m×n) + resultado(m×n) + un snapshot(n×n) caben en ram_kb*pct bytes. */
int matrices_caben_en_ram(int m, int n, long long ram_kb, double pct) {
    if (ram_kb <= 0) return 1;
    long long needed = (long long)m * m * sizeof(float)
                     + 2LL * m * n * sizeof(float)
                     + (long long)n * n * sizeof(float);
    long long avail  = (long long)((double)(ram_kb * 1024LL) * pct);
    return needed <= avail;
}
