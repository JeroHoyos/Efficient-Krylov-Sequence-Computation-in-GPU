#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
  #include <windows.h>
#endif
#include "benchmark.h"
#include "matrices.h"
#include "metricas.h"
#include "perfil.h"
#include "parametros.h"

void ejecutar_iteracion(float **A, float **Z_actual, Parametros p, float ***out_Z_nueva, float ***out_snap) {

    // Multiplicar A por Z_actual y guardar la nueva matriz resultado
    *out_Z_nueva = multiplicar_matrices(A, p.m, p.m, Z_actual, p.n);

    // Guardar una copia del resultado como snapshot de la iteración
    *out_snap    = copiar_snapshot(*out_Z_nueva, p.n);
}

static double tiempo_actual(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

void ejecutar_bucle(float **A, float **Z, Parametros p, float ***resultado,
                    const char *outdir, Metricas *m) {
    float **Z_actual = Z;
    float **Z_nueva  = NULL;

    printf("\n=== Ejecucion ===\n");

    // Capturar fallos de página antes de comenzar el bucle
    long minflt_ini = leer_minflt();
    long majflt_ini = leer_majflt();

    // Registrar el tiempo de inicio del benchmark completo
    double t_total_inicio = tiempo_actual();

    for (int iter = 0; iter < p.l; iter++) {

        // Registrar CPU y tiempo de pared al inicio de la iteración
        long long cpu_antes = leer_cpu_us_proceso();
        double t0 = tiempo_actual();
        ejecutar_iteracion(A, Z_actual, p, &Z_nueva, &resultado[iter]);
        double wall = tiempo_actual() - t0;

        // Guardar el tiempo que tomó esta iteración
        m->tiempos_iter[iter] = wall;

        // Calcular CPU% como (tiempo_cpu / tiempo_pared) * 100
        long long cpu_despues = leer_cpu_us_proceso();
        double cpu_s = (double)(cpu_despues - cpu_antes) / 1e6;
        m->cpu_iter[iter] = wall > 0.0 ? (cpu_s / wall) * 100.0 : 0.0;

        // Registrar y actualizar RSS por iteración
        long rss = leer_rss_kb();
        m->rss_iter[iter] = rss;
        if (rss > m->pico_mem_kb) m->pico_mem_kb = rss;

        // Guardar el snapshot de esta iteración en disco
        guardar_resultado_txt(resultado[iter], p.n, p.n, iter, outdir);

        // Liberar la Z anterior si ya no es la matriz inicial
        if (Z_actual != Z) liberar_matriz(Z_actual, p.m);

        // Avanzar Z_actual al resultado de esta iteración
        Z_actual = Z_nueva;
    }

    // Registrar el tiempo total transcurrido
    m->tiempo_total        = tiempo_actual() - t_total_inicio;

    // Calcular los fallos de página ocurridos durante el bucle
    m->fallos_pagina_menor = leer_minflt() - minflt_ini;
    m->fallos_pagina_mayor = leer_majflt() - majflt_ini;

    // Calcular el tiempo promedio por iteración
    double suma = 0.0;
    for (int i = 0; i < p.l; i++) suma += m->tiempos_iter[i];
    m->tiempo_promedio = p.l > 0 ? suma / p.l : 0.0;

    // Liberar la última matriz Z generada
    liberar_matriz(Z_actual, p.m);
}

void benchmark(Parametros p, const char *outdir) {

    // Inicializar las matrices A y Z con valores aleatorios
    float **A, **Z;
    inicializar_matrices(p, &A, &Z);

    // Reservar arreglo para guardar los snapshots de cada iteración
    float ***resultado = malloc(p.l * sizeof(float **));

    // Inicializar la estructura de métricas
    Metricas m;
    metricas_init(&m, p.l);
    m.p           = p;

    // Registrar los tamaños en bytes de cada matriz
    m.tam_A_bytes = (long)p.m * p.m * sizeof(float);
    m.tam_Z_bytes = (long)p.m * p.n * sizeof(float);

    // Ejecutar el bucle principal del benchmark
    ejecutar_bucle(A, Z, p, resultado, outdir, &m);

    // Imprimir el informe de métricas en pantalla
    metricas_imprimir(&m, p.l);

    // Guardar el informe de métricas en el directorio de salida
    metricas_guardar(&m, p.l, outdir);

    // Liberar toda la memoria utilizada
    metricas_free(&m);
    liberar_resultado(resultado, p.l, p.n);
    liberar_matriz(A, p.m);
}
