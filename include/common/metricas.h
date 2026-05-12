#ifndef METRICAS_H
#define METRICAS_H

#include <stdint.h>

/* Una muestra por iteración */
typedef struct {
    double tiempo_ms;   /* duración de la multiplicación  */
    double gflops;      /* GFLOPs efectivos               */
} Muestra;

/* Contenedor de todas las muestras */
typedef struct {
    Muestra *muestras;
    int      n;         /* número de iteraciones registradas */
    int      cap;       /* capacidad actual del array        */
} Metricas;

void   metricas_init(Metricas *m, int capacidad);
void   metricas_registrar(Metricas *m, double tiempo_ms,
                           long long flops);
void   metricas_imprimir(const Metricas *m);
void   metricas_guardar_csv(const Metricas *m, const char *outdir);
void   metricas_free(Metricas *m);

/* Helpers de tiempo portables */
double tiempo_ms_ahora(void);   /* devuelve ms desde epoch */

#endif
