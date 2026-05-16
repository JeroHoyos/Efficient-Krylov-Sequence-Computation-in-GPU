#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "parametros.h"
#include "metricas.h"

#if defined(USE_CUDA)
    #include "matmul_gpu.h"
    void ejecutar_bucle_gpu(GpuCtx *ctx, Parametros p, const char *outdir, Metricas *met);
#else
    #include "matmul_cpu.h"
    void ejecutar_bucle_cpu(float **A, float **Z, Parametros p, const char *outdir, Metricas *met);
#endif

void benchmark(Parametros p, const char *matrices_dir, const char *outdir);

#endif