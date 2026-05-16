#ifndef BLOCK_MUL_H
#define BLOCK_MUL_H

/*
 * Multiplica A [m x m] por Z_in [m x n] y guarda en Z_out [m x n].
 * Implementación por bloques en CPU (cache-friendly).
 */
void multiplicar_en(float **A,  int m_a, int k,
                    float **Zin, int n,
                    float **Zout);

#endif
