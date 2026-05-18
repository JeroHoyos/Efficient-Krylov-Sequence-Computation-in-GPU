// ! benchmark.c - Funciones para ejecutar el benchmark de multiplicación de matrices.
#include <stdio.h>
#include <stdlib.h>
#include "benchmark.h"
#include "matrices.h"
#include "metricas.h"
#include "parametros.h"
#if defined(USE_CUDA)
    #include "matmul_gpu.h"
#else
    #include "matmul_cpu.h"
#endif

#if !defined(USE_CUDA) // Usar CPU

void ejecutar_bucle_cpu(float **A, float **Z, Parametros p, const char *outdir, Metricas *met, CostoTeorico costo) {

    // Iniciar el doble buffer Z_cur es entrada, Z_nxt es salida, se intercambian cada iteración.
    float **Z_alt = crear_matriz(p.m, p.n);
    float **Z_cur = Z;
    float **Z_nxt = Z_alt;

    printf("\n=== Ejecucion CPU (%d iter.) ===\n", p.l);

    for (int iter = 0; iter < p.l; iter++) {

        double t0 = tiempo_actual_ms();
        matmul_cpu(A, p.m,p.n, Z_cur, Z_nxt);
        double dt = tiempo_actual_ms() - t0;

        metricas_registrar(met, dt, costo.flops, costo.bytes_read, costo.bytes_write);
        guardar_snapshot(Z_nxt, p.n, iter, outdir);

        // Imprime métricas de esta iteración.
        printf("  iter %4d  %8.3f ms  %8.3f GFLOPs  %8.3f GB/s\n",
                iter, met->muestras[met->n - 1].tiempo_ms,
                met->muestras[met->n - 1].gflops,
                met->muestras[met->n - 1].gbps_estimado);

        // Intercambia punteros.
        float **tmp = Z_cur;
        Z_cur = Z_nxt;
        Z_nxt = tmp;
    }

    liberar_matriz(Z_alt, p.m);
}

#else // Usar GPU


void ejecutar_bucle_gpu(GpuCtx *ctx, Parametros p, const char *outdir, Metricas *met, CostoTeorico costo) {

    float **snap = crear_matriz(p.n, p.n);

    printf("\n=== Ejecucion GPU (%d iter.) ===\n", p.l);

    for (int iter = 0; iter < p.l; iter++) {

        double t0 = tiempo_actual_ms();
        gpu_multiplicar(ctx);
        double dt = tiempo_actual_ms() - t0;
        
        metricas_registrar(met, dt, costo.flops, costo.bytes_read, costo.bytes_write);
        gpu_copiar_snapshot(ctx, snap);
        guardar_snapshot(snap, p.n, iter, outdir);

        // Imprime métricas de esta iteración.
        printf("  iter %4d  %8.3f ms  %8.3f GFLOPs  %8.3f GB/s\n",
                iter, met->muestras[met->n - 1].tiempo_ms,
                met->muestras[met->n - 1].gflops,
                met->muestras[met->n - 1].gbps_estimado);
    }

    liberar_matriz(snap, p.n);
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

    CostoTeorico costo = costo_teorico(p.m, p.n);

    double t0 = tiempo_actual_ms();

    #if defined(USE_CUDA)

        // Inicializa contexto GPU, reserva memoria y copia A y Z a VRAM según el modo.
        GpuCtx ctx;
        if (gpu_ctx_init(&ctx, A, Z, p.m, p.n) != 0) {
            fprintf(stderr, "Error inicializando contexto GPU\n");
            liberar_matriz(A, p.m);
            liberar_matriz(Z, p.m);
            return;
        }

        // Z ya está en VRAM, se puede liberar siempre tras init.
        liberar_matriz(Z, p.m);

        // Revisa el modo de ejecución para decidir cuándo liberar A de RAM.
        GpuExecMode mode = ctx.mode;
        if (mode == GPU_EXEC_FULL) {
            liberar_matriz(A, p.m);
        }

        ejecutar_bucle_gpu(&ctx, p, outdir, &met, costo);
        gpu_ctx_free(&ctx);

        if (mode == GPU_EXEC_SLAB) {
            liberar_matriz(A, p.m);
        }
    #else
        ejecutar_bucle_cpu(A, Z, p, outdir, &met, costo);
        liberar_matriz(A, p.m);
        liberar_matriz(Z, p.m);
    #endif

    double total = tiempo_actual_ms() - t0;

    metricas_imprimir(&met);
    printf("  Tiempo total benchmark: %8.3f ms\n", total);
    metricas_guardar_csv(&met, outdir, total);
    metricas_free(&met);
}
