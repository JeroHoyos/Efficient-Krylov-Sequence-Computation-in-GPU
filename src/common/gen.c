//! gen.c - Generación de los binarios de las matrices A y Z, y guardar los parámetros en un archivo de texto.
//!
//! Este módulo contiene las funciones para generar la matriz A, Z y guardar los parámetros de la sesión.
//! La función main() se encuentra en gen_main.c, que organiza el flujo general del programa.
//!

#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include "gen.h"

/// Genera la matriz A en formato binario.
///
/// La matriz A es cuadrada [m x m] y derecha estocástica (cada fila suma 1.0).
///
/// # Argumentos
/// - `ruta`: Ruta del archivo binario donde se guardará la matriz A.
/// - `m`: Tamaño de la matriz (número de filas y columnas).
///
void gen_A_bin(const char *ruta, int m) {

    FILE *f = fopen(ruta, "wb");
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear %s\n", ruta);
        return;
    }

    // Se reutiliza el mismo buffer de fila en cada iteración para evitar reservas repetidas.
    float *fila = malloc(m * sizeof(float));

    for (int i = 0; i < m; i++) {

        float suma = 0;

        for (int j = 0; j < m; j++) {
            fila[j] = (float)rand() / RAND_MAX;
            suma    += fila[j];
        }

        // Divide cada elemento por la suma de la fila para que sume exactamente 1.0.
        for (int j = 0; j < m; j++) {
            fila[j] /= suma;
        }

        fwrite(fila, sizeof(float), m, f);
    }

    free(fila);
    fclose(f);
    printf("Guardada: %-40s\n", ruta);
}

/// Genera la matriz Z en formato binario.
///
/// La matriz Z es rectangular [m x n] con valores flotantes aleatorios en [0, 1).
///
/// # Argumentos
/// - `ruta`: Ruta del archivo binario donde se guardará la matriz Z.
/// - `m`: Número de filas de la matriz Z.
/// - `n`: Número de columnas de la matriz Z.
///
void gen_Z_bin(const char *ruta, int m, int n) {

    FILE *f = fopen(ruta, "wb");
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear %s\n", ruta);
        return;
    }

    // Se reutiliza el mismo buffer de fila en cada iteración para evitar reservas repetidas.
    float *fila = malloc(n * sizeof(float));

    for (int i = 0; i < m; i++) {

        for (int j = 0; j < n; j++) {
            fila[j] = (float)rand() / RAND_MAX;
        }

        fwrite(fila, sizeof(float), n, f);
    }

    free(fila);
    fclose(f);
    printf("Guardada: %-40s\n", ruta);
}

/// Guarda los parámetros de la sesión en un archivo de texto.
///
/// Escribe input, m, n, l y el tamaño en MB de las matrices A y Z en params.txt
/// dentro del directorio especificado.
///
/// # Argumentos
/// - `dir`: Directorio donde se guardará el archivo de parámetros.
/// - `input`: Exponente dado por el usuario (m = 2^input).
/// - `m`: Número de filas y columnas de A; número de filas de Z.
/// - `n`: Número de columnas de Z (128).
/// - `l`: (2*m)/n.
///
void guardar_parametros(const char *dir, int input, int m, int n, int l) {

    double mb_A = (long long)m * m * sizeof(float) / (1024.0 * 1024.0);
    double mb_Z = (long long)m * n * sizeof(float) / (1024.0 * 1024.0);

    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/params.txt", dir);

    // Aunque la extensión es .txt, se abre en modo binario para consistencia
    // con el resto del módulo y evitar conversiones de salto de línea en Windows.
    FILE *f = fopen(ruta, "wb");
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear %s\n", ruta);
        return;
    }

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

/// Orquesta la generación de A, Z y el archivo de parámetros.
///
/// Llama a gen_A_bin(), gen_Z_bin() y guardar_parametros() en ese orden,
/// construyendo las rutas de salida a partir del directorio base.
///
/// # Argumentos
/// - `dir`: Directorio de salida para todos los archivos generados.
/// - `input`: Exponente dado por el usuario (m = 2^input).
/// - `m`: Tamaño de A [m x m] y número de filas de Z.
/// - `n`: Número de columnas de Z (128).
/// - `l`: (2*m)/n.
///
void generar_matrices(const char *dir, int input, int m, int n, int l) {

    char ruta[256];

    printf("Generando A [%d x %d]...\n", m, m);
    snprintf(ruta, sizeof(ruta), "%s/A.bin", dir);
    gen_A_bin(ruta, m);

    printf("Generando Z [%d x %d]...\n", m, n);
    snprintf(ruta, sizeof(ruta), "%s/Z.bin", dir);
    gen_Z_bin(ruta, m, n);

    guardar_parametros(dir, input, m, n, l);
}