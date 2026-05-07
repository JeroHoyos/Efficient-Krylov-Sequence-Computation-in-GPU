#ifndef BLOCK_MUL_H
#define BLOCK_MUL_H

#include "parametros.h"
#include "metricas.h"

#ifndef RAM_PCT
#define RAM_PCT 50
#endif

/* Ejecuta p.l iteraciones de Z_{i+1} = A × Z_i leyendo desde archivos binarios
   y procesando por bloques de BS×BS. Escribe snapshots n×n en outdir y métricas en m. */
void ejecutar_bucle_bloques(const char *matrices_dir, Parametros p,
                            const char *outdir, Metricas *m, int BS);

#endif
