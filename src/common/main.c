// Punto de entrada del benchmark de GEMM
//
// Lee los parámetros generados por gen.exe, crea un directorio de salida
// con timestamp y delega la ejecución a benchmark.h.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <direct.h>
#include "parametros.h"
#include "benchmark.h"

// Lee input, m, n y l desde 'data/params.txt' generado por gen.exe.
int leer_params(const char *dir, Parametros *p) {
    
    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/params.txt", dir);

    FILE *f = fopen(ruta, "r");
    if (!f) { fprintf(stderr, "Error: no se pudo abrir %s\n", ruta); return -1; }

    int leidos = fscanf(f, "input %d\nm %d\nn %d\nl %d\n",
                        &p->input, &p->m, &p->n, &p->l);
    fclose(f);

    // fscanf debe leer exactamente 4 campos; cualquier otro resultado indica
    // que el archivo está corrupto o fue generado por una versión distinta.
    if (leidos != 4) {
        fprintf(stderr, "Error: formato inválido en %s\n", ruta);
        return -1;
    }
    return 0;
}

// Crea el directorio de salida con formato "gpu_EXP_MMDD_HHMMSS" o "cpu_...".
// También escribe la ruta en '.last_outdir' para que scripts externos la lean.
int crear_outdir(int input, char *outdir, size_t size) {
    time_t t           = time(NULL);
    struct tm *tm_info = localtime(&t);
    char ts[16];
    strftime(ts, sizeof(ts), "%m%d_%H%M%S", tm_info);

#if defined(USE_CUDA)
    snprintf(outdir, size, "gpu_%d_%s", input, ts);
#else
    snprintf(outdir, size, "cpu_%d_%s", input, ts);
#endif

    if (_mkdir(outdir) != 0) {
        fprintf(stderr, "Error: no se pudo crear %s\n", outdir);
        return -1;
    }

    // '.last_outdir' permite que scripts de análisis encuentren el resultado
    // más reciente sin necesidad de parsear nombres de directorio.
    FILE *lf = fopen(".last_outdir", "w");
    if (lf) { fprintf(lf, "%s\n", outdir); fclose(lf); }

    printf("Directorio de salida: %s\n", outdir);
    return 0;
}

int main(void) {

    const char *matrices_dir = "data";
    Parametros p;

    if (leer_params(matrices_dir, &p) != 0) {return 1;}

    #if defined(USE_CUDA)
        printf("\n[Modo: GPU / CUDA]\n");
    #else
        printf("\n[Modo: CPU]\n");
    #endif

    print_parametros(p);

    char outdir[64];
    if (crear_outdir(p.input, outdir, sizeof(outdir)) != 0) {return 1;}

    benchmark(p, matrices_dir, outdir);
    return 0;
}