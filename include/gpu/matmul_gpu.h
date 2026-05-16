#ifndef BLOCK_MUL_GPU_H
#define BLOCK_MUL_GPU_H

#include "parametros.h"

#ifdef __cplusplus
extern "C" {
#endif


// Contexto GPU: A y Z viven en VRAM durante todo el bucle.
// Se crean una sola vez antes del bucle y se liberan al final.

typedef struct {
    float *d_A;      // A  [m x m] en VRAM        
    float *d_Z_cur;  // Z  activo [m x n] en VRAM 
    float *d_Z_nxt;  // Z  próximo [m x n] en VRAM
    float *d_snap;   // snapshot [n x n] en VRAM  
    int m, n;
} GpuCtx;

// Reserva memoria en GPU y copia A y Z desde host.
// Retorna 0 en éxito, -1 en error CUDA.
int  gpu_ctx_init(GpuCtx *ctx, float **A, float **Z,
                  int m, int n);

// Ejecuta la multiplicación A x Z_cur → Z_nxt en GPU,
// luego intercambia los punteros internamente.
// Retorna tiempo de ejecución del kernel en ms.
double gpu_multiplicar(GpuCtx *ctx);

// Copia el snapshot (primeras n filas de Z_cur) al host.
void gpu_copiar_snapshot(const GpuCtx *ctx, float **snap_host);

// Libera toda la VRAM del contexto.
void gpu_ctx_free(GpuCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif
