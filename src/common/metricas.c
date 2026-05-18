//! metricas.c - Metricas de rendimiento para el benchmark, incluyendo tiempo, GFLOPs y GB/s estimados.
//!
//! Este modulo define las estructuras y funciones para registrar, imprimir y guardar métricas de rendimiento durante el benchmark de multiplicación de matrices.
//! Las métricas incluyen el tiempo de ejecución en milisegundos, el rendimiento en GFLOPs y una estimación de GB/s basada en los bytes leídos y escritos.
//! El tiempo se mide usando QueryPerformanceCounter de Windows para alta resolución.
//!
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "metricas.h"

/// Toma el tiempo actual en milisegundos usando QueryPerformanceCounter de Windows.
/// 
/// Argumentos:
///   - Ninguno.
/// Retorna:
///   - El tiempo actual en milisegundos como un double.
///
double tiempo_actual_ms(void) {
    // ticks por segundo, se consulta una sola vez.
    static LARGE_INTEGER freq;    

    static int initialized = 0;
    LARGE_INTEGER now;

    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }

    // ticks desde que arrancó el sistema.
    QueryPerformanceCounter(&now); 
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}

/// Inicializa el contenedor de métricas con una capacidad inicial dada.
/// 
/// Argumentos:
///   - m: puntero a la estructura Metricas a inicializar.
///   - capacidad: número inicial de muestras que puede contener sin realloc.
///
void metricas_init(Metricas *m, int capacidad) {
    m->muestras = malloc(capacidad * sizeof(Muestra));
    m->n        = 0;
    m->cap      = capacidad;
}

/// Registra una nueva muestra de métricas en el contenedor, calculando GFLOPs y GB/s estimados.
/// 
/// Si el buffer de muestras se llena, se duplica su capacidad automáticamente.
///
/// Argumentos:
///   - m: puntero a la estructura Metricas donde se registrará la muestra.
///   - tiempo_ms: duración de la operación en milisegundos.
///   - flops: número total de operaciones de punto flotante realizadas.
///   - bytes_leidos: número total de bytes leídos durante la operación.
///   - bytes_escritos: número total de bytes escritos durante la operación.
///
void metricas_registrar(Metricas *m, double tiempo_ms, long long flops, long long bytes_leidos, long long bytes_escritos) {

    // Si el buffer se llena, duplica la capacidad.
    if (m->n >= m->cap) {
        m->cap *= 2;
        m->muestras = realloc(m->muestras, m->cap * sizeof(Muestra));
    }

    // Rellena la muestra actual y avanza el contador.
    Muestra *s        = &m->muestras[m->n++];
    s->tiempo_ms      = tiempo_ms;
    s->flops          = flops;
    s->bytes_leidos   = bytes_leidos;
    s->bytes_escritos = bytes_escritos;
    s->bytes_totales  = bytes_leidos + bytes_escritos;

    // Rendimiento teórico — 0.0 si tiempo es 0 para evitar división por cero.
    double seg = tiempo_ms / 1000.0;
    if (seg > 0) {
        s->gflops        = (double)flops             / seg / 1e9;
        s->gbps_estimado = (double)s->bytes_totales  / seg / 1e9;
    } else {
        s->gflops        = 0.0;
        s->gbps_estimado = 0.0;
    }
}

/// Libera la memoria usada por el contenedor de métricas y resetea sus campos.
///
/// Argumentos:
///   - m: puntero a la estructura Metricas a liberar.
///
void metricas_free(Metricas *m) {
    free(m->muestras);
    m->muestras = NULL;
    m->n = m->cap = 0;
}

