//! matmul_gpu.cu - Multiplicación de matrices en GPU mediante CUDA.
//!
//! Implementa la multiplicación iterativa A×Z necesaria para la secuencia de Krylov
//! A^l × Z usando CUDA. El modo de ejecución se selecciona automáticamente según
//! la VRAM disponible al inicializar el contexto:
//!
//!   - Modo FULL: A, Z_cur y Z_nxt residen por completo en VRAM. Cada iteración
//!                lanza un único kernel con tiles en memoria compartida (32×32).
//!
//!   - Modo SLAB: cuando A no cabe en VRAM se almacena en memoria host pinned y
//!                se procesa en slabs horizontales con doble buffer y dos streams
//!                CUDA, solapando transferencias PCIe con cómputo en la GPU.
//!
//! Cómo funciona:
//!
//! 1. gpu_ctx_init() consulta la VRAM libre con cudaMemGetInfo y elige el modo.
//! 2. gpu_multiplicar() delega al helper interno correspondiente (full o slab).
//! 3. gpu_copiar_snapshot() descarga las primeras n×n filas de Z_cur al host.
//! 4. gpu_ctx_free() libera todos los recursos CUDA reservados.
//!
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matmul_gpu.h"

#define BLOCK_SIZE  32
#define TILE_SIZE   32
#define VRAM_SAFETY_NUM 85
#define VRAM_SAFETY_DEN 100

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
                                     int cols, int inner)
{
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

/// Calcula el presupuesto seguro de VRAM aplicando el margen VRAM_SAFETY_NUM/VRAM_SAFETY_DEN
/// sobre los bytes libres reportados por cudaMemGetInfo.
///
/// Argumentos:
/// - free_bytes: bytes de VRAM libres reportados por cudaMemGetInfo.
///
static size_t safe_vram_budget(size_t free_bytes)
{
    return (free_bytes / VRAM_SAFETY_DEN) * VRAM_SAFETY_NUM;
}

/// Convierte bytes a GiB (gibibytes) para mostrar en mensajes de diagnóstico.
///
/// Argumentos:
/// - bytes: cantidad de bytes a convertir.
///
static double bytes_to_gib(size_t bytes)
{
    return (double)bytes / (1024.0 * 1024.0 * 1024.0);
}

/// Calcula el tamaño en bytes de una matriz rows×cols de floats con detección de overflow.
///
/// Argumentos:
/// - rows: número de filas de la matriz.
/// - cols: número de columnas de la matriz.
/// - out:  puntero donde se escribe el resultado en bytes.
///
/// Retorna 0 en éxito, -1 si las dimensiones producen overflow.
///
static int matrix_bytes(int rows, int cols, size_t *out)
{
    if (rows <= 0 || cols <= 0) {
        fprintf(stderr, "Dimensiones invalidas: %d x %d\n", rows, cols);
        return -1;
    }

    const size_t r = (size_t)rows;
    const size_t c = (size_t)cols;

    if (r != 0 && c > ((size_t)-1) / r) {
        fprintf(stderr, "Overflow calculando cantidad de elementos\n");
        return -1;
    }

    const size_t elems = r * c;
    if (elems > ((size_t)-1) / sizeof(float)) {
        fprintf(stderr, "Overflow calculando bytes de matriz\n");
        return -1;
    }

    *out = elems * sizeof(float);
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

/// Copia una matriz 2D (arreglo de punteros) fila por fila al buffer lineal dst en memoria pinned.
///
/// Argumentos:
/// - dst:  buffer destino contiguo en host pinned con capacidad rows×cols floats.
/// - src:  matriz 2D en host; src[i] apunta a la fila i con cols floats.
/// - rows: número de filas a copiar.
/// - cols: número de columnas por fila.
///
static int copy_matrix_to_pinned(float *dst, float **src, int rows, int cols)
{
    for (int i = 0; i < rows; i++) {
        if (!src[i]) {
            fprintf(stderr, "Fila host nula copiando matriz pinned\n");
            return -1;
        }
        memcpy(dst + (size_t)i * cols, src[i], (size_t)cols * sizeof(float));
    }
    return 0;
}

/// Inicializa el contexto GPU en modo FULL: reserva A, Z_cur, Z_nxt y snapshot completos
/// en VRAM y copia los datos desde host.
///
/// Argumentos:
/// - ctx:        contexto GPU a inicializar (campos m y n ya establecidos).
/// - A:          matriz A [m × m] en host.
/// - Z:          matriz Z inicial [m × n] en host.
/// - bytes_A:    tamaño en bytes de A.
/// - bytes_Z:    tamaño en bytes de Z.
/// - bytes_snap: tamaño en bytes del buffer de snapshot [n × n].
///
/// Retorna 0 en éxito, -1 en error.
///
static int init_full_mode(GpuCtx *ctx, float **A, float **Z,
                          size_t bytes_A, size_t bytes_Z,
                          size_t bytes_snap)
{
    ctx->mode = GPU_EXEC_FULL;
    ctx->slab_rows = ctx->m;

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

    if (copy_matrix_to_device(ctx->d_A, A, ctx->m, ctx->m) != 0 ||
        copy_matrix_to_device(ctx->d_Z_cur, Z, ctx->m, ctx->n) != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    return 0;
}

/// Inicializa el contexto GPU en modo SLAB: A va a memoria host pinned, Z_cur y Z_nxt a VRAM.
/// Crea dos buffers de slab de A en VRAM y dos streams CUDA para el pipeline de doble buffer.
///
/// Argumentos:
/// - ctx:        contexto GPU a inicializar (campos m y n ya establecidos).
/// - A:          matriz A [m × m] en host.
/// - Z:          matriz Z inicial [m × n] en host.
/// - bytes_A:    tamaño en bytes de A (para alojar en host pinned).
/// - bytes_Z:    tamaño en bytes de Z (VRAM y host pinned de salida).
/// - bytes_snap: tamaño en bytes del buffer de snapshot [n × n].
/// - slab_rows:  número máximo de filas de A en cada slab de VRAM.
///
/// Retorna 0 en éxito, -1 en error.
///
static int init_slab_mode(GpuCtx *ctx, float **A, float **Z,
                          size_t bytes_A, size_t bytes_Z,
                          size_t bytes_snap, int slab_rows)
{
    ctx->mode = GPU_EXEC_SLAB;
    ctx->slab_rows = slab_rows;

    const size_t slab_bytes = (size_t)slab_rows * ctx->m * sizeof(float);

    if (cuda_check(cudaMallocHost((void **)&ctx->h_A_pinned, bytes_A),
                   "Error reservando A pinned en host") != 0 ||
        cuda_check(cudaMallocHost((void **)&ctx->h_Z_out_pinned, bytes_Z),
                   "Error reservando Z_out pinned en host") != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    if (copy_matrix_to_pinned(ctx->h_A_pinned, A, ctx->m, ctx->m) != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    if (cuda_check(cudaMalloc((void **)&ctx->d_Z_cur, bytes_Z),
                   "Error reservando Z_cur en GPU") != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_Z_nxt, bytes_Z),
                   "Error reservando Z_nxt en GPU") != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_snap, bytes_snap),
                   "Error reservando snapshot en GPU") != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_A_slab[0], slab_bytes),
                   "Error reservando slab A[0] en GPU") != 0 ||
        cuda_check(cudaMalloc((void **)&ctx->d_A_slab[1], slab_bytes),
                   "Error reservando slab A[1] en GPU") != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    if (copy_matrix_to_device(ctx->d_Z_cur, Z, ctx->m, ctx->n) != 0) {
        gpu_ctx_free(ctx);
        return -1;
    }

    for (int i = 0; i < 2; i++) {
        if (cuda_check(cudaStreamCreateWithFlags(&ctx->streams[i],
                                                 cudaStreamNonBlocking),
                       "Error creando stream CUDA") != 0) {
            gpu_ctx_free(ctx);
            return -1;
        }
    }

    return 0;
}

