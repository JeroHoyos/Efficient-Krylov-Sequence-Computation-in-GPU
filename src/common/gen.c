#include <stdio.h>
#include <stdlib.h>
#include "gen.h"

/* Genera A [m×m] row-stochastic fila a fila y la escribe en binario.
   Nunca carga más de una fila en RAM (m floats). */
static void gen_A_bin(const char *ruta, int m) {
    FILE *f = fopen(ruta, "wb");
    if (!f) { fprintf(stderr, "Error: no se pudo crear %s\n", ruta); return; }

    // Buffer de una fila para mantener el working set en O(m) floats
    float *fila = malloc(m * sizeof(float));
    // Cada cuántas filas actualizar el progreso en pantalla
    int periodo = m > 10 ? m / 10 : 1;

    for (int i = 0; i < m; i++) {
        // Generar valores aleatorios y acumular la suma para normalizar
        float suma = 0.0f;
        for (int j = 0; j < m; j++) {
            fila[j] = (float)rand() / RAND_MAX;
            suma += fila[j];
        }
        // Normalizar la fila para que sume 1 (propiedad row-estocástica)
        for (int j = 0; j < m; j++) fila[j] /= suma;
        // Escribir la fila en binario row-major
        fwrite(fila, sizeof(float), m, f);

        if (i % periodo == 0) {
            printf("\r  Generando A: %3d%%", (int)(100LL * i / m));
            fflush(stdout);
        }
    }
    printf("\r  Guardada: %-40s\n", ruta);

    free(fila);
    fclose(f);
}

/* Genera Z [m×n] aleatoria fila a fila y la escribe en binario.
   Nunca carga más de una fila en RAM (n floats). */
static void gen_Z_bin(const char *ruta, int m, int n) {
    FILE *f = fopen(ruta, "wb");
    if (!f) { fprintf(stderr, "Error: no se pudo crear %s\n", ruta); return; }

    // Buffer de una fila para mantener el working set en O(n) floats
    float *fila = malloc(n * sizeof(float));
    // Cada cuántas filas actualizar el progreso en pantalla
    int periodo = m > 10 ? m / 10 : 1;

    for (int i = 0; i < m; i++) {
        // Generar n valores aleatorios en [0, 1] para esta fila
        for (int j = 0; j < n; j++)
            fila[j] = (float)rand() / RAND_MAX;
        // Escribir la fila en binario row-major
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

/* Genera A (m×m) y Z (m×n) fila a fila y las guarda como binarios en dir. */
void generar_matrices(const char *dir, int m, int n) {
    char ruta[256];

    // Generar y guardar la matriz A row-estocástica
    printf("Generando A [%d x %d]...\n", m, m);
    snprintf(ruta, sizeof(ruta), "%s/A.bin", dir);
    gen_A_bin(ruta, m);

    // Generar y guardar la matriz Z aleatoria
    printf("Generando Z [%d x %d]...\n", m, n);
    snprintf(ruta, sizeof(ruta), "%s/Z.bin", dir);
    gen_Z_bin(ruta, m, n);
}
