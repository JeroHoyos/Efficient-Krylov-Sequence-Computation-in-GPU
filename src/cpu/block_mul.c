#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
  #include <windows.h>
#endif
#include "block_mul.h"
#include "matrices.h"
#include "metricas.h"
#include "perfil.h"

/* Retorna el tiempo de pared actual en segundos con alta resolución. */
static double tiempo_actual(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

/* Libera un bloque de nf filas reservado con malloc. */
static void liberar_bloque(float **b, int nf) {
    for (int i = 0; i < nf; i++) free(b[i]);
    free(b);
}

/* Lee el subbloque A[fila0..fila0+nf)[col0..col0+nc) de un archivo binario row-major
   cuya fila tiene ncols_total columnas en total. */
static float **leer_bloque_bin(FILE *f, int fila0, int nf, int col0, int nc, int ncols_total) {
    float **bloque = malloc(nf * sizeof(float *));
    for (int i = 0; i < nf; i++) {
        bloque[i] = malloc(nc * sizeof(float));
        long off = ((long)(fila0 + i) * ncols_total + col0) * (long)sizeof(float);
        fseek(f, off, SEEK_SET);
        fread(bloque[i], sizeof(float), nc, f);
    }
    return bloque;
}

/* C_strip[ni×n] += A_block[ni×nk] × Z_block[nk×n] */
static void mul_acumular(float **C, float **A, float **Z, int ni, int nk, int n) {
    for (int i = 0; i < ni; i++)
        for (int k = 0; k < nk; k++) {
            float aik = A[i][k];
            for (int j = 0; j < n; j++)
                C[i][j] += aik * Z[k][j];
        }
}

/* Calcula C[m×n] = A[m×m] × Z[m×n] leyendo de binarios por bloques de BS×BS.
   Escribe C secuencialmente en ruta_C. Copia las primeras n_snap filas en snap_out. */
static void multiplicar_bloques(const char *ruta_A, const char *ruta_Z,
                                const char *ruta_C, int m, int n, int BS,
                                float **snap_out, int n_snap) {
    FILE *fA = fopen(ruta_A, "rb");
    FILE *fZ = fopen(ruta_Z, "rb");
    FILE *fC = fopen(ruta_C, "wb");
    if (!fA || !fZ || !fC) {
        fprintf(stderr, "Error abriendo archivos para multiplicación por bloques\n");
        if (fA) fclose(fA);
        if (fZ) fclose(fZ);
        if (fC) fclose(fC);
        return;
    }

    int snap_restante = n_snap;

    for (int i0 = 0; i0 < m; i0 += BS) {
        int ni = (i0 + BS <= m) ? BS : (m - i0);

        float **C_strip = malloc(ni * sizeof(float *));
        for (int r = 0; r < ni; r++)
            C_strip[r] = calloc(n, sizeof(float));

        for (int k0 = 0; k0 < m; k0 += BS) {
            int nk = (k0 + BS <= m) ? BS : (m - k0);
            float **Ab = leer_bloque_bin(fA, i0, ni, k0, nk, m);
            float **Zb = leer_bloque_bin(fZ, k0, nk, 0,  n,  n);
            mul_acumular(C_strip, Ab, Zb, ni, nk, n);
            liberar_bloque(Ab, ni);
            liberar_bloque(Zb, nk);
        }

        if (snap_restante > 0 && snap_out) {
            int fs = (snap_restante < ni) ? snap_restante : ni;
            for (int r = 0; r < fs; r++)
                memcpy(snap_out[i0 + r], C_strip[r], n * sizeof(float));
            snap_restante -= fs;
        }

        /* Escritura secuencial: i0 siempre crece, no hace falta fseek. */
        for (int r = 0; r < ni; r++)
            fwrite(C_strip[r], sizeof(float), n, fC);

        liberar_bloque(C_strip, ni);
    }

    fclose(fA);
    fclose(fZ);
    fclose(fC);
}

/* Ejecuta p.l iteraciones de Z_{i+1} = A × Z_i usando archivos binarios y bloques de BS. */
void ejecutar_bucle_bloques(const char *matrices_dir, Parametros p,
                            const char *outdir, Metricas *m, int BS) {
    char ruta_A[300], z_a[300], z_b[300];
    snprintf(ruta_A, sizeof(ruta_A), "%s/A.bin", matrices_dir);
    snprintf(z_a,   sizeof(z_a),   "%s/Z.bin",     matrices_dir);
    snprintf(z_b,   sizeof(z_b),   "%s/Z_new.bin", matrices_dir);

    char *z_cur = z_a;
    char *z_nxt = z_b;

    /* Un único buffer de snapshot reutilizado en todas las iteraciones. */
    float **snap = crear_matriz(p.n, p.n);

    printf("\n=== Ejecucion (bloques, BS=%d) ===\n", BS);

    long minflt_ini = leer_minflt();
    long majflt_ini = leer_majflt();
    double t_total_ini = tiempo_actual();

    for (int iter = 0; iter < p.l; iter++) {
        /* Misma simetría de medición que el modo en RAM. */
        double t0 = tiempo_actual();
        long long cpu_antes = leer_cpu_us_proceso();

        multiplicar_bloques(ruta_A, z_cur, z_nxt, p.m, p.n, BS, snap, p.n);

        long long cpu_despues = leer_cpu_us_proceso();
        double wall = tiempo_actual() - t0;

        m->tiempos_iter[iter] = wall;

        double cpu_s = (double)(cpu_despues - cpu_antes) / 1e6;
        double pct_cpu = wall > 0.0 ? (cpu_s / wall) * 100.0 : 0.0;
        m->cpu_iter[iter] = pct_cpu > 100.0 ? 100.0 : pct_cpu;

        long rss = leer_rss_kb();
        m->rss_iter[iter] = rss;
        if (rss > m->pico_mem_kb) m->pico_mem_kb = rss;

        guardar_resultado_txt(snap, p.n, p.n, iter, outdir);

        char *tmp = z_cur; z_cur = z_nxt; z_nxt = tmp;
    }

    m->tiempo_total        = tiempo_actual() - t_total_ini;
    m->fallos_pagina_menor = leer_minflt() - minflt_ini;
    m->fallos_pagina_mayor = leer_majflt() - majflt_ini;

    double suma = 0.0;
    for (int i = 0; i < p.l; i++) suma += m->tiempos_iter[i];
    m->tiempo_promedio = p.l > 0 ? suma / p.l : 0.0;

    liberar_matriz(snap, p.n);
}
