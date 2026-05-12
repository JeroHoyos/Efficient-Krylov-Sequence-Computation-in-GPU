#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "metricas.h"

// clock() es la única opción portable en Windows sin WinAPI extra.
double tiempo_ms_ahora(void) {
    return (double)clock() / CLOCKS_PER_SEC * 1000.0;
}

void metricas_init(Metricas *m, int capacidad) {
    m->muestras = malloc(capacidad * sizeof(Muestra));
    m->n        = 0;
    m->cap      = capacidad;
}

// Si el buffer se llena, duplica la capacidad — mismo patrón que un vector dinámico.
void metricas_registrar(Metricas *m, double tiempo_ms, long long flops) {
    if (m->n >= m->cap) {
        m->cap     *= 2;
        m->muestras = realloc(m->muestras, m->cap * sizeof(Muestra));
    }
    Muestra *s   = &m->muestras[m->n++];
    s->tiempo_ms = tiempo_ms;
    // Guarda 0.0 si tiempo_ms es 0 para evitar división por cero.
    s->gflops    = (tiempo_ms > 0)
                   ? (double)flops / (tiempo_ms / 1000.0) / 1e9
                   : 0.0;
}

void metricas_free(Metricas *m) {
    free(m->muestras);
    m->muestras = NULL;
    m->n = m->cap = 0;
}

void metricas_imprimir(const Metricas *m) {
    if (m->n == 0) { printf("  (sin muestras)\n"); return; }

    double suma_t = 0, suma_g = 0, min_t = m->muestras[0].tiempo_ms;
    double max_t  = min_t;

    for (int i = 0; i < m->n; i++) {
        double t = m->muestras[i].tiempo_ms;
        suma_t  += t;
        suma_g  += m->muestras[i].gflops;
        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;
    }

    printf("\n=== Metricas (%d iter.) ===\n", m->n);
    printf("  Tiempo promedio : %8.3f ms\n", suma_t / m->n);
    printf("  Tiempo min      : %8.3f ms\n", min_t);
    printf("  Tiempo max      : %8.3f ms\n", max_t);
    printf("  GFLOPs promedio : %8.3f\n",    suma_g / m->n);
}

void metricas_guardar_csv(const Metricas *m, const char *outdir) {
    char ruta[256];
    snprintf(ruta, sizeof(ruta), "%s/metricas.csv", outdir);

    FILE *f = fopen(ruta, "w");
    if (!f) {
        fprintf(stderr, "Error: no se pudo crear %s\n", ruta);
        return;
    }

    fprintf(f, "iter,tiempo_ms,gflops\n");
    for (int i = 0; i < m->n; i++)
        fprintf(f, "%d,%.6f,%.6f\n",
                i, m->muestras[i].tiempo_ms, m->muestras[i].gflops);

    fclose(f);
    printf("  Metricas CSV: %s\n", ruta);
}