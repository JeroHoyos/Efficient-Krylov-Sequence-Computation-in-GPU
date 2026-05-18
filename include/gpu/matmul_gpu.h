#ifndef MATMUL_GPU_H
#define MATMUL_GPU_H

#include <stddef.h>
#include <cuda_runtime.h>

#include "parametros.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPU_EXEC_FULL = 0,
    GPU_EXEC_SLAB = 1
} GpuExecMode;

typedef struct {
    float *d_A;    
    float *d_Z_cur;  
    float *d_Z_nxt; 
    float *d_snap;   

    float **h_A_2d;      
    float *h_A_stage[2];  
    float *h_Z_out_pinned; 
    float *d_A_slab[2];     
    cudaStream_t streams[2];

    GpuExecMode mode;
    int m, n;
    int slab_rows;
    size_t bytes_Z;
} GpuCtx;

int gpu_ctx_init(GpuCtx *ctx, float **A, float **Z, int m, int n);

double gpu_multiplicar(GpuCtx *ctx);

void gpu_copiar_snapshot(const GpuCtx *ctx, float **snap_host);

void gpu_ctx_free(GpuCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif
