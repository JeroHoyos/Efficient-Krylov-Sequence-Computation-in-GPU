/**
 * @file    benchmark_matmul.c
 * @brief   Benchmark de multiplicación iterativa de matrices en punto flotante
 *          de doble precisión.
 *
 * @details Este programa genera dos matrices aleatorias A (m×m) y Z (m×n)
 *          con valores uniformes en [0.0, 1.0) y ejecuta l iteraciones de la
 *          operación Z_{k+1} = A · Z_k. En cada iteración se capturan las
 *          primeras n filas del resultado en una lista llamada `resultado`,
 *          formando una colección de l snapshots de tamaño n×n.
 *
 * @note    Parámetros derivados del exponente de entrada:
 *            m = 2^input   (dimensión cuadrada de A y filas de Z)
 *            n = 128       (columnas de Z, fijo)
 *            l = (2*m)/n   (número de iteraciones)
 *
 * @warning Con valores altos de `input` (cercanos a 14) los valores de las
 *          matrices crecen exponencialmente. Considere normalizar A o Z si
 *          los valores numéricos son relevantes para su caso de uso.
 *
 * @author  —
 * @date    2026
 *
 * Compilación:
 * @code
 *   gcc -O2 -o benchmark benchmark_matmul.c -lm
 * @endcode
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* =========================================================================
 * Estructuras
 * ========================================================================= */

/**
 * @brief Agrupa los parámetros principales del benchmark.
 */
typedef struct {
    int input;  /**< Exponente ingresado por el usuario.          */
    int m;      /**< Dimensión de A (m×m) y filas de Z (m×n).    */
    int n;      /**< Columnas de Z (fijo en 128).                 */
    int l;      /**< Número de iteraciones = (2·m)/n.             */
} Parametros;

/**
 * @brief Acumula los tiempos medidos durante el benchmark.
 */
typedef struct {
    double init;        /**< Tiempo de inicialización de A y Z.   */
    double total_mult;  /**< Suma de tiempos de multiplicación.    */
    double total_copia; /**< Suma de tiempos de copia a snapshots. */
    double total_ejec;  /**< Tiempo total del bucle de ejecución.  */
} Tiempos;

/* =========================================================================
 * Utilidades de tiempo
 * ========================================================================= */

/**
 * @brief  Devuelve el tiempo actual en segundos (alta resolución).
 * @return Tiempo en segundos como double.
 */
static double ahora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/**
 * @brief  Calcula el tiempo transcurrido entre dos marcas.
 *
 * @param  inicio  Marca de tiempo de inicio (segundos).
 * @param  fin     Marca de tiempo de fin (segundos).
 * @return Diferencia en segundos.
 */
static double elapsed(double inicio, double fin) {
    return fin - inicio;
}

/* =========================================================================
 * Gestión de memoria
 * ========================================================================= */

/**
 * @brief   Crea una matriz de doubles con valores aleatorios en [0.0, 1.0).
 *
 * @param   filas  Número de filas. Debe ser > 0.
 * @param   cols   Número de columnas. Debe ser > 0.
 * @return  Puntero doble a la matriz creada en heap.
 *          El llamador debe liberarla con liberar_matriz().
 */
double **crear_matriz(int filas, int cols) {
    double **matriz = malloc(filas * sizeof(double *));
    for (int i = 0; i < filas; i++) {
        matriz[i] = malloc(cols * sizeof(double));
        for (int j = 0; j < cols; j++)
            matriz[i][j] = (double)rand() / RAND_MAX;
    }
    return matriz;
}

/**
 * @brief  Libera la memoria dinámica de una matriz de doubles.
 *
 * @param  matriz  Puntero doble a la matriz. No debe ser NULL.
 * @param  filas   Número de filas con las que fue creada.
 */
void liberar_matriz(double **matriz, int filas) {
    for (int i = 0; i < filas; i++)
        free(matriz[i]);
    free(matriz);
}

/**
 * @brief  Libera todos los snapshots almacenados en `resultado`.
 *
 * @param  resultado  Arreglo de l matrices n×n.
 * @param  l          Número de snapshots.
 * @param  n          Filas de cada snapshot.
 */
static void liberar_resultado(double ***resultado, int l, int n) {
    for (int i = 0; i < l; i++)
        liberar_matriz(resultado[i], n);
    free(resultado);
}

/* =========================================================================
 * Álgebra lineal
 * ========================================================================= */

/**
 * @brief   Multiplica C = A · B con dimensiones (filas_a×cols_a) · (cols_a×cols_b).
 *
 * @param   A       Matriz izquierda.
 * @param   filas_a Filas de A (y de C).
 * @param   cols_a  Columnas de A = filas de B.
 * @param   B       Matriz derecha.
 * @param   cols_b  Columnas de B (y de C).
 * @return  Nueva matriz C (filas_a×cols_b) reservada en heap.
 *          El llamador debe liberarla con liberar_matriz().
 *
 * @pre     cols_a == filas de B. Ningún puntero puede ser NULL.
 */
