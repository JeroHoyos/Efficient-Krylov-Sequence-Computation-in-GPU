#include <stdio.h>
#include <stdlib.h>
#include "metricas.h"

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <time.h>
#endif

// Temporizador monotonico de pared: mide tiempo real transcurrido.
double tiempo_ms_ahora(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    static int initialized = 0;
    LARGE_INTEGER now;

    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }

    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec * 1000.0 + (double)now.tv_nsec / 1000000.0;
#endif
}

void metricas_init(Metricas *m, int capacidad) {
    m->muestras = malloc(capacidad * sizeof(Muestra));
    m->n        = 0;
    m->cap      = capacidad;
}

// Si el buffer se llena, duplica la capacidad: mismo patron que un vector dinamico.
void metricas_registrar(Metricas *m, double tiempo_ms, long long flops,
                        long long bytes_leidos, long long bytes_escritos) {
    if (m->n >= m->cap) {
        m->cap     *= 2;
        m->muestras = realloc(m->muestras, m->cap * sizeof(Muestra));
    }

    Muestra *s = &m->muestras[m->n++];
    s->tiempo_ms = tiempo_ms;
    s->flops = flops;
    s->bytes_leidos = bytes_leidos;
    s->bytes_escritos = bytes_escritos;
    s->bytes_totales = bytes_leidos + bytes_escritos;

    // Guarda 0.0 si tiempo_ms es 0 para evitar division por cero.
    s->gflops = (tiempo_ms > 0)
                ? (double)flops / (tiempo_ms / 1000.0) / 1e9
                : 0.0;
    s->gbps_estimado = (tiempo_ms > 0)
                       ? (double)s->bytes_totales / (tiempo_ms / 1000.0) / 1e9
                       : 0.0;
}

void metricas_free(Metricas *m) {
    free(m->muestras);
    m->muestras = NULL;
    m->n = m->cap = 0;
}

void metricas_imprimir(const Metricas *m) {
    if (m->n == 0) { printf("  (sin muestras)\n"); return; }

    double suma_t = 0, suma_g = 0, suma_b = 0;
    double min_t = m->muestras[0].tiempo_ms;
    double max_t = min_t;

    for (int i = 0; i < m->n; i++) {
        double t = m->muestras[i].tiempo_ms;
        suma_t += t;
        suma_g += m->muestras[i].gflops;
        suma_b += m->muestras[i].gbps_estimado;
        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;
    }

    printf("\n=== Metricas (%d iter.) ===\n", m->n);
    printf("  Tiempo promedio : %8.3f ms\n", suma_t / m->n);
    printf("  Tiempo min      : %8.3f ms\n", min_t);
    printf("  Tiempo max      : %8.3f ms\n", max_t);
    printf("  GFLOPs promedio : %8.3f\n",    suma_g / m->n);
    printf("  GB/s estimado   : %8.3f\n",    suma_b / m->n);
}

void metricas_guardar_csv(const Metricas *m, const char *outdir,
                          double benchmark_total_ms) {
    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/metricas.csv", outdir);

    FILE *f = fopen(ruta, "w");
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear %s\n", ruta);
        return;
    }

    fprintf(f, "tipo,iter,tiempo_ms,gflops,gbps_estimado\n");
    if (m->n == 0) {
        fprintf(f, "total,,%.6f,,\n", benchmark_total_ms);
        fclose(f);
        printf("  Metricas CSV: %s\n", ruta);
        return;
    }

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
        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;
        if (g < min_g) min_g = g;
        if (g > max_g) max_g = g;
        if (b < min_b) min_b = b;
        if (b > max_b) max_b = b;
    }

    for (int i = 0; i < m->n; i++)
        fprintf(f, "iteracion,%d,%.6f,%.6f,%.6f\n",
                i, m->muestras[i].tiempo_ms, m->muestras[i].gflops,
                m->muestras[i].gbps_estimado);

    fprintf(f, "promedio,,%.6f,%.6f,%.6f\n",
            suma_t / m->n, suma_g / m->n, suma_b / m->n);
    fprintf(f, "min,,%.6f,%.6f,%.6f\n", min_t, min_g, min_b);
    fprintf(f, "max,,%.6f,%.6f,%.6f\n", max_t, max_g, max_b);
    fprintf(f, "total,,%.6f,,\n", benchmark_total_ms);

    fclose(f);
    printf("  Metricas CSV: %s\n", ruta);
}
