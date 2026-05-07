#ifndef METRICAS_H
#define METRICAS_H

#include "parametros.h"

/* Resultados de rendimiento recolectados durante la ejecución del benchmark. */
typedef struct {
    Parametros p;              /* Parámetros con los que se ejecutó el benchmark. */
    double     tiempo_total;   /* Tiempo de pared total del bucle (segundos).     */
    double    *tiempos_iter;   /* Tiempo de pared por iteración (arreglo de l).   */
    double     tiempo_promedio;/* Promedio de tiempos_iter.                        */
    long       pico_mem_kb;    /* Pico de RSS observado durante el bucle (KB).    */
    long      *rss_iter;       /* RSS al finalizar cada iteración (arreglo de l). */
    double    *cpu_iter;       /* CPU% por iteración (arreglo de l).              */
    long       tam_A_bytes;    /* Tamaño de la matriz A en bytes.                 */
    long       tam_Z_bytes;    /* Tamaño de la matriz Z en bytes.                 */
    long       fallos_pagina_menor; /* Fallos de página menores durante el bucle. */
    long       fallos_pagina_mayor; /* Fallos de página mayores durante el bucle. */
} Metricas;

/* Inicializa todos los campos de m y reserva los arreglos de l elementos. */
void metricas_init(Metricas *m, int l);

/* Libera los arreglos dinámicos de la estructura de métricas. */
void metricas_free(Metricas *m);

/* Imprime el informe de métricas en la salida estándar. */
void metricas_imprimir(const Metricas *m, int l);

/* Guarda el informe de métricas en un archivo dentro de outdir. */
void metricas_guardar(const Metricas *m, int l, const char *outdir);

#endif
