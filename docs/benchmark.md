# Punto de entrada y bucle principal (`src/cpu/main.c`, `src/cpu/benchmark.c`)

## `main.c` — punto de entrada

### Argumento de línea de comandos para MATDIR

El programa recibe el directorio de matrices como `argv[1]`, con `"data"` como default si se omite:

```c
const char *matrices_dir = argc > 1 ? argv[1] : "data";
```

El Makefile lo invoca directamente:

```makefile
run: $(BIN)
    ./$(BIN) $(MATDIR)
```

Esto permite ejecutar el binario directamente (`./build/bench_O0 data`) sin necesitar un pipe.

### Directorio de salida con timestamp

Cada ejecución crea un directorio propio con el nombre `cpu_{exp}_MMDD_HHMMSS/`. El timestamp evita que dos ejecuciones consecutivas se sobreescriban entre sí, lo que es útil al hacer un barrido `for EXP in $(seq 10 20)`. El nombre del directorio creado se guarda en `.last_outdir` para que el target `pg` del Makefile pueda colocar el reporte de gprof junto a las métricas del mismo run sin necesidad de buscarlo.

## `benchmark.c` — lógica central

### Decisión in-RAM vs. bloques

```c
if (matrices_caben_en_ram(p.m, p.n, ram_kb, ram_pct))
    ejecutar_bucle(A, Z, p, resultado, outdir, &m);
else
    ejecutar_bucle_bloques(matrices_dir, p, resultado, outdir, &m, BS);
```

Esta bifurcación ocurre en tiempo de ejecución basándose en la RAM disponible en ese momento. El motivo es que el mismo binario debe funcionar tanto en máquinas con poca RAM (laptops con EXP grande) como con mucha (servidores). Compilar modos separados obligaría al usuario a elegir manualmente y arriesgaría un OOM silencioso.

### El bucle en RAM (`ejecutar_bucle`)

Antes del bucle se pre-asignan dos buffers: `Z_alt` (segundo buffer m×n) y `snap` (buffer n×n de snapshot). Ambos se reutilizan en todas las iteraciones, eliminando todos los mallocs del camino caliente.

Cada iteración:

1. Multiplica `A × Z_cur` → `Z_nxt` usando `multiplicar_en` (escribe en buffer pre-asignado).
2. Mide tiempo de pared y CPU en orden simétrico: `t0 → cpu_antes → trabajo → cpu_despues → wall`.
3. Copia las primeras `n` filas de `Z_nxt` en `snap` con `copiar_snapshot_en`.
4. Guarda `snap` en disco como `resultado_N.txt`.
5. Lee RSS.
6. Intercambia punteros `Z_cur ↔ Z_nxt` (swap O(1), sin copias).

Al terminar, `benchmark()` libera explícitamente tanto `A` como `Z` (el original cargado desde disco). `Z_alt` y `snap` se liberan al final de `ejecutar_bucle`.

### Medición de tiempo de pared y CPU%

Se usa `CLOCK_MONOTONIC` en POSIX y `QueryPerformanceCounter` en Windows. Ambos son relojes monotónicos de alta resolución que no se ven afectados por ajustes de NTP ni cambios de zona horaria.

El orden de captura es simétrico: `t0 = now()` y `cpu_antes = leer_cpu()` antes del trabajo, `cpu_despues = leer_cpu()` y `wall = now() - t0` después. De esta manera, ambas ventanas de medición rodean exactamente el mismo intervalo de trabajo.

### Snapshots como matrices `n×n`

El resultado completo de `A × Z` es `m×n`, pero solo se guardan las primeras `n` filas (un bloque `n×n`). Esto reduce el almacenamiento en disco de O(m·n) a O(n²) por iteración. Dado que n=128 y l=2m/n puede llegar a miles de iteraciones para EXP grande, la diferencia es sustancial. El bloque capturado es suficiente para verificar la convergencia numérica.