/// Inicializa el contexto GPU consultando la VRAM disponible y eligiendo el modo de ejecución.
///
/// Si A + Z_in + Z_out + snapshot caben en el presupuesto seguro de VRAM se usa modo FULL;
/// de lo contrario se calcula el tamaño máximo de slab y se inicializa el modo SLAB.
///
/// Argumentos:
/// - ctx: estructura de contexto a inicializar.
/// - A:   matriz de entrada [m × m] en host.
/// - Z:   vector de estado inicial [m × n] en host.
/// - m:   número de filas y columnas de A.
/// - n:   número de columnas de Z.
///
/// Retorna 0 en éxito, -1 en error.
///
int gpu_ctx_init(GpuCtx *ctx, float **A, float **Z, int m, int n)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->m = m;
    ctx->n = n;

    size_t bytes_A = 0;
    size_t bytes_Z = 0;
    size_t bytes_snap = 0;

    if (matrix_bytes(m, m, &bytes_A) != 0 ||
        matrix_bytes(m, n, &bytes_Z) != 0 ||
        matrix_bytes(n, n, &bytes_snap) != 0) {
        return -1;
    }

    ctx->bytes_Z = bytes_Z;

    size_t free_vram = 0;
    size_t total_vram = 0;
    if (cuda_check(cudaMemGetInfo(&free_vram, &total_vram),
                   "Error consultando VRAM disponible") != 0) {
        return -1;
    }

    const size_t budget = safe_vram_budget(free_vram);
    const size_t z_required = 2 * bytes_Z + bytes_snap;
    const size_t full_required = bytes_A + 2 * bytes_Z;
    const size_t full_required_with_snap = full_required + bytes_snap;

    printf("  VRAM GPU libre: %.2f GiB / %.2f GiB; presupuesto seguro: %.2f GiB\n",
           bytes_to_gib(free_vram), bytes_to_gib(total_vram),
           bytes_to_gib(budget));
    printf("  Memoria A + Z_in + Z_out: %.2f GiB\n",
           bytes_to_gib(full_required));

    if (full_required_with_snap <= budget) {
        printf("  Modo GPU: full VRAM, multiplicacion single-pass\n");
        return init_full_mode(ctx, A, Z, bytes_A, bytes_Z, bytes_snap);
    }

    if (z_required >= budget) {
        fprintf(stderr,
                "VRAM insuficiente: Z_in + Z_out + snapshot requieren %.2f GiB "
                "y el presupuesto seguro es %.2f GiB\n",
                bytes_to_gib(z_required), bytes_to_gib(budget));
        return -1;
    }

    const size_t row_bytes = (size_t)m * sizeof(float);
    const size_t slab_budget = budget - z_required;
    size_t slab_rows = slab_budget / (2 * row_bytes);

    if (slab_rows == 0) {
        fprintf(stderr, "VRAM insuficiente para un slab doble de A\n");
        return -1;
    }

    if (slab_rows > (size_t)m) {
        slab_rows = (size_t)m;
    }

    printf("  Modo GPU: slabs horizontales de A con doble buffer\n");
    printf("  Slab maximo seguro: %zu filas (%.2f GiB por buffer)\n",
           slab_rows, bytes_to_gib(slab_rows * row_bytes));

    return init_slab_mode(ctx, A, Z, bytes_A, bytes_Z, bytes_snap,
                          (int)slab_rows);
}

