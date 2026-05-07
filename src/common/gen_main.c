#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define MKDIR(p) mkdir(p, 0755)
#endif
#include "gen.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s EXP\n", argv[0]);
        return 1;
    }

    int input = atoi(argv[1]);
    if (input <= 0) {
        fprintf(stderr, "Error: EXP debe ser un entero positivo\n");
        return 1;
    }

    int m = (int)pow(2, input);
    int n = 128;
    int l = (2 * m) / n;

    long long bytes_A = (long long)m * m * sizeof(float);
    long long bytes_Z = (long long)m * n * sizeof(float);
    double mb_A = bytes_A / (1024.0 * 1024.0);
    double mb_Z = bytes_Z / (1024.0 * 1024.0);

    const char *dir = "data";
    MKDIR(dir);

    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/params.txt", dir);
    FILE *f = fopen(ruta, "w");
    if (!f) { fprintf(stderr, "Error: no se pudo abrir %s\n", ruta); return 1; }
    fprintf(f, "input %d\n"
               "m %d\n"
               "n %d\n"
               "l %d\n"
               "A: %d x %d floats = %.1f MB\n"
               "Z: %d x %d floats = %.1f MB\n",
               input, m, n, l,
               m, m, (float)mb_A,
               m, n, (float)mb_Z);
    fclose(f);
    printf("Params guardados en %s\n", ruta);

    generar_matrices(dir, m, n);

    printf("\nMatrices listas en '%s/'\n", dir);
    return 0;
}
