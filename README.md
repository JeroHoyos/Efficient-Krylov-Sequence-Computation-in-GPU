# Cómputo Eficiente de Secuencias de Krylov en GPU

> Benchmarking de multiplicación iterativa de matrices para generación de subespacios de Krylov — desde CPU base hasta CUDA.

> [Guía Rápida de la ejecución](#inicio-rápido)

![C](https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white)
![CUDA](https://img.shields.io/badge/CUDA-76B900?style=flat&logo=nvidia&logoColor=white)
![Estado](https://img.shields.io/badge/estado-en%20progreso-yellow)

---

## Tabla de Contenidos

1. [Subespacios de Krylov](#subespacios-de-krylov)
2. [Algoritmo](#algoritmo)
3. [Estructura del Proyecto](#estructura-del-proyecto)
4. [Implementación CPU](#implementación-cpu)
5. [Modo Sin RAM Suficiente](#modo-sin-ram-suficiente)
6. [Formato de Matrices](#formato-de-matrices)
7. [Implementación GPU](#implementación-gpu)
8. [Inicio Rápido](#inicio-rápido)

---

## Subespacios de Krylov

Un **subespacio de Krylov** de orden *r* generado por una matriz *m×m* *A* y un vector *z* es el subespacio vectorial generado por los productos matriz-vector sucesivos:

$$\mathcal{K}_l(A, z) = \text{span}\{z,\ Az,\ \ldots,\ A^{l-1}z\}$$

Los subespacios de Krylov son la base de los solvers iterativos para sistemas lineales grandes y problemas de autovalores (GMRES, Lanczos, Arnoldi). Su cómputo eficiente es crítico cuando *m* es grande, lo que los convierte en un objetivo ideal para su optimización.

Para saber más, revisar la investigación que se hizo para el proyecto en <a href="docs/research.md">Ver investigación</a>

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Algoritmo

El benchmark computa una **secuencia de Krylov por bloques** que reemplaza el vector inicial por una matriz *Z*.

### Parámetros

| Símbolo | Descripción | Valor |
| ------- | ----------- | ----- |
| `m` | Tamaño del problema (dimensión de la matriz) | 2¹⁰ ≤ m ≤ 2²⁰ |
| `n` | Tamaño del bloque (constante) | 128 |
| `A` | Matriz cuadrada (*m × m*, float) | aleatoria row-estocástica |
| `Z` | Matriz de bloque (*m × n*, float) | aleatoria |
| `l` | Número de iteraciones | `l = 2m/n` |

### Bucle

Para `i = 0, 1, ..., l − 1`:

1. Calcular `A · Z` (multiplicación de matrices, *m×m* × *m×n*)
2. Guardar las primeras `n` filas del resultado como snapshot de la iteración `i`
3. Actualizar `Z ← A · Z` para la siguiente iteración

El resultado es una secuencia de *l* snapshots de tamaño *n×n*..

Para saber con detalle cómo es la inicialización de las matrices, revisar en <a href="docs/generador.md.md">Ver documentación</a>

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Estructura del Proyecto

WIP

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Implementación CPU

La versión CPU está escrita en **C puro sin optimizaciones de compilador** (`-O0`). Su propósito es establecer una línea base de rendimiento: salida correcta, código directo, sin SIMD, sin paralelismo, sin ajuste de caché.

Las matrices se generan una vez y se guardan en disco mediante `gen_matrices`, y luego son leídas por el binario del benchmark. Esta separación permite que diferentes implementaciones (CPU, GPU) trabajen con exactamente las mismas entradas.

### Compilar y Ejecutar

El flujo de trabajo típico es:

```bash
# 1. Generar matrices para un exponente dado (m = 2^EXP)
make gen EXP=10          # escribe data/A.bin, data/Z.bin, data/params.txt

# 2. Ejecutar el benchmark con las matrices generadas
make run                 # lee desde data/ por defecto
make run MATDIR=data     # ídem, con ruta explícita

# Otros targets
make clean               # elimina build/, gmon.out, .last_outdir
make clean-runs          # elimina todos los directorios de reportes generados (cpu_*/)
make clean-matrices      # elimina el directorio data/ (incluyendo cachés .bin)
```

### Profiling con gprof

El target `pg` compila con `-pg` y ejecuta `gprof` automáticamente para generar un perfil plano:

```bash
make gen EXP=10          # generar matrices primero
make pg                  # compilar con -pg, ejecutar y guardar el reporte de gprof
```

El perfil se guarda junto a las otras métricas como `metricas_pg.txt` en el directorio del run. El perfil plano muestra la distribución del tiempo entre funciones — para la línea base CPU, ~100 % del tiempo de pared cae en `multiplicar_matrices`.

### Barrido EXP 10 → 20

```bash
for EXP in $(seq 10 20); do make gen EXP=$EXP && make run; done
```

### Salida

Cada ejecución crea un directorio con timestamp `cpu_{exp}_MMDD_HHMMSS/` que contiene:

| Archivo | Contenido |
| ------- | --------- |
| `resultado_N.txt` | Snapshot de las primeras *n* filas de *Z* tras la iteración *N* |
| `metricas.txt` | Reporte completo de métricas (ver abajo) |
| `metricas_pg.txt` | Perfil plano + grafo de llamadas de gprof *(solo para runs con `make pg`)* |

#### Secciones del reporte de métricas

```
[SISTEMA]       Modelo de CPU, núcleos lógicos, RAM total/disponible, SO, kernel, arquitectura
[CONFIGURACION] Exponente, dimensiones de matrices, tamaño de bloque, número de iteraciones
[MEMORIA]       Tamaño de A y Z en bytes, pico RSS, fallos de página menores/mayores
[TIEMPOS]       Total, promedio, mínimo, máximo y desviación estándar del tiempo de pared por iteración
[DETALLE]       Tabla por iteración: tiempo de pared (s), uso de CPU (%), memoria RSS (MB)
```

Ejecución de ejemplo (`EXP=10`, Intel Core i3-1005G1, 3.2 GB RAM):

```
  Total              : 6.265240 s
  Promedio / iter    : 0.383529 s
  Minimo             : 0.374585 s  (iter  9)
  Maximo             : 0.414318 s  (iter  2)
  Desv. estandar     : 0.010402 s
  Pico RSS           : 9260 KB (9.04 MB)
```

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Modo Sin RAM Suficiente

Para exponentes grandes (p. ej. EXP ≥ 16, donde solo A ocupa 16 GB), las matrices ya no caben en RAM. El benchmark lo detecta automáticamente y cambia a un **modo de multiplicación por bloques** que transmite datos desde disco.

### Cómo funciona

Al inicio, el benchmark lee `MemAvailable` de `/proc/meminfo` y lo compara con la memoria necesaria para mantener A, Z y el resultado simultáneamente. Si las matrices caben, usa la ruta normal en RAM. Si no:

1. **Cómputo por bloques** — A nunca se carga completamente. En su lugar, `A.bin` y `Z.bin` se leen en bloques cuadrados de tamaño *BS×BS*, y el producto se acumula en una franja del resultado que sí cabe en RAM. Dos archivos Z (`Z.bin` y `Z_new.bin`) se alternan en cada iteración para evitar leer y escribir el mismo archivo simultáneamente.

Dado que el generador escribe en binario directamente (ver [Formato de Matrices](#formato-de-matrices)), no se necesita ningún paso de conversión al ejecutar el benchmark.

### Fórmula del tamaño de bloque

$$BS = \left\lfloor \frac{\sqrt{\text{RAM}_\text{disponible} \times p / 24}}{64} \right\rfloor \times 64$$

donde `p` es la fracción `RAM_PCT` (por defecto 0.50). La fórmula mantiene tres bloques *BS×BS* en memoria a la vez (un bloque de A, uno de Z, una franja de acumulación), con un factor conservador para el overhead de alineación. BS siempre se redondea hacia abajo al múltiplo de 64 más cercano.

### Controlando RAM_PCT

`RAM_PCT` es un parámetro de tiempo de compilación (por defecto 50). Se pasa a `make` para cambiar el porcentaje de RAM disponible que puede usar el cálculo del tamaño de bloque:

```bash
make run RAM_PCT=25          # usar como máximo el 25% de la RAM disponible por bloque
make run RAM_PCT=75          # bloques más grandes, menos pasadas de E/S
make run RAM_PCT=0           # forzar modo bloques sin importar el tamaño de las matrices (pruebas)
```

Cambiar `RAM_PCT` requiere recompilar. Ejecutar `make clean` primero si el binario ya existe.

### Notas

- El modo fuera de RAM produce **resultados bit a bit idénticos** al modo en RAM: la descomposición por bloques es matemáticamente equivalente al producto matricial completo.
- Esta función es exclusiva de Linux; en otras plataformas `/proc/meminfo` no está disponible y el benchmark siempre usa la ruta en RAM.

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Formato de Matrices

Todas las matrices se almacenan como **float32 crudos en orden row-major** — sin cabeceras, sin delimitadores. El elemento `[i][j]` de una matriz con `cols` columnas está en el desplazamiento de byte `(i * cols + j) * 4`. Esto permite a `fseek` saltar a cualquier bloque en O(1), lo que hace posible el acceso eficiente fuera de RAM.

El generador (`make gen`) escribe `A.bin` y `Z.bin` **fila a fila**, reservando solo una fila a la vez independientemente del tamaño de la matriz. Una matriz de 4 TB (EXP=20) se genera con un working set constante de ~4 MB.

Tanto la ruta en RAM como la de bloques leen los mismos archivos `.bin`, por lo que no hay conversión de formato en ningún punto del pipeline.

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Implementación GPU

> **Próximamente.** La versión GPU usará **NVIDIA CUDA**.

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Inicio Rápido

### Prerrequisitos

#### CPU

- GCC (Linux) o MinGW-w64 (Windows)
- GNU Make

#### GPU *(cuando esté disponible)*

- GPU NVIDIA con CUDA Toolkit ≥ 11

### Clonar

```bash
git clone https://github.com/<your-username>/efficient-krylov-sequence-computation-in-gpu.git
cd efficient-krylov-sequence-computation-in-gpu
```

### Ejecutar benchmark CPU

```bash
make gen EXP=10   # generar matrices de entrada (m = 1024)
make run          # ejecutar benchmark (selecciona automáticamente modo en RAM o bloques)
```

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

*Proyecto de Arquitectura de Computadores y Computación de Alto Rendimiento.*
