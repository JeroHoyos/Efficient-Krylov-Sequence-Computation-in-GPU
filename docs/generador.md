# Generador de matrices (`src/common/`)

El generador produce las dos matrices de entrada `A` y `Z` y las persiste en disco antes de que corra el benchmark. 

## Diseño de las matrices

**La matriz A** se genera como una **matriz row-estocástica**: valores aleatorios en [0, 1] con cada fila normalizada para sumar 1. **Z** se inicializa con valores aleatorios en [0, 1].

La multiplicación repetida por una matriz row-estocástica es una iteración de cadena de Markov. Este proceso converja a un valor predecible se explica de forma natural al seguir el flujo de los datos mediante estos principios:

* **Ley de los Grandes Números:** Asegura que, al trabajar con volúmenes de datos tan grandes, el promedio de los valores aleatorios iniciales en $Z$ se concentrará casi sin desviación en **0.5**. Esto define que la "masa total" que el sistema va a repartir sea siempre proporcional a $m/2$.
* **Teorema de Perron-Frobenius:** Garantiza que, al multiplicar por la matriz estocástica, el sistema abandonará el caos inicial para estabilizarse en un estado de equilibrio único (distribución estacionaria), manteniendo siempre constante esa masa original de la columna.

Como la matriz $A$ es inmensa y aleatoria, en la práctica se comporta como una matriz doblemente estocástica, repartiendo la masa de forma uniforme entre las $m$ filas. Al estabilizarse el sistema, el valor de cada celda es simplemente el resultado de esta operación:

$$\frac{m/2}{m} = 0.5$$

Dado que m = 2^input es grande, la **ejecución converge a ≈ 0.5 independientemente de la semilla.**

## Por qué A es una matriz row-estocástica

Este comportamiento matemático es intencional para mantener el procesador trabajando de forma óptima durante la prueba:

| Convergencia | Efecto en el procesador |
| --- | --- |
| **→ ∞** | Desborda a `inf`/`NaN`. El procesador deja de hacer cálculos reales. |
| **→ 0** | Se rinde y los deja en 0. El procesador deja de hacer cálculos reales al no poder manejar valores tan pequeños. |
| **→ 0.5** | Los valores se mantienen en el rango normal. Al ser números ligeramente distintos, obligan al procesador a procesar variaciones reales constantemente, haciendo la prueba más realista. |

## Generación fila a fila

Tanto `gen_A_bin` como `gen_Z_bin` reservan un buffer de **una sola fila** y escriben fila a fila:

```c
float *fila = malloc(m * sizeof(float));
for (int i = 0; i < m; i++) {
    /* ... llenar fila ... */
    fwrite(fila, sizeof(float), m, f);
}
```

En lugar de intentar cargar toda la matriz, el programa la procesa fila por fila. En el caso extremo de EXP=20, la matriz completa pesaría unos colosales 4 TB, pero al cargar solo una fila a la vez, el consumo real de RAM se mantiene en apenas 4 MB.

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
A: m x m floats = <4*m MB>
Z: m x n floats = <4*m MB>
```

