#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matmul_gpu.h"
#include "metricas.h"

#define PORCENTAJE_VRAM_LIBRE 85
#define BLOCK_SIZE  32
#define TILE_SIZE   32
#define SLAB_STAGE_MAX_BYTES ((size_t)256 * 1024 * 1024)

/// Kernel tiled de multiplicación de matrices: d_A_slab × d_Z_in → d_Z_out.
///
/// Carga tiles TILE_SIZE×TILE_SIZE de A y Z_in en memoria compartida para reducir
/// accesos a memoria global. Soporta modo FULL (slab_rows == m) y modo SLAB
/// (subconjunto horizontal de A) mediante el parámetro row_offset.
///
/// Argumentos:
/// - d_A_slab:   filas [row_offset, row_offset+slab_rows) de A en VRAM, tamaño [slab_rows × inner].
/// - d_Z_in:     matriz Z de entrada en VRAM, tamaño [inner × cols].
/// - d_Z_out:    matriz Z de salida en VRAM, tamaño [M × cols] (M = m total).
/// - row_offset: índice de la primera fila global que escribe este slab en d_Z_out.
/// - slab_rows:  número de filas de A procesadas en este lanzamiento.
/// - cols:       número de columnas de Z_in y Z_out (= n).
/// - inner:      dimensión interior de la multiplicación (= m, columnas de A / filas de Z_in).
///
__global__ void tiled_mat_mul_kernel(const float *__restrict__ d_A_slab,
                                     const float *__restrict__ d_Z_in,
                                     float *__restrict__ d_Z_out,
                                     int row_offset, int slab_rows,
                                     int cols, int inner) {
    __shared__ float As[TILE_SIZE][TILE_SIZE];
    __shared__ float Bs[TILE_SIZE][TILE_SIZE];

    const int tx  = threadIdx.x;
    const int ty  = threadIdx.y;
    const int col = blockIdx.x * TILE_SIZE + tx;
    const int row = blockIdx.y * TILE_SIZE + ty;

    float acc = 0.0f;
    const int num_tiles = (inner + TILE_SIZE - 1) / TILE_SIZE;

    for (int t = 0; t < num_tiles; t++) {
        // Carga tile de A_slab: hilo (ty,tx) lee A[row, t*TILE+tx].
        // Hilos del warp comparten ty y varían tx → direcciones consecutivas → acceso coalescido.
        const int k_A = t * TILE_SIZE + tx;
        As[ty][tx] = (row < slab_rows && k_A < inner)
                     ? d_A_slab[(size_t)row * inner + k_A]
                     : 0.0f;

        // Carga tile de Z_in: hilo (ty,tx) lee Z_in[t*TILE+ty, col].
        // Hilos del warp comparten ty y varían tx (col) → consecutivo → coalescido.
        const int k_B = t * TILE_SIZE + ty;
        Bs[ty][tx] = (k_B < inner && col < cols)
                     ? d_Z_in[(size_t)k_B * cols + col]
                     : 0.0f;

        __syncthreads();

        // As[ty][k]: todos los hilos del warp tienen el mismo ty y k → broadcast, sin conflicto de bancos.
        // Bs[k][tx]: todos los hilos del warp tienen el mismo k, varían tx → sin conflicto de bancos.
        #pragma unroll
        for (int k = 0; k < TILE_SIZE; k++) {
            acc += As[ty][k] * Bs[k][tx];
        }

        __syncthreads();
    }

    if (row < slab_rows && col < cols) {
        d_Z_out[((size_t)row_offset + row) * cols + col] = acc;
    }
}

/// Verifica el resultado de una llamada CUDA e imprime el mensaje de error si falla.
///
/// Argumentos:
/// - err: código de error retornado por la llamada CUDA.
/// - msg: prefijo del mensaje de error a imprimir.
///
/// Retorna 0 en éxito, -1 en error.
///
static int cuda_check(cudaError_t err, const char *msg)
{
    if (err != cudaSuccess) {
        fprintf(stderr, "%s: %s\n", msg, cudaGetErrorString(err));
        return -1;
    }
    return 0;
}