/// Imprime un resumen de las métricas registradas, incluyendo tiempo promedio, mínimo, máximo, GFLOPs promedio y GB/s estimado promedio.
///
/// Argumentos:
///   - m: puntero a la estructura Metricas que contiene las muestras a imprimir.
///
void metricas_imprimir(const Metricas *m) {

    if (m->n == 0) { 
        printf("  (sin muestras)\n"); 
        return; 
    }

    double suma_t = 0, suma_g = 0, suma_b = 0;
    double min_t = m->muestras[0].tiempo_ms;
    double max_t = min_t;

    for (int i = 0; i < m->n; i++) {

        double t = m->muestras[i].tiempo_ms;
        suma_t += t;
        suma_g += m->muestras[i].gflops;
        suma_b += m->muestras[i].gbps_estimado;

        if (t < min_t) {
            min_t = t;
        }
        if (t > max_t) {
            max_t = t;
        }
    }

    printf("\n=== Metricas (%d iter.) ===\n", m->n);
    printf("  Tiempo promedio : %8.3f ms\n", suma_t / m->n);
    printf("  Tiempo min      : %8.3f ms\n", min_t);
    printf("  Tiempo max      : %8.3f ms\n", max_t);
    printf("  GFLOPs promedio : %8.3f\n",    suma_g / m->n);
    printf("  GB/s estimado   : %8.3f\n",    suma_b / m->n);
}

/// Guarda las métricas registradas en un archivo CSV.
///
/// Contiene un resumen con promedio, mínimo, máximo y el tiempo total del benchmark.
///
/// Argumentos:
///   - m: puntero a la estructura Metricas que contiene las muestras a guardar.
///   - outdir: ruta del directorio donde se guardará el archivo CSV.
///   - benchmark_total_ms: tiempo total del benchmark en milisegundos, que se
///      
void metricas_guardar_csv(const Metricas *m, const char *outdir, double benchmark_total_ms) {

    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/metricas.csv", outdir);

    FILE *f = fopen(ruta, "w");
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear %s\n", ruta);
        return;
    }

    fprintf(f, "tipo,iter,tiempo_ms,gflops,gbps_estimado\n");

    // Sin muestras, solo guarda el tiempo total.
    if (m->n == 0) {
        fprintf(f, "total,,%.6f,,\n", benchmark_total_ms);
        fclose(f);
        printf("  Metricas CSV: %s\n", ruta);
        return;
    }

    // Escribe una fila por iteración.
    for (int i = 0; i < m->n; i++) {
        fprintf(f, "iteracion,%d,%.6f,%.6f,%.6f\n",
                i, m->muestras[i].tiempo_ms,
                m->muestras[i].gflops,
                m->muestras[i].gbps_estimado);
    }

    // Calcula estadísticas sobre todas las muestras.
    double suma_t = 0.0, suma_g = 0.0, suma_b = 0.0;
    double min_t = m->muestras[0].tiempo_ms;
    double max_t = min_t;
    double min_g = m->muestras[0].gflops;
    double max_g = min_g;
    double min_b = m->muestras[0].gbps_estimado;
    double max_b = min_b;

    for (int i = 0; i < m->n; i++) {
        double t = m->muestras[i].tiempo_ms;
        double g = m->muestras[i].gflops;
        double b = m->muestras[i].gbps_estimado;

        suma_t += t;
        suma_g += g;
        suma_b += b;

        if (t < min_t) { min_t = t; }
        if (t > max_t) { max_t = t; }
        if (g < min_g) { min_g = g; }
        if (g > max_g) { max_g = g; }
        if (b < min_b) { min_b = b; }
        if (b > max_b) { max_b = b; }
    }

    // Escribe resumen estadístico al final del CSV.
    fprintf(f, "promedio,,%.6f,%.6f,%.6f\n", suma_t / m->n, suma_g / m->n, suma_b / m->n);
    fprintf(f, "min,,%.6f,%.6f,%.6f\n", min_t, min_g, min_b);
    fprintf(f, "max,,%.6f,%.6f,%.6f\n", max_t, max_g, max_b);
    fprintf(f, "total,,%.6f,,\n", benchmark_total_ms);

    fclose(f);
    printf("  Metricas CSV: %s\n", ruta);
}
