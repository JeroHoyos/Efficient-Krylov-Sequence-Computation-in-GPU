#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "parametros.h"
#include "metricas.h"

/* Ejecuta el bucle completo de p.l iteraciones usando buffers pre-asignados,
   midiendo tiempos, memoria y CPU por iteración. */
void ejecutar_bucle(float **A, float **Z, Parametros p,
                    const char *outdir, Metricas *m);

/* Punto de entrada del benchmark: carga matrices desde matrices_dir, ejecuta, reporta y libera. */
void benchmark(Parametros p, const char *matrices_dir, const char *outdir);

#endif
