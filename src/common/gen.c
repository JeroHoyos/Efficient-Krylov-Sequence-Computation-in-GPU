#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include "gen.h"

// Genera A [m x m] como matriz estocástica por filas y la guarda en binario.
void gen_A_bin(const char *ruta, int m) {
    FILE *f = fopen(ruta, "wb");
    if (!f) { fprintf(stderr, "Error: no se pudo crear %s\n", ruta); return; }

    // Se reserva solo una fila a la vez para mantener el uso de RAM constante
    // sin importar el tamaño de m.
    float *fila = malloc(m * sizeof(float));

    // 'periodo' controla cada cuántas filas se actualiza el porcentaje.
    // El mínimo de 1 evita división por cero cuando m <= 10.
    int periodo = (m > 10) ? m / 10 : 1;

    for (int i = 0; i < m; i++) {
        float suma = 0;
        for (int j = 0; j < m; j++) {
            fila[j] = (float)rand() / RAND_MAX;
            suma   += fila[j];
        }
        // Normaliza la fila para que sume 1.0 (distribución estocástica).
        for (int j = 0; j < m; j++) fila[j] /= suma;
        fwrite(fila, sizeof(float), m, f);

        if (i % periodo == 0) {
            printf("\r  Generando A: %3d%%", (int)(100LL * i / m));
            fflush(stdout);
        }
    }
    // '%-40s' alinea a la izquierda con relleno para borrar cualquier
    // texto residual del porcentaje que quedara en la misma línea.
    printf("\r  Guardada: %-40s\n", ruta);
    free(fila);
    fclose(f);
}

// Genera Z [m x n] con flotantes aleatorios en [0, 1) y la guarda en binario.
void gen_Z_bin(const char *ruta, int m, int n) {
    
    FILE *f = fopen(ruta, "wb");
    if (!f) { fprintf(stderr, "Error: no se pudo crear %s\n", ruta); return; }

    // 'n' columnas por fila, mucho menor que m, así que la reserva es pequeña.
    float *fila = malloc(n * sizeof(float));
    int periodo = (m > 10) ? m / 10 : 1;

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) fila[j] = (float)rand() / RAND_MAX;
        fwrite(fila, sizeof(float), n, f);

        if (i % periodo == 0) {
            printf("\r  Generando Z: %3d%%", (int)(100LL * i / m));
            fflush(stdout);
        }
    }
    printf("\r  Guardada: %-40s\n", ruta);
    free(fila);
    fclose(f);
}

// Guarda los parámetros de la sesión en 'data/params.txt' como texto plano.
void guardar_parametros(const char *dir, int input, int m, int n, int l) {
    double mb_A = (long long)m * m * sizeof(float) / (1024.0 * 1024.0);
    double mb_Z = (long long)m * n * sizeof(float) / (1024.0 * 1024.0);

    // Crea el directorio antes de intentar abrir el archivo dentro de él.
    // _mkdir() falla si ya existe, pero eso no es un error — se ignora.
    _mkdir(dir);

    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/params.txt", dir);

    FILE *f = fopen(ruta, "w");
    if (!f) { fprintf(stderr, "Error: no se pudo abrir %s\n", ruta); return; }

    fprintf(f,
        "input %d\nm %d\nn %d\nl %d\n"
        "A: %d x %d floats = %.1f MB\n"
        "Z: %d x %d floats = %.1f MB\n",
        input, m, n, l,
        m, m, (float)mb_A,
        m, n, (float)mb_Z);

    fclose(f);
    printf("Params guardados en %s\n", ruta);
}

// Organiza la generación de ambas matrices.
void generar_matrices(const char *dir, int m, int n) {

    char ruta[256];

    printf("Generando A [%d x %d]...\n", m, m);
    snprintf(ruta, sizeof(ruta), "%s/A.bin", dir);
    gen_A_bin(ruta, m);

    printf("Generando Z [%d x %d]...\n", m, n);
    snprintf(ruta, sizeof(ruta), "%s/Z.bin", dir);
    gen_Z_bin(ruta, m, n);
}