/// Ejecuta una iteración A×Z en modo FULL: lanza el kernel tiled sobre la matriz A completa
/// en VRAM y luego intercambia los punteros Z_cur ↔ Z_nxt.
///
/// Retorna el tiempo de ejecución del kernel medido con eventos CUDA en ms.
///
static double gpu_multiplicar_full(GpuCtx *ctx)
{
    cudaEvent_t start = NULL;
    cudaEvent_t stop  = NULL;
    float ms = 0.0f;

    if (cuda_check(cudaEventCreate(&start), "Error creando evento CUDA") != 0 ||
        cuda_check(cudaEventCreate(&stop),  "Error creando evento CUDA") != 0) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        exit(EXIT_FAILURE);
    }

    dim3 block(TILE_SIZE, TILE_SIZE, 1);
    dim3 grid((ctx->n + TILE_SIZE - 1) / TILE_SIZE,
              (ctx->m + TILE_SIZE - 1) / TILE_SIZE, 1);

    if (cuda_check(cudaEventRecord(start), "Error iniciando evento CUDA") != 0) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        exit(EXIT_FAILURE);
    }

    // row_offset=0 y slab_rows=ctx->m: el kernel unificado cubre la matriz completa.
    tiled_mat_mul_kernel<<<grid, block>>>(ctx->d_A, ctx->d_Z_cur, ctx->d_Z_nxt,
                                          0, ctx->m, ctx->n, ctx->m);

    cudaError_t err = cudaGetLastError();
    if (cuda_check(err, "Error lanzando tiled_mat_mul_kernel (full)") != 0) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        exit(EXIT_FAILURE);
    }

    if (cuda_check(cudaEventRecord(stop), "Error deteniendo evento CUDA") != 0 ||
        cuda_check(cudaEventSynchronize(stop), "Error sincronizando kernel CUDA") != 0 ||
        cuda_check(cudaEventElapsedTime(&ms, start, stop),
                   "Error midiendo tiempo CUDA") != 0) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        exit(EXIT_FAILURE);
    }

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    float *tmp   = ctx->d_Z_cur;
    ctx->d_Z_cur = ctx->d_Z_nxt;
    ctx->d_Z_nxt = tmp;

    return (double)ms;
}

