#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "parametros.h"

/*
 * Ejecuta el benchmark completo:
 *   - Carga A y Z desde matrices_dir
 *   - Itera p.l multiplicaciones
 *   - Guarda snapshots y métricas en outdir
 *
 * La implementación de la multiplicación es transparente:
 * en CPU enlaza block_mul_cpu.c, en GPU enlaza block_mul_gpu.cu.
 */
void benchmark(Parametros p,
               const char *matrices_dir,
               const char *outdir);

#endif