/// Copia una matriz 2D (arreglo de punteros) fila por fila al buffer lineal dst en VRAM.
///
/// Argumentos:
/// - dst:  buffer destino contiguo en VRAM con capacidad rows×cols floats.
/// - src:  matriz 2D en host; src[i] apunta a la fila i con cols floats.
/// - rows: número de filas a copiar.
/// - cols: número de columnas por fila.
///
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

/// Copia un slab de filas (en RAM pageable como arreglo de punteros) a un
/// buffer lineal pinned listo para enviarse al device por DMA.
///
/// Se ejecuta en el CPU; corre solapado con el cómputo del slab anterior en GPU
/// gracias al doble buffer staging[2] alimentado por dos streams CUDA distintos.
///
/// Argumentos:
/// - stage:      buffer destino pinned contiguo de tamaño >= rows×cols floats.
/// - A_2d:       matriz 2D pageable; A_2d[i] apunta a la fila i con cols floats.
/// - row_offset: índice global de la primera fila a copiar.
/// - rows:       número de filas a copiar.
/// - cols:       número de columnas por fila.
///
static void fill_stage_from_pageable(float *stage, float **A_2d,
                                     int row_offset, int rows, int cols)
{
    for (int i = 0; i < rows; i++) {
        memcpy(stage + (size_t)i * cols, A_2d[row_offset + i],
               (size_t)cols * sizeof(float));
    }
}

/// Inicializa el contexto GPU en modo FULL: reserva A, Z_cur, Z_nxt y snapshot completos
/// en VRAM y copia los datos desde host.
///
/// Argumentos:
/// - ctx:        contexto GPU a inicializar (campos m y n ya establecidos).
/// - A, Z, bytes_A, bytes_Z, bytes_snap: matrices A y Z en host y sus tamaños en bytes.
///
/// Retorna 0 en éxito, -1 en error.
///
int init_full_mode(GpuCtx *ctx, float **A, float **Z,size_t bytes_A, size_t bytes_Z, size_t bytes_snap)
{
    ctx->mode = GPU_EXEC_FULL;
    ctx->slab_rows = ctx->m;

    // Reserva los buffers en VRAM.
    if (cuda_check(cudaMalloc((void **)&ctx->d_A,     bytes_A),    "Error reservando A en GPU")        != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_Z_cur, bytes_Z),    "Error reservando Z_cur en GPU")    != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_Z_nxt, bytes_Z),    "Error reservando Z_nxt en GPU")    != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_snap,  bytes_snap), "Error reservando snapshot en GPU") != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    // Copia A y Z desde el host a VRAM.
    if (copy_matrix_to_device(ctx->d_A,     A, ctx->m, ctx->m) != 0 ||
        copy_matrix_to_device(ctx->d_Z_cur, Z, ctx->m, ctx->n) != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    return 0;
}

