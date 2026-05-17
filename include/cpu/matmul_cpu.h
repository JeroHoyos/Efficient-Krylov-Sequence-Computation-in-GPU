#ifndef MATMUL_CPU_H
#define MATMUL_CPU_H

/*
 * Multiplica A [m x m] por Z_in [m x n] y guarda en Z_out [m x n].
 */
void matmul_cpu(float **A, int m, int n, float **Zin, float **Zout);

#endif
