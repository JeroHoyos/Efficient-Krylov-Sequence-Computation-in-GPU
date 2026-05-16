#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block_mul_gpu.h"

#define BLOCK_SIZE 32

__global__ void coalesced_mat_mul_kernel(const float *__restrict__ d_A,
                                         const float *__restrict__ d_Z_in,
                                         float *__restrict__ d_Z_out,
                                         int rows, int cols, int inner)
{
    const int col = blockDim.x * blockIdx.x + threadIdx.x;
    const int row = blockDim.y * blockIdx.y + threadIdx.y;

    if (row < rows && col < cols) {
        float value = 0.0f;
        for (int k = 0; k < inner; k++) {
            value += d_A[row * inner + k] * d_Z_in[k * cols + col];
        }
        d_Z_out[row * cols + col] = value;
    }
}

static int cuda_check(cudaError_t err, const char *msg)
{
    if (err != cudaSuccess) {
        fprintf(stderr, "%s: %s\n", msg, cudaGetErrorString(err));
        return -1;
    }
    return 0;
}

static int copy_matrix_to_device(float *dst, float **src, int rows, int cols)
{
    for (int i = 0; i < rows; i++) {
        cudaError_t err = cudaMemcpy(dst + (size_t)i * cols, src[i],
                                     (size_t)cols * sizeof(float),
                                     cudaMemcpyHostToDevice);
        if (cuda_check(err, "Error copiando matriz a GPU") != 0) {
            return -1;
        }
    }
    return 0;
}

int gpu_ctx_init(GpuCtx *ctx, float **A, float **Z, int m, int n)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->m = m;
    ctx->n = n;

    const size_t bytes_A = (size_t)m * m * sizeof(float);
    const size_t bytes_Z = (size_t)m * n * sizeof(float);
    const size_t bytes_snap = (size_t)n * n * sizeof(float);

    if (cuda_check(cudaMalloc((void **)&ctx->d_A, bytes_A),
                   "Error reservando A en GPU") != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_Z_cur, bytes_Z),
                   "Error reservando Z_cur en GPU") != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_Z_nxt, bytes_Z),
                   "Error reservando Z_nxt en GPU") != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_snap, bytes_snap),
                   "Error reservando snapshot en GPU") != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    if (copy_matrix_to_device(ctx->d_A, A, m, m) != 0 ||
        copy_matrix_to_device(ctx->d_Z_cur, Z, m, n) != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    return 0;
}

double gpu_multiplicar(GpuCtx *ctx)
{
    cudaEvent_t start, stop;
    float ms = 0.0f;

    if (cuda_check(cudaEventCreate(&start), "Error creando evento CUDA") != 0 ||
        cuda_check(cudaEventCreate(&stop), "Error creando evento CUDA") != 0) {
        exit(EXIT_FAILURE);
    }

    dim3 block(BLOCK_SIZE, BLOCK_SIZE, 1);
    dim3 grid((ctx->n + BLOCK_SIZE - 1) / BLOCK_SIZE,
              (ctx->m + BLOCK_SIZE - 1) / BLOCK_SIZE, 1);

    cudaEventRecord(start);
    coalesced_mat_mul_kernel<<<grid, block>>>(ctx->d_A, ctx->d_Z_cur,
                                              ctx->d_Z_nxt, ctx->m, ctx->n,
                                              ctx->m);
    cudaError_t err = cudaGetLastError();
    if (cuda_check(err, "Error lanzando kernel coalesced_mat_mul_kernel") != 0) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        exit(EXIT_FAILURE);
    }

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&ms, start, stop);

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    float *tmp = ctx->d_Z_cur;
    ctx->d_Z_cur = ctx->d_Z_nxt;
    ctx->d_Z_nxt = tmp;

    return (double)ms;
}

void gpu_copiar_snapshot(const GpuCtx *ctx, float **snap_host)
{
    cudaError_t snap_err = cudaMemcpy(ctx->d_snap, ctx->d_Z_cur,
                                      (size_t)ctx->n * ctx->n * sizeof(float),
                                      cudaMemcpyDeviceToDevice);
    if (cuda_check(snap_err, "Error preparando snapshot en GPU") != 0) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < ctx->n; i++) {
        cudaError_t err = cudaMemcpy(snap_host[i],
                                     ctx->d_snap + (size_t)i * ctx->n,
                                     (size_t)ctx->n * sizeof(float),
                                     cudaMemcpyDeviceToHost);
        if (cuda_check(err, "Error copiando snapshot desde GPU") != 0) {
            exit(EXIT_FAILURE);
        }
    }
}

void gpu_ctx_free(GpuCtx *ctx)
{
    if (!ctx) {
        return;
    }

    cudaFree(ctx->d_A);
    cudaFree(ctx->d_Z_cur);
    cudaFree(ctx->d_Z_nxt);
    cudaFree(ctx->d_snap);

    memset(ctx, 0, sizeof(*ctx));
}
