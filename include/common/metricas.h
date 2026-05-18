#ifndef METRICAS_H
#define METRICAS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double    tiempo_ms;
    double    gflops;
    double    gbps_estimado;
    long long flops;
    long long bytes_leidos;
    long long bytes_escritos;
    long long bytes_totales;
} Muestra;

typedef struct {
    Muestra *muestras;
    int      n;
    int      cap;
} Metricas;

void   metricas_init(Metricas *m, int capacidad);
void   metricas_registrar(Metricas *m, double tiempo_ms, long long flops, long long bytes_leidos, long long bytes_escritos);
void   metricas_imprimir(const Metricas *m);
void   metricas_guardar_csv(const Metricas *m, const char *outdir, double benchmark_total_ms);
void   metricas_free(Metricas *m);
double tiempo_actual_ms(void);
void   imprimir_metrica_iter(int iter, const Muestra *s);

#ifdef __cplusplus
}
#endif

#endif