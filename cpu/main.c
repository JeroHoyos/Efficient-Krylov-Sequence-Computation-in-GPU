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

/* Crea el directorio de salida con timestamp y registra su nombre en .last_outdir.
   Devuelve 0 en éxito, -1 en error. */
int crear_outdir(int input, char *outdir, size_t size) {
    // Obtener timestamp actual y formatearlo como MMDD_HHMM
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char ts[16];
    strftime(ts, sizeof(ts), "%m%d_%H%M%S", tm_info);

    // Construir nombre del directorio con formato cpu_<input>_<timestamp>
    snprintf(outdir, size, "cpu_%d_%s", input, ts);

    // Crear el directorio físicamente
    if (MKDIR(outdir) != 0) {
        fprintf(stderr, "Error: no se pudo crear directorio %s\n", outdir);
        return -1;
    }

    // Registrar el nombre del directorio en .last_outdir
    FILE *lf = fopen(".last_outdir", "w");
    if (lf) { fprintf(lf, "%s\n", outdir); fclose(lf); }

    printf("Directorio de salida: %s\n", outdir);
    return 0;
}

/* Punto de entrada: lee el exponente, configura los parámetros y lanza el benchmark. */
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

    // Crear directorio de salida con timestamp
    char outdir[64];
    if (crear_outdir(input, outdir, sizeof(outdir)) != 0)
        return 1;

    // Mostrar parámetros y ejecutar el benchmark
    print_parametros(p);
    benchmark(p, outdir);
    return 0;
}
