#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
  #include <windows.h>
#endif
#include "benchmark.h"
#include "block_mul.h"
#include "matrices.h"
#include "metricas.h"
#include "perfil.h"
#include "parametros.h"

/* Retorna el tiempo de pared actual en segundos con alta resolución. */
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

/* Ejecuta p.l iteraciones usando dos buffers Z alternados y un buffer de snapshot
   reutilizables, evitando allocs dentro del bucle caliente. */
void ejecutar_bucle(float **A, float **Z, Parametros p,
                    const char *outdir, Metricas *m) {
    /* Dos buffers Z para alternar: Z es el buffer inicial (cargado desde disco),
       Z_alt es el segundo buffer pre-asignado. El swap de punteros evita copias. */
    float **Z_alt = crear_matriz(p.m, p.n);
    float **snap  = crear_matriz(p.n, p.n);

    float **Z_cur = Z;
    float **Z_nxt = Z_alt;

    printf("\n=== Ejecucion ===\n");

    long minflt_ini = leer_minflt();
    long majflt_ini = leer_majflt();
    double t_total_inicio = tiempo_actual();

    for (int iter = 0; iter < p.l; iter++) {
        /* Capturar wall y CPU en el mismo orden en ambos extremos para que
           las dos ventanas de medición sean simétricas alrededor del trabajo. */
        double t0 = tiempo_actual();
        long long cpu_antes = leer_cpu_us_proceso();

        multiplicar_en(A, p.m, p.m, Z_cur, p.n, Z_nxt);

        long long cpu_despues = leer_cpu_us_proceso();
        double wall = tiempo_actual() - t0;

        m->tiempos_iter[iter] = wall;

        double cpu_s = (double)(cpu_despues - cpu_antes) / 1e6;
        double pct = wall > 0.0 ? (cpu_s / wall) * 100.0 : 0.0;
        m->cpu_iter[iter] = pct > 100.0 ? 100.0 : pct;

        long rss = leer_rss_kb();
        m->rss_iter[iter] = rss;
        if (rss > m->pico_mem_kb) m->pico_mem_kb = rss;

        copiar_snapshot_en(Z_nxt, p.n, snap);
        guardar_resultado_txt(snap, p.n, p.n, iter, outdir);

        /* Rotar buffers: el resultado pasa a ser la nueva entrada. */
        float **tmp = Z_cur; Z_cur = Z_nxt; Z_nxt = tmp;
    }

    m->tiempo_total        = tiempo_actual() - t_total_inicio;
    m->fallos_pagina_menor = leer_minflt() - minflt_ini;
    m->fallos_pagina_mayor = leer_majflt() - majflt_ini;

    double suma = 0.0;
    for (int i = 0; i < p.l; i++) suma += m->tiempos_iter[i];
    m->tiempo_promedio = p.l > 0 ? suma / p.l : 0.0;

    liberar_matriz(Z_alt, p.m);
    liberar_matriz(snap, p.n);
}

/* Carga matrices desde matrices_dir, ejecuta el benchmark, imprime y guarda métricas, y libera memoria. */
void benchmark(Parametros p, const char *matrices_dir, const char *outdir) {
    double ram_pct = RAM_PCT / 100.0;
    long long ram_kb = leer_ram_disponible_kb();

    printf("\n=== Carga de matrices ===\n");
    print_tamano("A", p.m, p.m);
    print_tamano("Z", p.m, p.n);

    Metricas m;
    metricas_init(&m, p.l);
    m.p           = p;
    m.tam_A_bytes = (long)p.m * p.m * sizeof(float);
    m.tam_Z_bytes = (long)p.m * p.n * sizeof(float);

    if (matrices_caben_en_ram(p.m, p.n, ram_kb, ram_pct)) {
        float **A, **Z;
        if (cargar_matrices(matrices_dir, p.m, p.n, &A, &Z) != 0) {
            fprintf(stderr, "Error: no se pudieron cargar las matrices desde '%s'\n", matrices_dir);
            metricas_free(&m);
            return;
        }
        ejecutar_bucle(A, Z, p, outdir, &m);
        liberar_matriz(A, p.m);
        liberar_matriz(Z, p.m);
    } else {
        int BS = calcular_block_size(ram_kb, ram_pct);
        printf("  RAM insuficiente. Modo bloques (BS=%d, RAM_PCT=%d%%).\n", BS, RAM_PCT);
        ejecutar_bucle_bloques(matrices_dir, p, outdir, &m, BS);
    }

    metricas_imprimir(&m, p.l);
    metricas_guardar(&m, p.l, outdir);
    metricas_free(&m);
}
