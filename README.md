# Cómputo Eficiente de Secuencias de Krylov en GPU

> Benchmarking de multiplicación iterativa de matrices para generación de subespacios de Krylov, comparativa entre CPU base y GPU con CUDA.

![C](https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white)
![CUDA](https://img.shields.io/badge/CUDA-76B900?style=flat&logo=nvidia&logoColor=white)
![Plataforma](https://img.shields.io/badge/plataforma-Windows-blue)

---

## Tabla de Contenidos

1. [Subespacios de Krylov](#subespacios-de-krylov)
2. [Algoritmo](#algoritmo)
3. [Implementación GPU](#implementación-gpu)
4. [Estructura del Proyecto](#estructura-del-proyecto)
5. [Formato de Matrices](#formato-de-matrices)
6. [Documentación](#documentación)
7. [Inicio Rápido](#inicio-rápido)

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
| `m` | Tamaño del problema (dimensión de la matriz) | 2¹² ≤ m ≤ 2¹⁶ |
| `n` | Tamaño del bloque (constante) | 128 |
| `A` | Matriz cuadrada (*m × m*, float) | aleatoria row-estocástica |
| `Z` | Matriz de bloque (*m × n*, float) | aleatoria |
| `l` | Número de iteraciones | `l = 2m/n` |

### Bucle

Para `i = 0, 1, ..., l − 1`:

1. Calcular `A · Z` (multiplicación de matrices, *m×m* × *m×n*)
2. Guardar las primeras `n` filas del resultado como snapshot de la iteración `i`
3. Actualizar `Z ← A · Z` para la siguiente iteración

El resultado es una secuencia de *l* snapshots de tamaño *n×n*.

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Implementación GPU

La implementación GPU en `src/gpu/matmul_gpu.cu` selecciona automáticamente el modo de ejecución según la VRAM disponible en tiempo de inicialización.

### Modo FULL

Si `A + Z_in + Z_out + snapshot` caben en el 85 % de la VRAM libre, se asignan todas las matrices directamente en la GPU y cada iteración lanza un único kernel.

### Modo SLAB (fallback)

Si `A` no cabe en VRAM, se almacena en memoria host *pinned* (`cudaMallocHost`) y se procesa en **slabs horizontales** con doble buffer y dos streams CUDA:

- `Z_in` y `Z_out` residen siempre completos en VRAM.
- El tamaño máximo del slab se calcula dinámicamente a partir de la VRAM restante.
- Mientras un stream ejecuta el kernel sobre el slab actual, el otro transfiere el siguiente slab por PCIe (`cudaMemcpyAsync`), ocultando la latencia de transferencia.

### Kernel tiled

Ambos modos usan el mismo kernel `tiled_mat_mul_kernel` con tiles de 32×32 en memoria compartida:

- Acceso coalescido a `A` y `Z_in`.
- Sin conflictos de bancos en memoria compartida.
- `#pragma unroll` en el bucle de acumulación interno.

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Estructura del Proyecto

```
efficient-krylov/
├── src/
│   ├── common/         # fuentes compartidas (main, benchmark, matrices, métricas, gen)
│   ├── cpu/            # multiplicación CPU (matmul_cpu.c)
│   └── gpu/            # multiplicación GPU (matmul_gpu.cu)
├── include/
│   ├── common/         # cabeceras compartidas
│   ├── cpu/            # matmul_cpu.h
│   └── gpu/            # matmul_gpu.h
├── data/               # matrices generadas por make gen (ignorado por git)
├── build/              # binarios compilados (ignorado por git)
└── Makefile
```

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Documentación

La documentación detallada de cada componente está en `docs/`:

| Documento | Descripción |
| --------- | ----------- |
| <a href="docs/research.md">research.md</a> | Investigación y contexto teórico |
| <a href="docs/generador.md">generador.md</a> | Generación e inicialización de matrices |
| <a href="docs/cpu.md">cpu.md</a> | Implementación CPU |
| <a href="docs/gpu.md">gpu.md</a> | Implementación GPU (CUDA) |

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

## Inicio Rápido

### Prerrequisitos

#### CPU

- MinGW-w64 / GCC (Windows: vía MSYS2)
- GNU Make

#### GPU

- GPU NVIDIA con soporte CUDA (arquitectura `sm_89` por defecto, RTX 40xx)
- CUDA Toolkit ≥ 11
- `nvcc` en el PATH

> En Windows, ejecutar `make` desde una terminal MSYS2 o añadir `C:\msys64\usr\bin` al PATH.

### Clonar

```bash
git clone https://github.com/<your-username>/efficient-krylov-sequence-computation-in-gpu.git
cd efficient-krylov-sequence-computation-in-gpu
```

### Ejecutar benchmark CPU

```bash
make gen EXP=10   # genera matrices de entrada (m = 1024)
make run          # compila y ejecuta el benchmark
```

### Ejecutar benchmark GPU

```bash
make GPU=1        # compila con nvcc
make gen EXP=12   # genera matrices de entrada
make run GPU=1    # ejecuta el benchmark GPU
```

### Benchmark completo (EXP 12 a 16)

Genera automáticamente las matrices y ejecuta el benchmark para cada tamaño sin intervención manual:

```bash
make bench-all        # CPU: EXP=12,13,14,15,16
make bench-all GPU=1  # GPU: EXP=12,13,14,15,16
```

### Referencia de targets

| Comando | Descripción |
| ------- | ----------- |
| `make` | Compila benchmark CPU + generador |
| `make GPU=1` | Compila benchmark GPU (nvcc) + generador |
| `make gen EXP=N` | Genera matrices para m = 2^N |
| `make run` | Ejecuta el benchmark con los datos actuales en `data/` |
| `make run GPU=1` | Ídem en GPU |
| `make bench-all` | Genera y ejecuta para EXP=12..16 (CPU) |
| `make bench-all GPU=1` | Genera y ejecuta para EXP=12..16 (GPU) |
| `make clean` | Elimina el directorio `build/` |
| `make help` | Muestra la referencia de comandos |

([volver arriba](#cómputo-eficiente-de-secuencias-de-krylov-en-gpu))

---

*Proyecto hecho con trasnochos y sufrimiento*
