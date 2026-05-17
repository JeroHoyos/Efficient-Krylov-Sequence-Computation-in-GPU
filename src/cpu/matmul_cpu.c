//! matmul_cpu.c - Multiplicación de matrices en CPU.
//!
//! Se utiliza una implementación "hola mundo" de multiplicación de matrices, sin ninguna optimización.
//! El objetivo es tener una referencia de rendimiento para comparar con la versión GPU.
//!
#include <string.h>
#include "matmul_cpu.h"

/// Multiplica A×Zin y guarda el resultado en Zout. Zin y Zout pueden apuntar a la misma matriz.
///
/// Argumentos:
/// - A: matriz de entrada de tamaño m×k.
/// - m: número de filas de A y Zout.
/// - n: número de columnas de Zin y Zout.
/// - Zin: matriz de entrada de tamaño k×n.
/// - Zout: matriz de salida de tamaño m×n.
///
void matmul_cpu(float **A, int m, int n, float **Zin, float **Zout) {

    // Zout puede tener basura en memoria, se inicializa en 0 para que += acumule correctamente.
    for (int i = 0; i < m; i++) {
        memset(Zout[i], 0, n * sizeof(float));
    }

    // El "hola mundo" de la multiplicación de matrices.
    for (int i = 0; i < m; i++) 
        for (int k_ = 0; k_ < m; k_++)
            for (int j = 0; j < n; j++)
                Zout[i][j] += A[i][k_] * Zin[k_][j];
}