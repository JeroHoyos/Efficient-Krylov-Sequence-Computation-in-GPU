#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "parametros.h"
#include "metricas.h"

/* Ejecuta una iteración: multiplica A×Z_actual y guarda el snapshot del resultado. */
void ejecutar_iteracion(float **A, float **Z_actual, Parametros p, float ***out_Z_nueva, float ***out_snap);

/* Ejecuta el bucle completo de p.l iteraciones, midiendo tiempos, memoria y CPU. */
void ejecutar_bucle(float **A, float **Z, Parametros p, float ***resultado,
                    const char *outdir, Metricas *m);

/* Punto de entrada del benchmark: carga matrices desde matrices_dir, ejecuta, reporta y libera. */
void benchmark(Parametros p, const char *matrices_dir, const char *outdir);

#endif