/// Inicializa el contexto GPU en modo SLAB.
///
/// Diseño de memoria:
///   - A se mantiene como matriz 2D en RAM pageable del host; ctx guarda una
///     referencia no-owning a sus filas en h_A_2d.
///   - Dos buffers pinned de staging h_A_stage[0..1] (tamaño slab_bytes c/u)
///     reciben los slabs de A por CPU memcpy antes del H2D DMA hacia VRAM.
///   - En VRAM viven Z_cur, Z_nxt, snapshot y dos buffers d_A_slab[0..1] que
///     alternan en el pipeline de doble buffer alimentado por dos streams.
///
/// Argumentos:
/// - ctx:        contexto GPU a inicializar (campos m y n ya establecidos).
/// - A:          matriz A [m × m] en host pageable. ctx mantiene referencia.
/// - Z:          matriz Z inicial [m × n] en host (se copia a VRAM).
/// - bytes_Z:    tamaño en bytes de Z (VRAM y host pinned de salida).
/// - bytes_snap: tamaño en bytes del buffer de snapshot [n × n].
/// - slab_rows:  número máximo de filas de A en cada slab.
///
/// Retorna 0 en éxito, -1 en error.
///
int init_slab_mode(GpuCtx *ctx, float **A, float **Z, size_t bytes_Z, size_t bytes_snap, int slab_rows) {
    // Configura el modo y guarda referencia no-owning a A en host.
    ctx->mode      = GPU_EXEC_SLAB;
    ctx->slab_rows = slab_rows;
    ctx->h_A_2d    = A;

    const size_t slab_bytes = (size_t)slab_rows * ctx->m * sizeof(float);

    // Reserva los buffers pinned en host: dos staging de A y el buffer de salida Z.
    if (cuda_check(cudaMallocHost((void **)&ctx->h_A_stage[0],   slab_bytes), "Error reservando staging A[0] pinned en host") != 0 ||
        cuda_check(cudaMallocHost((void **)&ctx->h_A_stage[1],   slab_bytes), "Error reservando staging A[1] pinned en host") != 0 ||
        cuda_check(cudaMallocHost((void **)&ctx->h_Z_out_pinned, bytes_Z),   "Error reservando Z_out pinned en host")         != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    // Reserva los buffers en VRAM: Z_cur, Z_nxt, snapshot y los dos slabs de A.
    if (cuda_check(cudaMalloc((void **)&ctx->d_Z_cur,     bytes_Z),    "Error reservando Z_cur en GPU")     != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_Z_nxt,     bytes_Z),    "Error reservando Z_nxt en GPU")     != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_snap,      bytes_snap), "Error reservando snapshot en GPU")  != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_A_slab[0], slab_bytes), "Error reservando slab A[0] en GPU") != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_A_slab[1], slab_bytes), "Error reservando slab A[1] en GPU") != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    // Copia Z inicial desde host a VRAM.
    if (copy_matrix_to_device(ctx->d_Z_cur, Z, ctx->m, ctx->n) != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    // Crea los dos streams CUDA para el pipeline de doble buffer.
    for (int i = 0; i < 2; i++) {
        if (cuda_check(cudaStreamCreateWithFlags(&ctx->streams[i], cudaStreamNonBlocking),
                       "Error creando stream CUDA") != 0) {
            gpu_ctx_free(ctx);
            return -1;
        }
    }

    return 0;
}


