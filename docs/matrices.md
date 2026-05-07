# E/S de matrices, detección de RAM y tamaño de bloque (`src/cpu/matrices.c`)

## Representación en memoria

Las matrices se representan como `float **`: un arreglo de punteros a filas, donde cada fila es un arreglo contiguo de floats. Esta representación permite acceder a `A[i][j]` directamente y liberar fila a fila, pero sacrifica la contigüidad total del bloque (importante para caché). Se eligió para simplicidad del código base ya que el objetivo es establecer una línea base sin optimizaciones, no maximizar rendimiento.

## Lectura de matrices binarias

```c
float **leer_matriz_bin(const char *ruta, int filas, int cols) {
    FILE *f = fopen(ruta, "rb");
    float **mat = malloc(filas * sizeof(float *));
    for (int i = 0; i < filas; i++) {
        mat[i] = malloc(cols * sizeof(float));
        fread(mat[i], sizeof(float), cols, f);
    }
    ...
}
```

Se lee fila a fila usando `fread` de `cols` floats de una vez. Esto es más eficiente que leer elemento a elemento porque aprovecha el buffering del sistema de archivos y las llamadas al sistema son O(filas) en lugar de O(filas·cols).

## Detección de RAM disponible

```c
long long leer_ram_disponible_kb(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    // busca "MemAvailable: N kB"
}
```

Se usa `MemAvailable` (no `MemFree`) porque el kernel puede recuperar memoria de cachés de página para asignaciones nuevas. `MemFree` subestimaría lo que el proceso puede realmente usar, causando que el modo bloques se active innecesariamente. En plataformas que no son Linux, la función retorna -1 y el código asume que las matrices caben (comportamiento conservador que evita fallar en Windows donde `/proc/meminfo` no existe).

## Decisión de carga completa o bloques

```c
int matrices_caben_en_ram(int m, int n, long long ram_kb, double pct) {
    long long needed = (long long)m * m * sizeof(float)   // A completa
                     + 2LL * m * n * sizeof(float);        // Z actual + resultado
    long long avail  = (long long)((double)(ram_kb * 1024LL) * pct);
    return needed <= avail;
}
```

El factor `pct` (`RAM_PCT/100`, por defecto 0.5) actúa como margen de seguridad: reserva la mitad de la RAM disponible para las matrices, dejando el resto para el SO, otros procesos y el overhead del propio programa. Usar el 100% de `MemAvailable` causaría OOM bajo carga del sistema.

Porque `RAM_PCT` es un parámetro de tiempo de compilación (`-DRAM_PCT=$(RAM_PCT)`), el binario no necesita parsear argumentos en tiempo de ejecución para un parámetro que raramente cambia. Si se quiere forzar el modo bloques para pruebas, se puede compilar con `RAM_PCT=0`.

## Cálculo del tamaño de bloque

```c
int calcular_block_size(long long ram_kb, double pct) {
    double bs = sqrt((double)(ram_kb * 1024LL) * pct / 24.0);
    int BS = (int)(bs / 64.0) * 64;
    return BS < 64 ? 64 : BS;
}
```

### Derivación del factor `/24`

En el modo bloques hay tres buffers activos simultáneamente, todos de tamaño BS×BS floats:
- Un bloque de A: `BS × BS × 4 bytes`
- Un bloque de Z: `BS × BS × 4 bytes`
- Una franja de acumulación C: `BS × BS × 4 bytes`

Total = `3 × BS² × 4 = 12 × BS²` bytes.

Queremos que quepan dentro de `RAM disponible × pct`, entonces:

```
12 × BS² ≤ ram_bytes × pct
BS² ≤ ram_bytes × pct / 12
BS ≤ sqrt(ram_bytes × pct / 12)
```

En el código aparece `/24` en lugar de `/12` porque hay un factor 2 adicional de conservadurismo para el overhead de alineación, las estructuras de metadatos del SO y el heap del proceso. En la práctica este factor extra evita fallos de asignación en máquinas con poca RAM.

### Alineación a múltiplos de 64

`BS` se redondea hacia abajo al múltiplo de 64 más cercano. Esto alinea el inicio de cada bloque a la línea de caché (64 bytes = una línea de caché en x86), lo que mejora la localidad de acceso aunque en la línea base `-O0` el impacto es menor. La alineación se vuelve importante para versiones optimizadas futuras.

## Multiplicación naive sin alloc (`multiplicar_en`)

```c
void multiplicar_en(float **A, int filas_A, int cols_A, float **B, int cols_B, float **C) {
    for (int i = 0; i < filas_A; i++)
        for (int j = 0; j < cols_B; j++) {
            float suma = 0.0f;
            for (int k = 0; k < cols_A; k++)
                suma += A[i][k] * B[k][j];
            C[i][j] = suma;
        }
}
```

La función escribe el resultado en `C` pre-asignado por el llamador, eliminando los `m` mallocs por iteración que tenía la versión anterior. El orden i→j→k accede B por columnas (no óptimo para caché), lo cual es intencional para la línea base sin optimizaciones. Una versión `-O2` reordenaría a i→k→j.

## `copiar_snapshot_en`

```c
void copiar_snapshot_en(float **src, int n, float **dst) {
    for (int i = 0; i < n; i++)
        memcpy(dst[i], src[i], n * sizeof(float));
}
```

Copia las primeras `n` filas de `src` (que tiene `m` filas) en `dst` (pre-asignado como n×n). Usar `memcpy` de filas enteras es más eficiente que copiar elemento a elemento porque el compilador y la libc pueden vectorizarlo incluso con `-O0`.

## Nota sobre la representación `float **`

La representación de matrices como `float **` (array de punteros a filas) es una **decisión explícita de la línea base**: facilita el código sin optimizaciones pero impide usar `cblas_sgemm` u otros kernels optimizados que requieren arrays contiguos (`float *` con acceso `[i*cols+j]`). La migración a array plano es el primer paso necesario para cualquier versión optimizada futura.