/// Ejecuta una iteración A×Z en modo SLAB mediante un pipeline explícito de doble buffer.
///
/// Pipeline:
///   1. Pre-carga el slab 0 en d_A_slab[0] sobre streams[0].
///   2. En cada iteración del bucle: lanza el kernel en cur_stream (que espera la
///      transferencia ya encolada) y simultáneamente prefetcha el siguiente slab en
///      nxt_stream, solapando el motor DMA con el motor de cómputo de la GPU.
///   3. Al terminar todos los slabs, vuelca Z_out completo al host pinned.
///
/// Retorna el tiempo total (transferencias + cómputo) medido con eventos CUDA en ms.
///
static double gpu_multiplicar_slab(GpuCtx *ctx)
{
    cudaEvent_t start    = NULL;
    cudaEvent_t stop     = NULL;
    cudaEvent_t done[2]  = {NULL, NULL};
    cudaStream_t timing_stream = NULL;
    float ms = 0.0f;

    if (cuda_check(cudaStreamCreateWithFlags(&timing_stream,
                                             cudaStreamNonBlocking),
                   "Error creando stream de timing") != 0 ||
        cuda_check(cudaEventCreate(&start),    "Error creando evento start")    != 0 ||
        cuda_check(cudaEventCreate(&stop),     "Error creando evento stop")     != 0 ||
        cuda_check(cudaEventCreate(&done[0]),  "Error creando evento done[0]")  != 0 ||
        cuda_check(cudaEventCreate(&done[1]),  "Error creando evento done[1]")  != 0) {
        cudaStreamDestroy(timing_stream);
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        cudaEventDestroy(done[0]);
        cudaEventDestroy(done[1]);
        exit(EXIT_FAILURE);
    }

    // Registra el inicio en el stream de timing; los dos streams de cómputo esperan antes de trabajar.
    if (cuda_check(cudaEventRecord(start, timing_stream),
                   "Error iniciando timing CUDA") != 0) {
        cudaStreamDestroy(timing_stream);
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        cudaEventDestroy(done[0]);
        cudaEventDestroy(done[1]);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < 2; i++) {
        if (cuda_check(cudaStreamWaitEvent(ctx->streams[i], start, 0),
                       "Error sincronizando stream con start") != 0) {
            exit(EXIT_FAILURE);
        }
    }

    // Pre-carga el slab 0 en buf[0] sobre streams[0].
    // El kernel del slab 0 (también en streams[0]) esperará automáticamente
    // esta transferencia gracias a la semántica in-order del stream.
    {
        int first_rows = ctx->slab_rows < ctx->m ? ctx->slab_rows : ctx->m;
        const size_t first_bytes = (size_t)first_rows * ctx->m * sizeof(float);
        if (cuda_check(cudaMemcpyAsync(ctx->d_A_slab[0], ctx->h_A_pinned,
                                       first_bytes, cudaMemcpyHostToDevice,
                                       ctx->streams[0]),
                       "Error precargando slab 0") != 0) {
            exit(EXIT_FAILURE);
        }
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

        // CÓMPUTO: lanza el kernel del slab N en cur_stream.
        // La transferencia del slab N a d_A_slab[cur_buf] fue encolada en cur_stream
        // (ya sea como pre-carga inicial o como prefetch de la iteración anterior),
        // por lo que el kernel espera automáticamente vía semántica in-order del stream.
        dim3 grid((ctx->n + TILE_SIZE - 1) / TILE_SIZE,
                  (rows_this_slab + TILE_SIZE - 1) / TILE_SIZE, 1);

        tiled_mat_mul_kernel<<<grid, block, 0, cur_stream>>>(
            ctx->d_A_slab[cur_buf], ctx->d_Z_cur, ctx->d_Z_nxt,
            row_offset, rows_this_slab, ctx->n, ctx->m);

        cudaError_t err = cudaGetLastError();
        if (cuda_check(err, "Error lanzando tiled_mat_mul_kernel (slab)") != 0) {
            exit(EXIT_FAILURE);
        }

        // PREFETCH: transfiere el slab N+1 a d_A_slab[nxt_buf] en nxt_stream.
        // Se ejecuta de forma concurrente con el kernel anterior (streams distintos):
        // el motor DMA (copia H2D) solapa con el motor de cómputo (kernel).
        // nxt_stream serializa esta copia detrás de cualquier kernel previo que usó
        // nxt_buf, evitando sobrescribir el buffer antes de que ese cómputo termine.
        const int nxt_row_offset = row_offset + ctx->slab_rows;
        if (nxt_row_offset < ctx->m) {
            int nxt_rows = ctx->slab_rows;
            if (nxt_row_offset + nxt_rows > ctx->m)
                nxt_rows = ctx->m - nxt_row_offset;
            const size_t nxt_bytes = (size_t)nxt_rows * ctx->m * sizeof(float);

            if (cuda_check(cudaMemcpyAsync(
                    ctx->d_A_slab[nxt_buf],
                    ctx->h_A_pinned + (size_t)nxt_row_offset * ctx->m,
                    nxt_bytes, cudaMemcpyHostToDevice, nxt_stream),
                   "Error prefetching slab") != 0) {
                exit(EXIT_FAILURE);
            }
        }
    }

    // Fusiona ambos streams de cómputo en el stream de timing antes de la copia D2H.
    for (int i = 0; i < 2; i++) {
        if (cuda_check(cudaEventRecord(done[i], ctx->streams[i]),
                       "Error marcando fin de stream") != 0 ||
            cuda_check(cudaStreamWaitEvent(timing_stream, done[i], 0),
                       "Error esperando fin de stream") != 0) {
            exit(EXIT_FAILURE);
        }
    }

    // Copia Z_out completo al host pinned una vez que todos los slabs terminaron.
    if (ctx->h_Z_out_pinned) {
        if (cuda_check(cudaMemcpyAsync(ctx->h_Z_out_pinned, ctx->d_Z_nxt,
                                       ctx->bytes_Z, cudaMemcpyDeviceToHost,
                                       timing_stream),
                       "Error copiando Z_out completo a host") != 0) {
            exit(EXIT_FAILURE);
        }
    }

    if (cuda_check(cudaEventRecord(stop, timing_stream),
                   "Error deteniendo timing CUDA") != 0 ||
        cuda_check(cudaEventSynchronize(stop),
                   "Error sincronizando ejecucion por slabs") != 0 ||
        cuda_check(cudaEventElapsedTime(&ms, start, stop),
                   "Error midiendo tiempo por slabs") != 0) {
        exit(EXIT_FAILURE);
    }

    cudaStreamDestroy(timing_stream);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    cudaEventDestroy(done[0]);
    cudaEventDestroy(done[1]);

    float *tmp   = ctx->d_Z_cur;
    ctx->d_Z_cur = ctx->d_Z_nxt;
    ctx->d_Z_nxt = tmp;

    return (double)ms;
}

/// Despacha la multiplicación A×Z al modo correcto (FULL o SLAB) según ctx->mode.
/// Intercambia internamente Z_cur y Z_nxt tras cada iteración.
///
/// Argumentos:
/// - ctx: contexto GPU inicializado con gpu_ctx_init().
///
/// Retorna el tiempo de ejecución en ms medido con eventos CUDA.
///
double gpu_multiplicar(GpuCtx *ctx)
{
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

    cudaFreeHost(ctx->h_A_pinned);
    cudaFreeHost(ctx->h_Z_out_pinned);

    memset(ctx, 0, sizeof(*ctx));
}