int gpu_ctx_init(GpuCtx *ctx, float **A, float **Z, int m, int n) {

    // Inicializa el contexto GPU en 0 y establece m, n. 
    memset(ctx, 0, sizeof(*ctx));
    ctx->m = m;
    ctx->n = n;

    // Calcula los tamaños en bytes de A, Z y snapshot. 
    const size_t bytes_A = (size_t)m * m * sizeof(float);
    const size_t bytes_Z = (size_t)m * n * sizeof(float);
    const size_t bytes_snap = (size_t)n * n * sizeof(float);

    ctx->bytes_Z = bytes_Z;

    // Consulta la VRAM disponible.
    size_t free_vram = 0;
    size_t total_vram = 0;
    if (cuda_check(cudaMemGetInfo(&free_vram, &total_vram), "Error consultando, VRAM disponible") != 0) {
        return -1;
    }

    // Calcula el límite seguro de VRAM y los requisitos de cada modo,
    // luego decide entre modo FULL, modo SLAB o error por memoria insuficiente.

    const size_t budget = (free_vram / 100) * PORCENTAJE_VRAM_LIBRE; 
    const size_t z_required = 2 * bytes_Z + bytes_snap; // Z_cur + Z_nxt + snapshot
    const size_t full_required = bytes_A + 2 * bytes_Z; // A + Z_cur + Z_nxt
    const size_t full_required_with_snap = full_required + bytes_snap; // A + Z_cur + Z_nxt + snapshot

    printf("  VRAM GPU libre: %.2f GiB / %.2f GiB; presupuesto seguro: %.2f GiB\n",
            bytes_to_gib(free_vram), bytes_to_gib(total_vram),bytes_to_gib(budget));
    printf("  Memoria A + Z_in + Z_out: %.2f GiB\n",
            bytes_to_gib(full_required));

    if (full_required_with_snap <= budget) {
        printf("  Modo GPU: full VRAM, multiplicacion single-pass\n");
        return init_full_mode(ctx, A, Z, bytes_A, bytes_Z, bytes_snap);
    }

    if (z_required >= budget) {
        fprintf(stderr,
                "VRAM insuficiente: Z_in + Z_out + snapshot requieren %.2f GiB y el presupuesto seguro es %.2f GiB\n",
                bytes_to_gib(z_required), bytes_to_gib(budget));
        return -1;
    }

    const size_t row_bytes = (size_t)m * sizeof(float); // bytes por fila de A
    const size_t slab_budget_vram = budget - z_required; // VRAM disponible para los slabs de A
    size_t slab_rows_vram = slab_budget_vram / (2 * row_bytes); // filas por slab limitado por VRAM 
    size_t slab_rows_pin  = SLAB_STAGE_MAX_BYTES / row_bytes; // filas por slab limitado por el tamaño máximo del staging pinned (RAM)

    // El slab se limita por VRAM, por límite de RAM pinned y por m.
    size_t slab_rows;
    if (slab_rows_vram < slab_rows_pin) {
        slab_rows = slab_rows_vram;
    } else {
        slab_rows = slab_rows_pin;
    }

    // El slab no puede tener más filas que m.
    if (slab_rows > (size_t)m) {
        slab_rows = (size_t)m;
    }

    // Para facilitar la lógica del kernel y aprovechar la memoria compartida, se ajusta slab_rows
    if (slab_rows >= TILE_SIZE) {
        slab_rows = (slab_rows / TILE_SIZE) * TILE_SIZE;
    }

    if (slab_rows == 0) {
        fprintf(stderr,
                "VRAM o RAM pinned insuficiente para un slab doble de A (fila de A = %.2f MiB)\n",
                (double)row_bytes / (1024.0 * 1024.0));
        return -1;
    }

    const size_t slab_bytes = slab_rows * row_bytes; // bytes por slab de A
    const size_t pinned_total = 2 * slab_bytes + bytes_Z; // RAM pinned total para staging de A y Z_out
    const int num_slabs = (int)(((size_t)m + slab_rows - 1) / slab_rows); // número de slabs necesarios para cubrir A

    printf("  Modo GPU: slabs horizontales de A con doble buffer + staging pinned\n");

    printf("  Slab: %zu filas (%.2f MiB por buffer, %d slabs por iter Krylov)\n",
           slab_rows, (double)slab_bytes / (1024.0 * 1024.0), num_slabs);

    printf("  RAM pinned reservada (2 staging A + Z_out): %.2f MiB\n",
           (double)pinned_total / (1024.0 * 1024.0));

    printf("  RAM pageable de A (referenciada): %.2f GiB\n",
           bytes_to_gib(bytes_A));

    return init_slab_mode(ctx, A, Z, bytes_Z, bytes_snap, (int)slab_rows);
}

/// Ejecuta una iteración A×Z en modo FULL
///
/// Argumentos:
/// - ctx: Estructura con los parametros para ejecutar
///
void gpu_multiplicar_full(GpuCtx *ctx)
{
    // Configura el grid y el bloque para cubrir la matriz completa.
    dim3 block(TILE_SIZE, TILE_SIZE, 1);
    dim3 grid((ctx->n + TILE_SIZE - 1) / TILE_SIZE,
              (ctx->m + TILE_SIZE - 1) / TILE_SIZE, 1);

    // Ejecuta el kernel de la multiplicación de matrices
    tiled_mat_mul_kernel<<<grid, block>>>(ctx->d_A, ctx->d_Z_cur, ctx->d_Z_nxt,0, ctx->m, ctx->n, ctx->m);

    cudaError_t err = cudaGetLastError();
    if (cuda_check(err, "Error lanzando tiled_mat_mul_kernel (full)") != 0) {
        exit(EXIT_FAILURE);
    }

    // Espera a que el kernel termine antes de intercambiar los punteros.
    if (cuda_check(cudaDeviceSynchronize(), "Error sincronizando kernel CUDA") != 0) {
        exit(EXIT_FAILURE);
    }

    // Intercambia Z_cur y Z_nxt para la siguiente iteración.
    float *tmp   = ctx->d_Z_cur;
    ctx->d_Z_cur = ctx->d_Z_nxt;
    ctx->d_Z_nxt = tmp;
}

