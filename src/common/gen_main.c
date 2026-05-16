//! gen_main.c - Lectura de argumentos, generación de matrices A y Z, y guardado de parámetros.
//!
//! Este programa toma el argumento EXP y cálcula m = 2^EXP, n = 128, l = (2*m)/n.
//! Luego genera la matriz A de tamaño [m x m] y la matriz Z de tamaño [m x n], ambas con valores aleatorios entre 0 y 1. 
//! Finalmente, guarda las matrices en formato binario y los parámetros en un archivo de texto dentro del directorio "data".
//!
//! Cómo funciona
//!
//! 1. Revisa si es válido el argumento EXP y calcula m, n, l.
//! 2. Cálcula el tamaño aproximado de la matriz A y solicita confirmación para continuar.
//! 3. Genera la matriz A y la matriz Z, guardándolas en formato binario.
//! 4. Guarda los parámetros en un archivo de texto y muestra un mensaje de éxito al finalizar.
//!

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <direct.h>
#include "gen.h"

int main(int argc, char *argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <EXP>  (m = 2^EXP)\n", argv[0]);
        return 1;
    }

    // atoi() devuelve 0 para "0" y para cadenas no numéricas.
    int input = atoi(argv[1]);
    if (input <= 0) {
        fprintf(stderr, "Error: EXP debe ser un entero positivo\n");
        return 1;
    }

    // Calculando parámetros
    int m = (int)pow(2, input);
    int n = 128;
    int l = (2 * m) / n;

    // Calcula el tamaño aproximado de la matriz A en diferentes unidades.
    long long bytes_A = (long long)m * m * sizeof(float);
    double mb_A = bytes_A / (1024.0 * 1024.0);
    double gb_A = mb_A   / 1024.0;
    double tb_A = gb_A   / 1024.0;

    printf("\nSe generará A [%d x %d]\n", m, m);

    if (tb_A >= 1.0) {
        printf("Tamaño aproximado: %.2f TB\n", tb_A);
    }
    else if (gb_A >= 1.0) {
        printf("Tamaño aproximado: %.2f GB\n", gb_A);
    }
    else if (mb_A >= 1.0) {
        printf("Tamaño aproximado: %.2f MB\n", mb_A);
    }
    else {
        printf("Tamaño aproximado: %lld Bytes\n", bytes_A);
    }

    // Solicita confirmación para evitar generar archivos enormes por accidente.
    char confirmacion;
    printf("¿Continuar? (s/n): ");
    scanf(" %c", &confirmacion);
    if (confirmacion != 's' && confirmacion != 'S') {
        printf("Operación cancelada.\n");
        return 0;
    }

    // _mkdir() falla si el directorio ya existe, se ignora el error.
    const char *dir = "data";
    _mkdir(dir);

    generar_matrices(dir, input, m, n, l);

    printf("\nMatrices generadas correctamente en '%s/'\n", dir);
    return 0;
}