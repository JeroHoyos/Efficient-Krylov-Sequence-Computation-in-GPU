# Multiplicación por bloques fuera de RAM (`src/cpu/block_mul.c`)

## Problema que resuelve

Para EXP ≥ 16 la matriz A ocupa 16 GB o más. Cargarla completa en RAM causaría un OOM. El modo bloques calcula el mismo producto `A × Z` sin cargar nunca A completa: la divide en tiles de BS×BS y los procesa uno a uno.

## Estructura del algoritmo tiled

El producto `C[m×n] = A[m×m] × Z[m×n]` se descompone como:

```
para cada franja de filas i0..i0+BS de C:
    C_strip = 0   (franja BS×n en RAM)
    para cada bloque k0..k0+BS (columnas de A / filas de Z):
        Ab = A[i0..i0+BS, k0..k0+BS]    (leído de disco)
        Zb = Z[k0..k0+BS, 0..n]         (leído de disco)
        C_strip += Ab × Zb               (acumulación parcial)
    escribir C_strip en disco
```

La identidad algebraica que lo justifica es la descomposición por bloques de la multiplicación de matrices:

```
C[i,j] = Σ_k A[i,k] * Z[k,j]
```

Procesando los índices k en grupos de BS se obtiene exactamente el mismo resultado que procesarlos todos a la vez. No hay aproximación.

## Lectura de subbloques con `fseek`

```c
static float **leer_bloque_bin(FILE *f, int fila0, int nf, int col0, int nc, int ncols_total) {
    for (int i = 0; i < nf; i++) {
        long off = ((long)(fila0 + i) * ncols_total + col0) * (long)sizeof(float);
        fseek(f, off, SEEK_SET);
        fread(bloque[i], sizeof(float), nc, f);
    }
}
```

El formato binario raw (sin cabeceras, row-major) hace que la posición de cualquier elemento sea calculable en O(1): `offset = (fila * total_cols + col) * 4`. Esto permite saltar directamente a cualquier tile sin leer los datos intermedios. Un formato con cabeceras o compresión requeriría lectura secuencial o una tabla de índices.

## Bucle interno i→k→j (orden óptimo para caché)

```c
static void mul_acumular(float **C, float **A, float **Z, int ni, int nk, int n) {
    for (int i = 0; i < ni; i++)
        for (int k = 0; k < nk; k++) {
            float aik = A[i][k];          // cargado una vez por fila k
            for (int j = 0; j < n; j++)
                C[i][j] += aik * Z[k][j]; // Z[k] y C[i] se acceden secuencialmente
        }
}
```

El orden i→k→j carga `A[i][k]` en un registro y lo multiplica por toda la fila `Z[k]`, acumulando en `C[i]`. Tanto `Z[k]` como `C[i]` se acceden de forma contigua en memoria, lo que maximiza los hits de caché para los datos que sí están en RAM.

Esto contrasta con la multiplicación naive de `matrices.c` (orden i→j→k) que accede `Z` por columnas. En el modo bloques, la acumulación ocurre sobre buffers pequeños (BS×BS en lugar de m×m), por lo que el orden importa más: caben en L2/L3.

## Alternancia de archivos Z

```c
char *z_cur = z_a;   // apunta a "data/Z.bin"
char *z_nxt = z_b;   // apunta a "data/Z_new.bin"

for (int iter = 0; iter < p.l; iter++) {
    multiplicar_bloques(ruta_A, z_cur, z_nxt, ...);
    char *tmp = z_cur; z_cur = z_nxt; z_nxt = tmp;
}
```

El resultado de cada iteración (`z_nxt`) se convierte en la entrada de la siguiente (`z_cur`). Usar dos archivos distintos evita el problema de leer y escribir simultáneamente el mismo archivo, lo que produciría resultados incorrectos (se leería un Z que ya fue parcialmente sobreescrito).

Los punteros `z_cur` y `z_nxt` se intercambian en cada iteración mediante swap de punteros: no se copian ni se renombran los archivos, solo se rota qué rol cumple cada nombre.

## Captura de snapshot durante la escritura

```c
if (snap_restante > 0 && snap_out) {
    int fs = (snap_restante < ni) ? snap_restante : ni;
    for (int r = 0; r < fs; r++)
        memcpy(snap_out[i0 + r], C_strip[r], n * sizeof(float));
    snap_restante -= fs;
}
```

El snapshot (primeras `n` filas del resultado) se captura mientras la franja de C aún está en RAM, inmediatamente antes de escribirla a disco. Esto evita una segunda pasada de lectura para extraer el snapshot: se paga el costo de la copia (`n×n` floats) solo para las primeras `n/BS` franjas.

## Escritura secuencial

La escritura de `fC` no usa `fseek` porque las franjas se generan en orden creciente de `i0`. La escritura secuencial es la más eficiente para HDD y SSD: no fuerza seeks innecesarios y permite que el kernel optimice con write-ahead buffering.
