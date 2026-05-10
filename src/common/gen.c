#include <stdio.h>
#include <stdlib.h>
#include "gen.h"
#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(dir) _mkdir(dir)
#else
    #include <sys/stat.h>
    #define MKDIR(dir) mkdir(dir, 0777)
#endif

/* Genera la matriz A [m×m] fila por fila y la guarda en binario.
   Cada fila se normaliza para que sume 1 (row-stochastic). */
void gen_A_bin(const char *ruta, int m) {

    /* Abre el archivo binario para escritura */
    FILE *f = fopen(ruta, "wb");

    /* Verifica que el archivo se abrió correctamente */
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear %s\n", ruta);
        return;
    }

    /* Reserva memoria para una sola fila */
    float *fila = malloc(m * sizeof(float));

    /* Actualiza el progreso aproximadamente cada 10% */
    int periodo;
    if (m > 10) {periodo = m / 10;} else {periodo = 1;}

    /* Recorre todas las filas de la matriz */
    for (int i = 0; i < m; i++) {

        /* Suma de los elementos de la fila */
        float suma = 0;

        /* Genera valores aleatorios */
        for (int j = 0; j < m; j++) {
            fila[j] = (float)rand() / RAND_MAX;
            suma += fila[j];
        }

        /* Normaliza la fila para que sume 1 */
        for (int j = 0; j < m; j++) {
            fila[j] /= suma;
        }

        /* Guarda la fila en el archivo binario */
        fwrite(fila, sizeof(float), m, f);

        /* Actualiza el progreso en pantalla */
        if (i % periodo == 0) {
            printf("\r  Generando A: %3d%%", (int)(100LL * i / m));
            fflush(stdout);
        }
    }

    printf("\r  Guardada: %-40s\n", ruta);

    /* Libera memoria */
    free(fila);

    /* Cierra el archivo */
    fclose(f);
}

/* Genera la matriz Z [m×n] fila por fila y la guarda en binario */
void gen_Z_bin(
    const char *ruta, 
    int m, 
    int n
) {

    /* Abre el archivo binario para escritura */
    FILE *f = fopen(ruta, "wb");

    /* Verifica que el archivo se abrió correctamente */
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear %s\n", ruta);
        return;
    }

    /* Reserva memoria para una sola fila */
    float *fila = malloc(n * sizeof(float));

    /* Actualiza el progreso aproximadamente cada 10% */
    int periodo;

    if (m > 10) {
        periodo = m / 10;
    } else {
        periodo = 1;
    }

    /* Recorre todas las filas de la matriz */
    for (int i = 0; i < m; i++) {

        /* Genera valores aleatorios entre 0 y 1 */
        for (int j = 0; j < n; j++) {
            fila[j] = (float)rand() / RAND_MAX;
        }

        /* Guarda la fila en el archivo binario */
        fwrite(fila, sizeof(float), n, f);

        /* Actualiza el progreso en pantalla */
        if (i % periodo == 0) {
            printf("\r  Generando Z: %3d%%", (int)(100LL * i / m));
            fflush(stdout);
        }
    }

    /* Mensaje final */
    printf("\r  Guardada: %-40s\n", ruta);

    /* Libera memoria */
    free(fila);

    /* Cierra el archivo */
    fclose(f);
}

/* Guarda los parámetros del experimento en data/params.txt */
void guardar_parametros(
    const char *dir,
    int input,
    int m,
    int n,
    int l
) {

    /* Calcula memoria usada por las matrices */
    long long bytes_A = (long long)m * m * sizeof(float);
    long long bytes_Z = (long long)m * n * sizeof(float);

    /* Convierte bytes a MB */
    double mb_A = bytes_A / (1024.0 * 1024.0);
    double mb_Z = bytes_Z / (1024.0 * 1024.0);

    /* Crea la carpeta data/ */
    MKDIR(dir);

    /* Reserva espacio para guardar la ruta del archivo */
    char ruta[256];

    /* Construye el texto: data/params.txt */
    snprintf(ruta, sizeof(ruta), "%s/params.txt", dir);

    /* Abre el archivo para escritura */
    FILE *f = fopen(ruta, "w");

    /* Verifica que el archivo se abrió correctamente */
    if (!f) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", ruta);
        return;
    }

    /* Guarda los parámetros en params.txt */
    fprintf(f,
        "input %d\n"
        "m %d\n"
        "n %d\n"
        "l %d\n"
        "A: %d x %d floats = %.1f MB\n"
        "Z: %d x %d floats = %.1f MB\n",
        input, m, n, l,
        m, m, (float)mb_A,
        m, n, (float)mb_Z
    );

    /* Cierra el archivo */
    fclose(f);

    /* Mensaje de confirmación */
    printf("Params guardados en %s\n", ruta);
}

/* Genera las matrices A (m×m) y Z (m×n) y las guarda como archivos binarios */
void generar_matrices(
    const char *dir, // carpeta donde se guardan los archivos .bin
    int m,           // tamaño de la matriz A (m x m)
    int n            // número de columnas de la matriz Z (m x n)
) {
    /* Variable donde se construyen rutas de los archivos */
    char ruta[256];

    printf("Generando A [%d x %d]...\n", m, m);
    /* Guarda en 'ruta' el texto: data/A.bin */
    snprintf(ruta, sizeof(ruta), "%s/A.bin", dir);
    /* Genera y guarda A */
    gen_A_bin(ruta, m);

    printf("Generando Z [%d x %d]...\n", m, n);
    /* Guarda en 'ruta' el texto: data/Z.bin */
    snprintf(ruta, sizeof(ruta), "%s/Z.bin", dir);
    /* Genera y guarda Z */
    gen_Z_bin(ruta, m, n);
}