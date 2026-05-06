#ifndef METRICAS_H
#define METRICAS_H

#include "parametros.h"

typedef struct {
    Parametros p;
    double     tiempo_total;
    double    *tiempos_iter;
    double     tiempo_promedio;
    long       pico_mem_kb;
    long      *rss_iter;
    double    *cpu_iter;
    long       tam_A_bytes;
    long       tam_Z_bytes;
    long       fallos_pagina_menor;
    long       fallos_pagina_mayor;
} Metricas;

void metricas_init(Metricas *m, int l);
void metricas_free(Metricas *m);
void metricas_imprimir(const Metricas *m, int l);
void metricas_guardar(const Metricas *m, int l, const char *outdir);

long leer_rss_kb(void);

#endif
