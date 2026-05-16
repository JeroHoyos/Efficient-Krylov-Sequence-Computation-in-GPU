#ifndef METRICAS_H
#define METRICAS_H

#include <stdint.h>

/* Una muestra por iteracion */
typedef struct {
    double tiempo_ms;           /* duracion de la multiplicacion */
    double gflops;              /* GFLOPs efectivos */
    double gbps_estimado;       /* GB/s algoritmicos estimados */
    long long flops;
    long long bytes_leidos;
    long long bytes_escritos;
    long long bytes_totales;
} Muestra;

/* Contenedor de todas las muestras */
typedef struct {
    Muestra *muestras;
    int      n;         /* numero de iteraciones registradas */
    int      cap;       /* capacidad actual del array */
} Metricas;

void   metricas_init(Metricas *m, int capacidad);
void   metricas_registrar(Metricas *m, double tiempo_ms,
                           long long flops,
                           long long bytes_leidos,
                           long long bytes_escritos);
void   metricas_imprimir(const Metricas *m);
void   metricas_guardar_csv(const Metricas *m, const char *outdir,
                             double benchmark_total_ms);
void   metricas_free(Metricas *m);

/* Helpers de tiempo portables */
double tiempo_actual_ms(void);   /* devuelve ms de un reloj monotonico */

#endif
