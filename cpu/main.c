#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define MKDIR(p) mkdir(p, 0755)
#endif
#include "parametros.h"
#include "benchmark.h"

int main(void) {
    int input;

    // Solicitar el exponente al usuario
    printf("Ingrese el exponente: ");

    // Validar que la entrada sea un entero
    if (scanf("%d", &input) != 1) {
        printf("Error: entrada invalida\n");
        return 1;
    }

    // Calcular dimensiones a partir del exponente
    Parametros p = {
        .input = input,
        .m     = (int)pow(2, input),
        .n     = 128,
        .l     = 0
    };

    // Calcular número de iteraciones como (2*m)/n
    p.l = (2 * p.m) / p.n;

    // Obtener fecha y hora actual para nombrar el directorio
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char ts[16];
    strftime(ts, sizeof(ts), "%m%d_%H%M", tm_info);
    char outdir[64];
    snprintf(outdir, sizeof(outdir), "cpu_%d_%s", input, ts);

    // Crear directorio de salida con la marca de tiempo
    if (MKDIR(outdir) != 0) {
        fprintf(stderr, "Error: no se pudo crear directorio %s\n", outdir);
        return 1;
    }

    // Registrar el directorio de salida para uso del Makefile
    FILE *lf = fopen(".last_outdir", "w");
    if (lf) { fprintf(lf, "%s\n", outdir); fclose(lf); }

    // Imprimir el directorio de salida
    printf("Directorio de salida: %s\n", outdir);

    // Mostrar parámetros y ejecutar el benchmark
    print_parametros(p);
    benchmark(p, outdir);
    return 0;
}