double **multiplicar_matrices(double **A, int filas_a, int cols_a,
                               double **B, int cols_b) {
    double **C = malloc(filas_a * sizeof(double *));
    for (int i = 0; i < filas_a; i++) {
        C[i] = calloc(cols_b, sizeof(double));
        for (int j = 0; j < cols_b; j++)
            for (int k = 0; k < cols_a; k++)
                C[i][j] += A[i][k] * B[k][j];
    }
    return C;
}

/**
 * @brief  Copia las primeras `n` filas de una matriz m×n en un snapshot n×n.
 *
 * @param  src  Matriz fuente de dimensiones m×cols.
 * @param  n    Número de filas y columnas a copiar.
 * @return Nueva matriz n×n reservada en heap.
 *         El llamador debe liberarla con liberar_matriz().
 */
static double **copiar_snapshot(double **src, int n) {
    double **snap = malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) {
        snap[i] = malloc(n * sizeof(double));
        for (int j = 0; j < n; j++)
            snap[i][j] = src[i][j];
    }
    return snap;
}

/* =========================================================================
 * Impresión y diagnóstico
 * ========================================================================= */

/**
 * @brief  Imprime el tamaño en memoria de una matriz con unidades legibles.
 *
 * @param  nombre  Etiqueta de la matriz (p.ej. "A", "Z").
 * @param  filas   Número de filas.
 * @param  cols    Número de columnas.
 */
