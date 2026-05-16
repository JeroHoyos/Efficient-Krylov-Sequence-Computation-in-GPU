// ! benchmark.c - Funciones para ejecutar el benchmark de multiplicación de matrices.
#include <stdio.h>
#include <stdlib.h>
#include "benchmark.h"
#include "matrices.h"
#include "metricas.h"
#include "parametros.h"
#if defined(USE_CUDA)
    #include "block_mul_gpu.h"
#else
    #include "matmul_cpu.h"
#endif

#if !defined(USE_CUDA)

void ejecutar_bucle_cpu(float **A, float **Z, Parametros p, const char *outdir, Metricas *met) {

    // Iniciar el doble buffer Z_cur es entrada, Z_nxt es salida, se intercambian cada iteración.
    float **Z_alt = crear_matriz(p.m, p.n);
    float **Z_cur = Z;
    float **Z_nxt = Z_alt;

    // Flops, bytes leídos y bytes escritos teóricos de una multiplicación A×Z.
    long long flops       = 2LL * p.m * p.m * p.n;
    long long bytes_read  = 2LL * p.m * p.m * p.n * (long long)sizeof(float);
    long long bytes_write = 1LL * p.m * p.n * (long long)sizeof(float);

    printf("\n=== Ejecucion CPU (%d iter.) ===\n", p.l);

    for (int iter = 0; iter < p.l; iter++) {

        double t0 = tiempo_ms_ahora();
        matmul_cpu(A, p.m,p.n, Z_cur, Z_nxt);
        double dt = tiempo_ms_ahora() - t0;

        metricas_registrar(met, dt, flops, bytes_read, bytes_write);
        guardar_snapshot(Z_nxt, p.n, iter, outdir);

        // Intercambia punteros.
        float **tmp = Z_cur;
        Z_cur = Z_nxt;
        Z_nxt = tmp;

        printf("  iter %4d  %7.2f ms\n", iter, dt);
    }

    liberar_matriz(Z_alt, p.m);
}

#else  /* USE_CUDA */

// Itera p.l multiplicaciones en GPU. El swap de buffers ocurre dentro
// de gpu_multiplicar(); aquí solo se mide el tiempo y se baja el snapshot.
void ejecutar_bucle_gpu(GpuCtx *ctx, Parametros p, const char *outdir, Metricas *met) {


    // Flops, bytes leídos y bytes escritos teóricos de una multiplicación A×Z.
    long long flops       = 2LL * p.m * p.m * p.n;
    long long bytes_read  = 2LL * p.m * p.m * p.n * (long long)sizeof(float);
    long long bytes_write = 1LL * p.m * p.n * (long long)sizeof(float);

    printf("\n=== Ejecucion GPU (%d iter.) ===\n", p.l);

    for (int i = 0; i < p.l; i++) {
        double dt = gpu_multiplicar(ctx);

        metricas_registrar(met, dt, flops, bytes_read, bytes_write);
        gpu_copiar_snapshot(ctx, snap);
        guardar_snapshot(snap, p.n, i, outdir);

        printf("  iter %4d  %7.2f ms\n", i, dt);
    }

}

#endif /* USE_CUDA */

void benchmark(Parametros p, const char *matrices_dir, const char *outdir) {

    // Carga matrices A y Z desde los archivos de /data.
    float **A, **Z;
    if (cargar_matrices(matrices_dir, p.m, p.n, &A, &Z) != 0) {
        fprintf(stderr, "Error cargando matrices desde '%s'\n", matrices_dir);
        return;
    }

    Metricas met;
    metricas_init(&met, p.l);

    double t0 = tiempo_actual_ms();

    #if defined(USE_CUDA)
        GpuCtx ctx;
        if (gpu_ctx_init(&ctx, A, Z, p.m, p.n) != 0) {
            fprintf(stderr, "Error inicializando contexto GPU\n");
            liberar_matriz(A, p.m);
            liberar_matriz(Z, p.m);
            return;
        }
        // Libera host RAM antes del bucle — las matrices ya viven en VRAM.
        liberar_matriz(A, p.m);
        liberar_matriz(Z, p.m);
        ejecutar_bucle_gpu(&ctx, p, outdir, &met);
        gpu_ctx_free(&ctx);
    #else
        ejecutar_bucle_cpu(A, Z, p, outdir, &met);
        liberar_matriz(A, p.m);
        liberar_matriz(Z, p.m);
    #endif

    double total = tiempo_actual_ms() - t0;

    metricas_imprimir(&met);
    printf("  Tiempo total benchmark: %8.3f ms\n", total);
    metricas_guardar_csv(&met, outdir, total);
    metricas_free(&met);
}