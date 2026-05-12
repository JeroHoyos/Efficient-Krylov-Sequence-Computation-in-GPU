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

    int input = atoi(argv[1]);

    // atoi() devuelve 0 para "0" y para cadenas no numéricas — rechaza ambos.
    if (input <= 0) {
        fprintf(stderr, "Error: EXP debe ser un entero positivo\n");
        return 1;
    }

    // Calculando parametros
    int m = (int)pow(2, input);
    int n = 128;
    int l = (2 * m) / n;

    // Calcula el tamaño aproximado de la matriz A en diferentes unidades.
    long long bytes_A = (long long)m * m * sizeof(float);
    double mb_A = bytes_A / (1024.0 * 1024.0);
    double gb_A = mb_A   / 1024.0;
    double tb_A = gb_A   / 1024.0;

    printf("\nSe generará A [%d x %d]\n", m, m);

    // Muestra la unidad más grande cuyo valor sea >= 1.0.
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

    // _mkdir() falla si el directorio ya existe, pero no es un error — se ignora.
    const char *dir = "data";
    _mkdir(dir);

    // Care vergas lean el código
    guardar_parametros(dir, input, m, n, l);
    generar_matrices(dir, m, n);

    printf("\nMatrices generadas correctamente en '%s/'\n", dir);
    return 0;
}