# Generador de matrices (`src/common/`)

## Propósito

El generador produce las dos matrices de entrada —`A` y `Z`— y las persiste en disco antes de que corra el benchmark. Separarlos en binarios distintos permite que cualquier implementación futura (GPU, versión optimizada, etc.) trabaje exactamente con las mismas entradas sin necesidad de regenerarlas.

## Por qué matrices row-estocásticas

`A` se genera como una **matriz row-estocástica**: cada fila tiene valores aleatorios en [0,1] que se normalizan para que sumen exactamente 1. Esta propiedad es matemáticamente conveniente para el benchmark:

- Multiplicar repetidamente por una matriz estocástica es una iteración de cadena de Markov. Las columnas de Z convergen hacia la distribución estacionaria de A escalada por la suma inicial de cada columna.
- Para una matriz estocástica aleatoria grande, esa distribución es aproximadamente uniforme (~1/m por fila). Una columna aleatoria de longitud m suma ~m/2, por lo que el valor de convergencia es (m/2)/m = **0.5**.
- Ese valor intermedio garantiza que los floats nunca desborden a `inf`/`NaN` ni entren en el rango subnormal (donde la FPU de x86 usa microcódigo y es 10–100× más lenta). El benchmark mide ciclos de multiplicación, no comportamiento excepcional de la FPU.

## Generación fila a fila

Tanto `gen_A_bin` como `gen_Z_bin` reservan un buffer de **una sola fila** y escriben fila a fila:

```c
float *fila = malloc(m * sizeof(float));
for (int i = 0; i < m; i++) {
    /* ... llenar fila ... */
    fwrite(fila, sizeof(float), m, f);
}
```

Esto mantiene el working set constante en O(m) floats independientemente de cuánto mida la matriz completa. Para EXP=20 (m = 1 048 576), A ocupa ~4 TB — cargarla entera en memoria para generarla sería imposible. Escribiéndola fila a fila, el working set es siempre ~4 MB (una fila de floats).

## Formato binario raw

Los archivos resultantes son **float32 crudos en orden row-major** sin cabeceras. El elemento `[i][j]` está en el byte `(i * cols + j) * 4`. Este formato permite:

- Leer cualquier subbloque con un solo `fseek` en O(1), lo que hace posible el modo de multiplicación por bloques.
- No hay paso de conversión entre el generador y el benchmark: ambos hablan el mismo formato.

## `params.txt`

El generador escribe también un archivo de texto con los parámetros derivados:

```
input <EXP>
m <2^EXP>
n 128
l <2*m/n>
```

El benchmark lo lee al arrancar para conocer las dimensiones sin necesidad de que el usuario las reingrese. El valor `n = 128` es fijo (tamaño del bloque de Krylov); `l = 2m/n` da suficientes iteraciones para observar la convergencia.