static void gpu_multiplicar_slab(GpuCtx *ctx) {

    /// Se cargan las primeras filas dependiendo de si el slab cabe completo o no.
    int first_rows;
    if (ctx->slab_rows < ctx->m) {
        first_rows = ctx->slab_rows;
    }
    else {
        first_rows = ctx->m;
    }


    const size_t first_bytes = (size_t)first_rows * ctx->m * sizeof(float);

    fill_stage_from_pageable(ctx->h_A_stage[0], ctx->h_A_2d,
                             0, first_rows, ctx->m);

    if (cuda_check(cudaMemcpyAsync(ctx->d_A_slab[0], ctx->h_A_stage[0],
                                   first_bytes, cudaMemcpyHostToDevice,
                                   ctx->streams[0]),
                   "Error precargando slab 0") != 0) {
        exit(EXIT_FAILURE);
    }

    dim3 block(TILE_SIZE, TILE_SIZE, 1);
    int slab_idx = 0;

    for (int row_offset = 0; row_offset < ctx->m;
         row_offset += ctx->slab_rows, slab_idx++) {

        const int cur_buf = slab_idx & 1;
        const int nxt_buf = cur_buf ^ 1;
        cudaStream_t cur_stream = ctx->streams[cur_buf];
        cudaStream_t nxt_stream = ctx->streams[nxt_buf];

        int rows_this_slab = ctx->slab_rows;
        if (row_offset + rows_this_slab > ctx->m)
            rows_this_slab = ctx->m - row_offset;

        // CÓMPUTO: kernel del slab actual sobre cur_stream. La H2D del slab ya
        // fue encolada (pre-carga inicial o prefetch previo); in-order garantiza
        // que el kernel arranque después de que d_A_slab[cur_buf] esté listo.
        dim3 grid((ctx->n + TILE_SIZE - 1) / TILE_SIZE,
                  (rows_this_slab + TILE_SIZE - 1) / TILE_SIZE, 1);

        tiled_mat_mul_kernel<<<grid, block, 0, cur_stream>>>(
            ctx->d_A_slab[cur_buf], ctx->d_Z_cur, ctx->d_Z_nxt,
            row_offset, rows_this_slab, ctx->n, ctx->m);

        cudaError_t err = cudaGetLastError();
        if (cuda_check(err, "Error lanzando tiled_mat_mul_kernel (slab)") != 0) {
            exit(EXIT_FAILURE);
        }

        // PREFETCH del siguiente slab:
        //   - Espera a que termine la H2D previa fuera de h_A_stage[nxt_buf]
        //     (encolada en nxt_stream): así la CPU puede sobrescribir el
        //     staging sin riesgo. En estado estacionario esa H2D ya terminó.
        //   - CPU memcpy pageable → h_A_stage[nxt_buf]. Corre en paralelo con
        //     el kernel del slab actual (que está en cur_stream).
        //   - cudaMemcpyAsync H2D staging → d_A_slab[nxt_buf] sobre nxt_stream;
        //     in-order asegura que el kernel previo del nxt_stream (si lo hay)
        //     termine antes de tocar d_A_slab[nxt_buf].
        const int nxt_row_offset = row_offset + ctx->slab_rows;
        if (nxt_row_offset < ctx->m) {
            int nxt_rows = ctx->slab_rows;
            if (nxt_row_offset + nxt_rows > ctx->m)
                nxt_rows = ctx->m - nxt_row_offset;
            const size_t nxt_bytes = (size_t)nxt_rows * ctx->m * sizeof(float);

            if (cuda_check(cudaStreamSynchronize(nxt_stream),
                           "Error sincronizando nxt_stream para reusar staging") != 0) {
                exit(EXIT_FAILURE);
            }

            fill_stage_from_pageable(ctx->h_A_stage[nxt_buf], ctx->h_A_2d,
                                     nxt_row_offset, nxt_rows, ctx->m);

            if (cuda_check(cudaMemcpyAsync(ctx->d_A_slab[nxt_buf],
                                           ctx->h_A_stage[nxt_buf],
                                           nxt_bytes,
                                           cudaMemcpyHostToDevice, nxt_stream),
                           "Error prefetching slab") != 0) {
                exit(EXIT_FAILURE);
            }
        }
    }

    // Espera todo el cómputo del último slab antes de tocar Z_nxt.
    for (int i = 0; i < 2; i++) {
        if (cuda_check(cudaStreamSynchronize(ctx->streams[i]),
                       "Error sincronizando streams al cierre del slab") != 0) {
            exit(EXIT_FAILURE);
        }
    }

    // Sólo se necesita el bloque n×n del comienzo de Z_nxt para el snapshot;
    // copiarlo (y no los m×n completos) ahorra ~m/n veces de ancho de banda PCIe.
    const size_t snap_bytes = (size_t)ctx->n * ctx->n * sizeof(float);
    if (ctx->h_Z_out_pinned) {
        if (cuda_check(cudaMemcpy(ctx->h_Z_out_pinned, ctx->d_Z_nxt,
                                  snap_bytes, cudaMemcpyDeviceToHost),
                       "Error copiando snapshot a host pinned") != 0) {
            exit(EXIT_FAILURE);
        }
    }

    float *tmp   = ctx->d_Z_cur;
    ctx->d_Z_cur = ctx->d_Z_nxt;
    ctx->d_Z_nxt = tmp;
}

