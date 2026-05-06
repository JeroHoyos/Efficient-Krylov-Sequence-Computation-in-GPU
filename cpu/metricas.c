#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
  #include <windows.h>
  #include <psapi.h>
#endif
#include "metricas.h"

void metricas_init(Metricas *m, int l) {

    // Inicializar campos de tiempo en cero
    m->tiempo_total         = 0.0;
    m->tiempo_promedio      = 0.0;

    // Reservar arreglos por iteración
    m->tiempos_iter         = calloc(l, sizeof(double));
    m->rss_iter             = calloc(l, sizeof(long));
    m->cpu_iter             = calloc(l, sizeof(double));

    // Inicializar campos de memoria en cero
    m->pico_mem_kb          = 0;
    m->tam_A_bytes          = 0;
    m->tam_Z_bytes          = 0;

    // Inicializar contadores de fallos de página
    m->fallos_pagina_menor  = 0;
    m->fallos_pagina_mayor  = 0;
}

void metricas_free(Metricas *m) {

    free(m->tiempos_iter);
    free(m->rss_iter);
    free(m->cpu_iter);
}

long leer_rss_kb(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (long)(pmc.WorkingSetSize / 1024);
    return -1;
#else
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;

    char line[256];
    long rss = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, " %ld", &rss);
            break;
        }
    }
    fclose(f);
    return rss;
#endif
}

static void fmt_bytes(char *buf, size_t bufsz, long bytes) {

    // Formatear en GB si supera 1 GB
    if      (bytes >= 1024L * 1024 * 1024) snprintf(buf, bufsz, "%ld bytes (%.2f GB)", bytes, bytes / (1024.0 * 1024.0 * 1024.0));

    // Formatear en MB si supera 1 MB
    else if (bytes >= 1024L * 1024)        snprintf(buf, bufsz, "%ld bytes (%.2f MB)", bytes, bytes / (1024.0 * 1024.0));

    // Formatear en KB si supera 1 KB
    else if (bytes >= 1024L)               snprintf(buf, bufsz, "%ld bytes (%.2f KB)", bytes, bytes / 1024.0);

    // Formatear en bytes si es menor a 1 KB
    else                                   snprintf(buf, bufsz, "%ld bytes", bytes);
}

static void escribir_metricas(FILE *out, const Metricas *m, int l) {
    char buf[64];

    // Buscar el tiempo mínimo y máximo entre todas las iteraciones
    double t_min = m->tiempos_iter[0], t_max = m->tiempos_iter[0];
    int    idx_min = 0, idx_max = 0;
    for (int i = 0; i < l; i++) {
        if (m->tiempos_iter[i] < t_min) { t_min = m->tiempos_iter[i]; idx_min = i; }
        if (m->tiempos_iter[i] > t_max) { t_max = m->tiempos_iter[i]; idx_max = i; }
    }

    // Calcular la varianza de los tiempos por iteración
    double var = 0.0;
    for (int i = 0; i < l; i++) {
        double d = m->tiempos_iter[i] - m->tiempo_promedio;
        var += d * d;
    }

    // Calcular la desviación estándar muestral
    double desv = l > 1 ? sqrt(var / (l - 1)) : 0.0;

    // Imprimir encabezado del informe
    fprintf(out, "\n================================================\n");
    fprintf(out, "            INFORME DE METRICAS\n");
    fprintf(out, "================================================\n");

    // Imprimir parámetros de configuración
    fprintf(out, "\n  [CONFIGURACION]\n");
    fprintf(out, "  Exponente (input)  : %d\n",       m->p.input);
    fprintf(out, "  Dimension A        : %d x %d\n",  m->p.m, m->p.m);
    fprintf(out, "  Columnas Z (n)     : %d\n",       m->p.n);
    fprintf(out, "  Iteraciones (l)    : %d\n",       m->p.l);

    // Imprimir métricas de uso de memoria
    fprintf(out, "\n  [MEMORIA]\n");
    fmt_bytes(buf, sizeof(buf), m->tam_A_bytes);
    fprintf(out, "  Matriz A           : %s\n", buf);
    fmt_bytes(buf, sizeof(buf), m->tam_Z_bytes);
    fprintf(out, "  Matriz Z           : %s\n", buf);
    fprintf(out, "  Pico RSS           : %ld KB (%.2f MB)\n",
            m->pico_mem_kb, m->pico_mem_kb / 1024.0);
    fprintf(out, "  Fallos pag. menor  : %ld\n", m->fallos_pagina_menor);
    fprintf(out, "  Fallos pag. mayor  : %ld\n", m->fallos_pagina_mayor);

    // Imprimir métricas de tiempo con estadísticas
    fprintf(out, "\n  [TIEMPOS]\n");
    fprintf(out, "  Total              : %.6f s\n", m->tiempo_total);
    fprintf(out, "  Promedio / iter    : %.6f s\n", m->tiempo_promedio);
    if (l > 0) {
        fprintf(out, "  Minimo             : %.6f s  (iter %2d)\n", t_min, idx_min);
        fprintf(out, "  Maximo             : %.6f s  (iter %2d)\n", t_max, idx_max);
        fprintf(out, "  Desv. estandar     : %.6f s\n", desv);
    }

    // Imprimir tabla por iteración: tiempo, CPU% y memoria RSS
    fprintf(out, "\n  [DETALLE POR ITERACION]\n");
    fprintf(out, "  %-8s  %-14s  %-10s  %-12s\n",
            "Iter", "Tiempo (s)", "CPU (%)", "Mem RSS (MB)");
    fprintf(out, "  %-8s  %-14s  %-10s  %-12s\n",
            "--------", "--------------", "----------", "------------");
    for (int i = 0; i < l; i++)
        fprintf(out, "  %-8d  %-14.6f  %-10.2f  %-12.2f\n",
                i, m->tiempos_iter[i], m->cpu_iter[i], m->rss_iter[i] / 1024.0);

    fprintf(out, "\n================================================\n");
}

void metricas_imprimir(const Metricas *m, int l) {

    // Escribir el informe de métricas en la salida estándar
    escribir_metricas(stdout, m, l);
}

void metricas_guardar(const Metricas *m, int l, const char *outdir) {

    // Construir la ruta del archivo de métricas
    char path[512];
    snprintf(path, sizeof(path), "%s/metricas.txt", outdir);

    // Abrir el archivo de salida para escritura
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Error: no se pudo crear %s\n", path); return; }

    // Escribir el informe en el archivo
    escribir_metricas(f, m, l);

    // Cerrar el archivo e informar la ruta guardada
    fclose(f);
    printf("  Metricas guardadas: %s\n", path);
}
