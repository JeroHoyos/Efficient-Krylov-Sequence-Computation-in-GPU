#include <stdio.h>
#include <stdlib.h>
#include "matrices.h"

/* Reserva e inicializa una matriz de [filas x cols] con valores aleatorios entre 0 y 1. */
float **crear_matriz(int filas, int cols) {

    // Reservar arreglo de punteros a filas
    float **matriz = malloc(filas * sizeof(float *));

    for (int i = 0; i < filas; i++) {
        // Reservar memoria para cada fila
        matriz[i] = malloc(cols * sizeof(float));

        for (int j = 0; j < cols; j++)
            // Asignar valor aleatorio entre 0 y 1
            matriz[i][j] = (float)rand() / RAND_MAX;
    }
    // Retornar la matriz inicializada
    return matriz;
}

/* Imprime el nombre, dimensiones y tamaño en bytes (con unidad apropiada) de una matriz. */
void print_tamano(const char *nombre, int filas, int cols) {

    // Calcular tamaño en bytes de la matriz
    long long bytes = (long long)filas * cols * sizeof(float);

    // Imprimir nombre y dimensiones de la matriz
    printf("  Matriz %s [%d x %d]: ", nombre, filas, cols);

    // Imprimir tamaño en TB si es mayor a 1 TB
    if (bytes >= 1024LL * 1024 * 1024 * 1024)
        printf("%lld bytes (%.2f TB)\n", bytes, bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0));

    // Imprimir tamaño en GB si es mayor a 1 GB
    else if (bytes >= 1024LL * 1024 * 1024)
        printf("%lld bytes (%.2f GB)\n", bytes, bytes / (1024.0 * 1024.0 * 1024.0));

    // Imprimir tamaño en MB si es mayor a 1 MB
    else if (bytes >= 1024 * 1024)
        printf("%lld bytes (%.2f MB)\n", bytes, bytes / (1024.0 * 1024.0));

    // Imprimir tamaño en KB si es mayor a 1 KB
    else if (bytes >= 1024)
        printf("%lld bytes (%.2f KB)\n", bytes, bytes / 1024.0);

    // Imprimir tamaño en bytes si es menor a 1 KB
    else
        printf("%lld bytes\n", bytes);
}

/* Inicializa A (m×m) y Z (m×n) con valores aleatorios e imprime sus tamaños. */
float inicializar_matrices(Parametros p, float ***out_A, float ***out_Z) {

    printf("\n=== Inicializacion ===\n");

    // Crear la matriz A cuadrada de m×m
    *out_A = crear_matriz(p.m, p.m);

    // Crear la matriz Z rectangular de m×n
    *out_Z = crear_matriz(p.m, p.n);

    // Mostrar el tamaño de cada matriz en pantalla
    print_tamano("A", p.m, p.m);
    print_tamano("Z", p.m, p.n);
}

/* Libera la memoria de cada fila y del arreglo de punteros de la matriz. */
void liberar_matriz(float **matriz, int filas) {

    // Liberar memoria de cada fila
    for (int i = 0; i < filas; i++) { free(matriz[i]); }

    // Liberar arreglo de punteros a filas
    free(matriz);
}

/* Crea y retorna una copia profunda de la matriz cuadrada src de n×n. */
float **copiar_snapshot(float **src, int n) {

    // Reservar arreglo de punteros a filas
    float **snap = malloc(n * sizeof(float *));

    for (int i = 0; i < n; i++) {
        // Reservar memoria para cada fila
        snap[i] = malloc(n * sizeof(float));

        for (int j = 0; j < n; j++)
            // Copiar elemento por elemento desde src
            snap[i][j] = src[i][j];
    }

    // Retornar la copia de la matriz
    return snap;
}

/* Multiplica A [filas_A x cols_A] por B [cols_A x cols_B] y retorna la matriz resultado C. */
float **multiplicar_matrices(float **A, int filas_A, int cols_A, float **B, int cols_B) {

    // Reservar arreglo de punteros a filas de la matriz resultado
    float **C = malloc(filas_A * sizeof(float *));

    for (int i = 0; i < filas_A; i++) {

        // Reservar memoria para cada fila
        C[i] = malloc(cols_B * sizeof(float));

        for (int j = 0; j < cols_B; j++) {

            // Acumulador para la suma de productos
            float suma = 0.0f;

            for (int k = 0; k < cols_A; k++)
                // Multiplicar fila de A por columna de B
                suma += A[i][k] * B[k][j];

            // Asignar resultado al elemento C[i][j]
            C[i][j] = suma;
        }
    }

    // Retornar la matriz resultado
    return C;
}

/* Guarda la matriz mat [filas x cols] como texto plano en outdir/resultado_<iter>.txt. */
void guardar_resultado_txt(float **mat, int filas, int cols, int iter, const char *outdir) {

    // Construir la ruta del archivo de salida
    char nombre[256];
    snprintf(nombre, sizeof(nombre), "%s/resultado_%d.txt", outdir, iter);

    // Abrir el archivo para escritura
    FILE *f = fopen(nombre, "w");
    if (!f) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", nombre);
        return;
    }

    // Escribir cada elemento separado por espacios, con salto de línea al final de cada fila
    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < cols; j++) {
            fprintf(f, j < cols - 1 ? "%f " : "%f", mat[i][j]);
        }
        fprintf(f, "\n");
    }

    // Cerrar el archivo e informar la ruta guardada
    fclose(f);
    printf("  Guardado: %s\n", nombre);
}

/* Lee una matriz [filas x cols] desde un archivo de texto con floats separados por espacios. */
float **leer_matriz_txt(const char *ruta, int filas, int cols) {

    // Abrir el archivo para lectura
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", ruta);
        return NULL;
    }

    // Reservar arreglo de punteros a filas
    float **mat = malloc(filas * sizeof(float *));

    for (int i = 0; i < filas; i++) {

        // Reservar memoria para cada fila
        mat[i] = malloc(cols * sizeof(float));

        for (int j = 0; j < cols; j++)
            // Leer cada elemento del archivo
            fscanf(f, "%f", &mat[i][j]);
    }

    // Cerrar el archivo y retornar la matriz
    fclose(f);
    return mat;
}

/* Carga A (m×m) y Z (m×n) desde A.txt y Z.txt dentro de dir. Retorna 0 en éxito, -1 en error. */
int cargar_matrices(const char *dir, int m, int n, float ***out_A, float ***out_Z) {
    char ruta[256];

    // Construir la ruta de A.txt y cargar la matriz A
    snprintf(ruta, sizeof(ruta), "%s/A.txt", dir);
    printf("  Cargando A desde %s...\n", ruta);
    *out_A = leer_matriz_txt(ruta, m, m);
    if (!*out_A) return -1;

    // Construir la ruta de Z.txt y cargar la matriz Z
    snprintf(ruta, sizeof(ruta), "%s/Z.txt", dir);
    printf("  Cargando Z desde %s...\n", ruta);
    *out_Z = leer_matriz_txt(ruta, m, n);

    // Si Z no se pudo cargar, liberar A antes de retornar error
    if (!*out_Z) { liberar_matriz(*out_A, m); return -1; }

    return 0;
}

/* Libera el arreglo de l snapshots, donde cada snapshot tiene n filas. */
void liberar_resultado(float ***resultado, int l, int n) {

    // Liberar cada snapshot individualmente
    for (int i = 0; i < l; i++)
        liberar_matriz(resultado[i], n);

    // Liberar el arreglo de punteros a snapshots
    free(resultado);
}