/// Llama la función de multiplicación según el modo
///
/// Argumentos:
/// - ctx: contexto GPU inicializado con gpu_ctx_init().
///
void gpu_multiplicar(GpuCtx *ctx) {
    if (ctx->mode == GPU_EXEC_SLAB) {
        return gpu_multiplicar_slab(ctx);
    }
    return gpu_multiplicar_full(ctx);
}

/// Copia las primeras n filas × n columnas de Z_cur al arreglo host snap_host.
///
/// En modo SLAB copia directamente desde el buffer pinned h_Z_out_pinned (ya en host).
/// En modo FULL hace una copia device→device al buffer d_snap y luego la baja al host.
///
/// Argumentos:
/// - ctx:       contexto GPU con el resultado de la última iteración.
/// - snap_host: arreglo de punteros host de n filas × n columnas donde se escribe el snapshot.
///
void gpu_copiar_snapshot(const GpuCtx *ctx, float **snap_host)
{
    if (ctx->mode == GPU_EXEC_SLAB && ctx->h_Z_out_pinned) {
        // En modo SLAB el resultado ya está en host pinned; se copian las primeras n filas.
        for (int i = 0; i < ctx->n; i++) {
            memcpy(snap_host[i], ctx->h_Z_out_pinned + (size_t)i * ctx->n,
                   (size_t)ctx->n * sizeof(float));
        }
        return;
    }

    // En modo FULL se copia primero el bloque n×n desde d_Z_cur a d_snap en VRAM.
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

/// Libera todos los recursos CUDA del contexto: streams, buffers en VRAM y memoria pinned.
///
/// Argumentos:
/// - ctx: contexto GPU a liberar. Si es NULL la función no hace nada.
///
void gpu_ctx_free(GpuCtx *ctx)
{
    if (!ctx) {
        return;
    }

    for (int i = 0; i < 2; i++) {
        if (ctx->streams[i]) {
            cudaStreamSynchronize(ctx->streams[i]);
            cudaStreamDestroy(ctx->streams[i]);
        }
    }

    cudaFree(ctx->d_A);
    cudaFree(ctx->d_Z_cur);
    cudaFree(ctx->d_Z_nxt);
    cudaFree(ctx->d_snap);
    cudaFree(ctx->d_A_slab[0]);
    cudaFree(ctx->d_A_slab[1]);

    cudaFreeHost(ctx->h_A_stage[0]);
    cudaFreeHost(ctx->h_A_stage[1]);
    cudaFreeHost(ctx->h_Z_out_pinned);

    // h_A_2d es una referencia no-owning; el caller libera A despues de gpu_ctx_free.

    memset(ctx, 0, sizeof(*ctx));
}
