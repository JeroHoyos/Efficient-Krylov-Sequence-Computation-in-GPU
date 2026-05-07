#include <stdio.h>
#include <stdlib.h>
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
   Retorna 0 en éxito, -1 en error. */
int crear_outdir(int input, char *outdir, size_t size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char ts[16];
    strftime(ts, sizeof(ts), "%m%d_%H%M%S", tm_info);
    snprintf(outdir, size, "cpu_%d_%s", input, ts);
    if (MKDIR(outdir) != 0) {
        fprintf(stderr, "Error: no se pudo crear directorio %s\n", outdir);
        return -1;
    }
    FILE *lf = fopen(".last_outdir", "w");
    if (lf) { fprintf(lf, "%s\n", outdir); fclose(lf); }
    printf("Directorio de salida: %s\n", outdir);
    return 0;
}

/* Lee los parametros desde dir/params.txt. Retorna 0 en éxito, -1 en error. */
int leer_params(const char *dir, Parametros *p) {
    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/params.txt", dir);
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", ruta);
        return -1;
    }
    if (fscanf(f, "input %d\nm %d\nn %d\nl %d\n",
               &p->input, &p->m, &p->n, &p->l) != 4) {
        fprintf(stderr, "Error: formato invalido en %s\n", ruta);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *matrices_dir = argc > 1 ? argv[1] : "data";

    Parametros p;
    if (leer_params(matrices_dir, &p) != 0)
        return 1;

    char outdir[64];
    if (crear_outdir(p.input, outdir, sizeof(outdir)) != 0)
        return 1;

    print_parametros(p);
    benchmark(p, matrices_dir, outdir);
    return 0;
}
