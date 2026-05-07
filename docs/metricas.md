# Recolección de métricas y generación de informes (`src/cpu/metricas.c`)

## Qué se mide y por qué

El benchmark recolecta tres series temporales, una por iteración:

| Campo | Qué mide | Por qué es útil |
|-------|----------|-----------------|
| `tiempos_iter[i]` | Tiempo de pared de la iteración i | Detecta varianza entre iteraciones (efectos de caché frío, paginación) |
| `cpu_iter[i]` | Porcentaje de CPU usado en la iteración i | Indica si el proceso está limitado por cómputo o por I/O/memoria |
| `rss_iter[i]` | RSS (Resident Set Size) al terminar la iteración i | Muestra si el uso de memoria crece, se estabiliza o varía |

Además se miden acumulados del ciclo completo:
- `tiempo_total`: tiempo de pared desde la primera hasta la última iteración.
- `pico_mem_kb`: el RSS máximo observado durante todo el benchmark.
- `fallos_pagina_menor` / `fallos_pagina_mayor`: contador de page faults del bucle completo. Los fallos mayores (que requieren I/O de disco) son especialmente relevantes en el modo bloques.

## Cómo se calcula CPU%

```c
long long cpu_antes  = leer_cpu_us_proceso();
double t0 = tiempo_actual();
/* ... iteración ... */
double wall = tiempo_actual() - t0;
long long cpu_despues = leer_cpu_us_proceso();

double cpu_s = (double)(cpu_despues - cpu_antes) / 1e6;
double pct   = wall > 0.0 ? (cpu_s / wall) * 100.0 : 0.0;
```

`cpu_s` es el tiempo de CPU consumido por el proceso (modo usuario + kernel) en segundos. `pct = cpu_s / wall * 100` da el porcentaje real de CPU: si el proceso usa un solo núcleo sin bloquearse, debería ser ~100%. Si espera I/O (modo bloques con disco lento), será < 100%. Si el sistema lo desaloja, puede bajar también.

El valor se satura en 100% (`pct > 100.0 ? 100.0 : pct`) porque en multithreading el CPU time puede superar el wall time, pero este benchmark es single-thread así que en la práctica no ocurre.

## Fallos de página: delta en lugar de acumulado

```c
long minflt_ini = leer_minflt();
long majflt_ini = leer_majflt();
/* ... bucle ... */
m->fallos_pagina_menor = leer_minflt() - minflt_ini;
m->fallos_pagina_mayor = leer_majflt() - majflt_ini;
```

Se captura la diferencia antes/después del bucle porque `getrusage` retorna contadores acumulados desde el inicio del proceso, no solo del benchmark. Sin el delta, los fallos de la carga inicial de matrices contaminarían los números.

## Estructura `Metricas`

Los arrays `tiempos_iter`, `rss_iter` y `cpu_iter` se reservan con `calloc` en `metricas_init` con tamaño `l` (número de iteraciones). Usar `calloc` en lugar de `malloc` garantiza que los campos estén en cero antes de ser escritos, lo que evita que un valor basura aparezca en el informe si una iteración fallara.

## Generación del informe

La función `escribir_metricas` es compartida por `metricas_imprimir` (escribe a stdout) y `metricas_guardar` (escribe a archivo). Esto garantiza que lo que se ve en pantalla y lo que queda en disco sean idénticos: no hay dos rutas de formateo que puedan desincronizarse.

El informe tiene cinco secciones:

```
[SISTEMA]       CPU modelo, núcleos, RAM total/disponible, SO, kernel, arquitectura
[CONFIGURACION] EXP, dimensiones de A y Z, n, l
[MEMORIA]       Tamaño de A y Z en bytes, pico RSS, fallos de página
[TIEMPOS]       Total, promedio, mínimo, máximo, desviación estándar
[DETALLE]       Tabla por iteración: tiempo, CPU%, RSS
```

La información de sistema se obtiene en tiempo de ejecución (no de constantes de compilación) para que el informe sea reproducible en otra máquina: el modelo de CPU y la RAM disponible quedan registrados junto a los tiempos, facilitando comparaciones entre plataformas.

## GFLOPS y ancho de banda

El informe incluye dos métricas de rendimiento computacional calculadas en `escribir_metricas`:

```c
double flops_iter    = 2.0 * m * m * n;           // 2 ops por multiply-add
double gflops_prom   = flops_iter / (tiempo_promedio * 1e9);

double bytes_iter    = tam_A_bytes + 2 * tam_Z_bytes; // leer A + leer Z + escribir Z
double bandwidth_prom = bytes_iter / (tiempo_promedio * 1e9);
```

El factor `2` en `flops_iter` cuenta una multiplicación y una adición por cada elemento del producto interno. El ancho de banda teórico asume que A se lee completa en cada iteración (cierto en modo en RAM) y que Z se lee y se escribe una vez.

Estas cifras permiten comparar la línea base contra implementaciones futuras (versión `-O2`, BLAS, CUDA) usando las mismas métricas estándar de HPC.

## Desviación estándar muestral

```c
double desv = l > 1 ? sqrt(var / (l - 1)) : 0.0;
```

Se usa la varianza muestral (divisor `l-1`, corrección de Bessel) en lugar de la poblacional (`l`). Con pocas iteraciones (EXP pequeño, `l` pequeño), el estimador muestral es más honesto sobre la incertidumbre en el tiempo promedio.
