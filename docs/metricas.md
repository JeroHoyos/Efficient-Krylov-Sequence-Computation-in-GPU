# Metricas del benchmark

El benchmark genera un unico archivo de metricas por ejecucion:

```text
<outdir>/metricas.csv
```

`<outdir>` es el directorio creado por `main.c`, por ejemplo `cpu_12_0515_103109/` o `gpu_12_0515_103109/`.

## Formato del CSV

Columnas:

| Columna | Significado |
|---------|-------------|
| `tipo` | Tipo de fila. Puede ser `iteracion`, `promedio`, `min`, `max` o `total`. |
| `iter` | Numero de iteracion. Solo aplica cuando `tipo=iteracion`. |
| `tiempo_ms` | Tiempo medido en milisegundos. En filas `iteracion` es el tiempo de una multiplicacion. En la fila `total` es el tiempo total del benchmark. |
| `gflops` | Rendimiento efectivo de la multiplicacion en GFLOPs. Solo aplica a filas de iteracion y resumen. |
| `gbps_estimado` | Ancho de banda algoritmico estimado en GB/s. Solo aplica a filas de iteracion y resumen. |

Ejemplo:

```csv
tipo,iter,tiempo_ms,gflops,gbps_estimado
iteracion,0,4.813152,892.339872,3569.795203
iteracion,1,4.750080,904.188392,3617.195067
promedio,,4.781616,898.264132,3593.495135
min,,4.750080,892.339872,3569.795203
max,,4.813152,904.188392,3617.195067
total,,312.000000,,
```

## Filas del CSV

`iteracion`: una fila por multiplicacion `A x Z_cur -> Z_nxt`.

`promedio`: promedio de todas las filas `iteracion`.

`min`: minimo observado en las filas `iteracion`.

`max`: maximo observado en las filas `iteracion`.

`total`: tiempo total del benchmark. Incluye carga de matrices, reserva/copias iniciales, todas las iteraciones, copia de snapshots y escritura de resultados. No incluye la escritura final del propio CSV de metricas ni la liberacion de memoria.

## Definicion de las metricas

`tiempo_ms`:

En CPU mide la llamada a `multiplicar_en`. En GPU mide el kernel `gpu_multiplicar` con eventos CUDA. La fila `total` usa el temporizador general del benchmark y por eso captura mas trabajo que solo la multiplicacion.

`gflops`:

```text
gflops = flops_por_iter / (tiempo_ms / 1000) / 1e9
```

Para este producto `A[m x m] * Z[m x n]`, el benchmark usa:

```text
flops_por_iter = 2 * m * m * n
```

`gbps_estimado`:

```text
gbps_estimado = bytes_algoritmicos_por_iter / (tiempo_ms / 1000) / 1e9
```

El conteo de bytes es algoritmico, no un contador real de hardware:

```text
bytes_algoritmicos_por_iter = (2 * m * m * n + m * n) * sizeof(float)
```

Esto sirve para comparar ejecuciones del mismo benchmark, pero no reemplaza una medicion de trafico real de memoria.

## Lo que no se guarda en el CSV

El CSV evita metricas estaticas como nombre de GPU, cantidad de SMs, compute capability o cantidad de nucleos CPU. Esos datos no cambian durante una ejecucion y no describen uso.

Tampoco se guardan metricas como uso real de SMs, actividad de warps, stalls, cache hit rate, ocupacion real o uso real de nucleos CPU. Esas metricas no se pueden medir de forma fiable con el runtime normal de CUDA ni con el temporizador C usado por el benchmark. Para obtenerlas hay que perfilar la ejecucion con herramientas externas como Nsight Compute, Nsight Systems, CUPTI, Windows Performance Recorder o Linux `perf`.
