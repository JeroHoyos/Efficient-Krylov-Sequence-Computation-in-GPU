#include <string.h>
#include "block_mul.h"

// Multiplica A[m_a×k] × Zin[k×n] → Zout[m_a×n].
void multiplicar_en(float **A,  int m_a, int k,
                    float **Zin, int n,
                    float **Zout) {

    // Inicializa Zout en cero antes de acumular productos.
    for (int i = 0; i < m_a; i++)
        memset(Zout[i], 0, n * sizeof(float));

    // Para cada fila i de A, acumula el producto punto con cada columna j de Zin.
    for (int i = 0; i < m_a; i++)
        for (int l = 0; l < k; l++)
            for (int j = 0; j < n; j++)
                Zout[i][j] += A[i][l] * Zin[l][j];
}