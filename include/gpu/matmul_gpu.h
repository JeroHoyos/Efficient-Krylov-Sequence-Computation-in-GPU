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

// Contexto GPU.
// FULL: A, Z_cur y Z_nxt viven completos en VRAM.
// SLAB: Z_cur y Z_nxt viven completos en VRAM; A vive en host pinned y
//       se procesa por slabs horizontales con doble buffer en VRAM.
typedef struct {
    float *d_A;      // A [m x m] en VRAM, solo en modo FULL.
    float *d_Z_cur;  // Z activo [m x n] en VRAM.
    float *d_Z_nxt;  // Z proximo [m x n] en VRAM.
    float *d_snap;   // snapshot [n x n] en VRAM.

    float *h_A_pinned;      // A [m x m] en host pinned, solo modo SLAB.
    float *h_Z_out_pinned;  // Z_out [m x n] en host pinned, solo modo SLAB.
    float *d_A_slab[2];     // Doble buffer de slabs de A en VRAM.
    cudaStream_t streams[2];

    GpuExecMode mode;
    int m, n;
    int slab_rows;
    size_t bytes_Z;
} GpuCtx;

// Reserva memoria y copia A y Z desde host.
// Retorna 0 en exito, -1 en error CUDA.
int gpu_ctx_init(GpuCtx *ctx, float **A, float **Z, int m, int n);

// Ejecuta A x Z_cur -> Z_nxt en GPU y luego intercambia los punteros internos.
// Retorna el tiempo medido con eventos CUDA en ms.
double gpu_multiplicar(GpuCtx *ctx);

// Copia el snapshot (primeras n filas de Z_cur) al host.
void gpu_copiar_snapshot(const GpuCtx *ctx, float **snap_host);

// Libera todos los recursos del contexto.
void gpu_ctx_free(GpuCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif
