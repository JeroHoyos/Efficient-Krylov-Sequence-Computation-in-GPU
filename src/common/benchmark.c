// Bucle principal del benchmark: carga matrices, itera multiplicaciones
#include <stdio.h>
#include <stdlib.h>
#include "benchmark.h"
#include "matrices.h"
#include "metricas.h"
#include "perfil.h"
#include "parametros.h"

#if defined(USE_CUDA)
  #include "block_mul_gpu.h"
#else
  #include "block_mul.h"
#endif


long long flops_mul(int m, int n) {
    return 2LL * m * m * n;
}

// Modo GPU
#if !defined(USE_CUDA)

// Itera p.l multiplicaciones A×Z en CPU con doble buffer para evitar
// copiar Z entre iteraciones — Z_cur y Z_nxt se intercambian en cada paso.
void ejecutar_bucle_cpu(float **A, float **Z,Parametros p, const char *outdir,Metricas *met) {
    
    float **Z_alt = crear_matriz(p.m, p.n);
    float **snap  = crear_matriz(p.n, p.n);
    float **Z_cur = Z;
    float **Z_nxt = Z_alt;

    long long flops = flops_mul(p.m, p.n);

    printf("\n=== Ejecucion CPU (%d iter.) ===\n", p.l);

    for (int i = 0; i < p.l; i++) {
        double t0 = tiempo_ms_ahora();
        multiplicar_en(A, p.m, p.m, Z_cur, p.n, Z_nxt);
        double dt = tiempo_ms_ahora() - t0;

        metricas_registrar(met, dt, flops);
        copiar_snapshot_en(Z_nxt, p.n, snap);
        guardar_resultado_txt(snap, p.n, p.n, i, outdir);

        // Intercambia punteros en lugar de copiar datos — O(1) por iteración.
        float **tmp = Z_cur; Z_cur = Z_nxt; Z_nxt = tmp;

        printf("  iter %4d  %7.2f ms\n", i, dt);
    }

    liberar_matriz(Z_alt, p.m);
    liberar_matriz(snap,  p.n);
}

void benchmark(Parametros p, const char *matrices_dir,const char *outdir) {

    printf("\n=== Carga de matrices ===\n");
    print_tamano("A", p.m, p.m);
    print_tamano("Z", p.m, p.n);
    print_memoria(p.m, p.n);

    float **A, **Z;
    if (cargar_matrices(matrices_dir, p.m, p.n, &A, &Z) != 0) {
        fprintf(stderr, "Error cargando matrices desde '%s'\n", matrices_dir);
        return;
    }

    Metricas met;
    metricas_init(&met, p.l);

    ejecutar_bucle_cpu(A, Z, p, outdir, &met);

    metricas_imprimir(&met);
    metricas_guardar_csv(&met, outdir);

    liberar_matriz(A, p.m);
    liberar_matriz(Z, p.m);
    metricas_free(&met);
}

// Modo GPU
#else  /* USE_CUDA */

// Itera p.l multiplicaciones en GPU. El swap de buffers ocurre dentro
// de gpu_multiplicar(); aquí solo se mide el tiempo y se baja el snapshot.
void ejecutar_bucle_gpu(GpuCtx *ctx,Parametros p, const char *outdir,Metricas *met) {

    // El snapshot vive en host: solo necesitamos n×n floats, no m×m.
    float **snap_host = crear_matriz(p.n, p.n);
    long long flops   = flops_mul(p.m, p.n);

    printf("\n=== Ejecucion GPU (%d iter.) ===\n", p.l);

    for (int i = 0; i < p.l; i++) {
        double dt = gpu_multiplicar(ctx);

        metricas_registrar(met, dt, flops);
        gpu_copiar_snapshot(ctx, snap_host);
        guardar_resultado_txt(snap_host, p.n, p.n, i, outdir);

        printf("  iter %4d  %7.2f ms\n", i, dt);
    }

    liberar_matriz(snap_host, p.n);
}

void benchmark(Parametros p, const char *matrices_dir,const char *outdir) {

    printf("\n=== Carga de matrices (host) ===\n");
    print_tamano("A", p.m, p.m);
    print_tamano("Z", p.m, p.n);
    print_memoria(p.m, p.n);

    float **A, **Z;
    if (cargar_matrices(matrices_dir, p.m, p.n, &A, &Z) != 0) {
        fprintf(stderr, "Error cargando matrices desde '%s'\n", matrices_dir);
        return;
    }

    // Sube A y Z a VRAM; desde aquí el host no los necesita.
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

    Metricas met;
    metricas_init(&met, p.l);

    ejecutar_bucle_gpu(&ctx, p, outdir, &met);

    metricas_imprimir(&met);
    metricas_guardar_csv(&met, outdir);

    gpu_ctx_free(&ctx);
    metricas_free(&met);
}

#endif /* USE_CUDA */