void print_tamano(const char *nombre, int filas, int cols) {
    long bytes = (long)filas * cols * sizeof(double);
    printf("  Matriz %s [%d x %d]: ", nombre, filas, cols);
    if (bytes >= 1024 * 1024)
        printf("%ld bytes (%.2f MB)\n", bytes, bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        printf("%ld bytes (%.2f KB)\n", bytes, bytes / 1024.0);
    else
        printf("%ld bytes\n", bytes);
}

/**
 * @brief  Imprime los parámetros calculados del benchmark.
 * @param  p  Estructura con los parámetros.
 */
static void print_parametros(Parametros p) {
    printf("\n=== Parametros ===\n");
    printf("  input : %d\n", p.input);
    printf("  m     : %d\n", p.m);
    printf("  n     : %d\n", p.n);
    printf("  l     : %d\n", p.l);
}

/**
 * @brief  Imprime el tamaño inicial de A y Z.
 * @param  p  Estructura con los parámetros.
 */
static void print_memoria_inicial(Parametros p) {
    printf("\n=== Memoria inicial ===\n");
    print_tamano("A", p.m, p.m);
    print_tamano("Z", p.m, p.n);
}

/**
 * @brief  Imprime el log de una iteración del benchmark.
 *
 * @param  iter                  Índice de la iteración (0-based).
 * @param  tiempo_mult           Tiempo de multiplicación en segundos.
 * @param  tiempo_copia          Tiempo de copia en segundos.
 * @param  n                     Dimensión del snapshot.
 * @param  bytes_acumulados      Bytes totales acumulados en resultado.
 */
static void print_iter(int iter, double tiempo_mult, double tiempo_copia,
                       int n, long bytes_acumulados) {
    (void)n;
    printf("  [Iter %3d] A^%d * Z  |  mult: %.4f s  copia: %.4f s  "
           "|  resultado: %ld entradas, %.2f KB acumulados\n",
           iter + 1, iter + 1,
           tiempo_mult, tiempo_copia,
           (long)(iter + 1),
           bytes_acumulados / 1024.0);
}

/**
 * @brief  Imprime el resumen final del benchmark.
 *
 * @param  p          Parámetros del benchmark.
 * @param  t          Tiempos acumulados.
 * @param  resultado  Arreglo de snapshots (para mostrar valores de muestra).
 * @param  bytes_tot  Bytes totales ocupados por resultado.
 */
static void print_resumen(Parametros p, Tiempos t,
                          double ***resultado, long bytes_tot) {
    printf("\n=== Resumen ===\n");
    printf("  Iteraciones realizadas : %d\n",   p.l);
    printf("  Tiempo inicializacion  : %.4f s\n", t.init);
    printf("  Tiempo total mult      : %.4f s  (promedio %.4f s/iter)\n",
           t.total_mult,  t.total_mult  / p.l);
    printf("  Tiempo total copia     : %.4f s  (promedio %.4f s/iter)\n",
           t.total_copia, t.total_copia / p.l);
    printf("  Tiempo total ejecucion : %.4f s\n", t.total_ejec);
    printf("  Memoria resultado      : %ld bytes (%.2f MB)  [%d matrices de %dx%d]\n",
           bytes_tot, bytes_tot / (1024.0 * 1024.0), p.l, p.n, p.n);
    printf("  resultado[0][0][0]     = %.6f\n",   resultado[0][0][0]);
    printf("  resultado[%d][0][0]  = %.6f\n", p.l - 1, resultado[p.l-1][0][0]);
}

/* =========================================================================
 * Lógica del benchmark
 * ========================================================================= */

/**
 * @brief  Inicializa A y Z y mide el tiempo de creación.
 *
 * @param  p       Parámetros del benchmark.
 * @param  out_A   Salida: matriz A (m×m).
 * @param  out_Z   Salida: matriz Z (m×n).
 * @return Tiempo de inicialización en segundos.
 */
static double inicializar_matrices(Parametros p,
                                   double ***out_A, double ***out_Z) {
    printf("\n=== Inicializacion ===\n");
    double t0 = ahora();
    *out_A = crear_matriz(p.m, p.m);
    *out_Z = crear_matriz(p.m, p.n);
    double t1 = ahora();

    print_tamano("A", p.m, p.m);
    print_tamano("Z", p.m, p.n);
    double tiempo = elapsed(t0, t1);
    printf("  Tiempo inicializacion: %.4f s\n", tiempo);
    return tiempo;
}

/**
 * @brief  Ejecuta una iteración: multiplica A·Z_actual, guarda snapshot.
 *
 * @param  A               Matriz cuadrada m×m.
 * @param  Z_actual        Matriz actual m×n (entrada de la multiplicación).
 * @param  p               Parámetros del benchmark.
 * @param  out_Z_nueva     Salida: resultado de A·Z_actual (m×n).
 * @param  out_snap        Salida: snapshot n×n de las primeras n filas.
 * @param  out_t_mult      Salida: tiempo de multiplicación en segundos.
 * @param  out_t_copia     Salida: tiempo de copia en segundos.
 */
static void ejecutar_iteracion(double **A, double **Z_actual, Parametros p,
                                double ***out_Z_nueva, double ***out_snap,
                                double *out_t_mult,   double *out_t_copia) {
    double t0, t1;

    t0 = ahora();
    *out_Z_nueva = multiplicar_matrices(A, p.m, p.m, Z_actual, p.n);
    t1 = ahora();
    *out_t_mult = elapsed(t0, t1);

    t0 = ahora();
    *out_snap = copiar_snapshot(*out_Z_nueva, p.n);
    t1 = ahora();
    *out_t_copia = elapsed(t0, t1);
}

/**
 * @brief  Bucle principal: itera l veces A^k·Z y llena resultado[].
 *
 * @param  A          Matriz A (m×m).
 * @param  Z          Matriz Z inicial (m×n).
 * @param  p          Parámetros del benchmark.
 * @param  resultado  Arreglo preallocado de l punteros para los snapshots.
 * @param  t          Estructura de tiempos a rellenar.
 * @return Bytes totales ocupados por resultado al finalizar.
 */
static long ejecutar_bucle(double **A, double **Z, Parametros p,
                            double ***resultado, Tiempos *t) {
    long bytes_acumulados = 0;
    double **Z_actual = Z;
    double **Z_nueva  = NULL;
    double t_mult, t_copia;

    printf("\n=== Ejecucion ===\n");
    double t0 = ahora();

    for (int iter = 0; iter < p.l; iter++) {
        ejecutar_iteracion(A, Z_actual, p,
                           &Z_nueva, &resultado[iter],
                           &t_mult, &t_copia);

        t->total_mult  += t_mult;
        t->total_copia += t_copia;
        bytes_acumulados += (long)p.n * p.n * sizeof(double);

        print_iter(iter, t_mult, t_copia, p.n, bytes_acumulados);

        if (Z_actual != Z)
            liberar_matriz(Z_actual, p.m);
        Z_actual = Z_nueva;
    }

    t->total_ejec = elapsed(t0, ahora());

    /* Liberar la última Z_nueva */
    liberar_matriz(Z_actual, p.m);
    return bytes_acumulados;
}

/**
 * @brief  Orquesta el benchmark completo: init → bucle → resumen → limpieza.
 * @param  p  Parámetros del benchmark.
 */
void benchmark(Parametros p) {
    Tiempos t = {0};
    double **A, **Z;

    t.init = inicializar_matrices(p, &A, &Z);

    double ***resultado = malloc(p.l * sizeof(double **));
    long bytes_tot = ejecutar_bucle(A, Z, p, resultado, &t);

    print_resumen(p, t, resultado, bytes_tot);

    liberar_resultado(resultado, p.l, p.n);
    liberar_matriz(Z, p.m);
    liberar_matriz(A, p.m);
}

/* =========================================================================
 * Punto de entrada
 * ========================================================================= */

/**
 * @brief  Lee el exponente, construye los parámetros y lanza el benchmark.
 * @return 0 si la ejecución fue exitosa, 1 si el exponente es inválido.
 */
int main(void) {
    int input;
    printf("Ingrese el exponente (max 14): ");
    scanf("%d", &input);

    if (input > 14) {
        printf("Error: input debe ser <= 14 para no desbordar int\n");
        return 1;
    }

    Parametros p = {
        .input = input,
        .m     = (int)pow(2, input),
        .n     = 128,
        .l     = 0
    };
    p.l = (2 * p.m) / p.n;

    print_parametros(p);
    print_memoria_inicial(p);
    benchmark(p);
    return 0;
}