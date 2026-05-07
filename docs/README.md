# Documentación técnica

Esta carpeta documenta el funcionamiento interno de cada módulo del benchmark CPU de secuencias de Krylov por bloques.

## Índice

| Archivo | Qué cubre |
|---------|-----------|
| [generador.md](generador.md) | Cómo se generan A (row-estocástica) y Z (aleatoria) y por qué se escriben fila a fila |
| [benchmark.md](benchmark.md) | El punto de entrada, el bucle principal y la decisión in-RAM vs. bloques |
| [matrices.md](matrices.md) | E/S binaria, detección de RAM disponible y cálculo del tamaño de bloque |
| [block_mul.md](block_mul.md) | Multiplicación tiled fuera de RAM y la técnica de alternancia de archivos Z |
| [metricas.md](metricas.md) | Recolección de métricas por iteración y generación del informe |
| [perfil.md](perfil.md) | Contadores de bajo nivel: CPU, RSS y fallos de página |

## Visión general del flujo

```
make gen EXP=N
  └─ gen_main.c → gen.c → escribe data/A.bin, data/Z.bin, data/params.txt

make run
  └─ main.c          lee MATDIR por stdin, crea directorio cpu_{exp}_MMDD_HHMMSS/
       └─ benchmark.c  detecta RAM disponible
            ├─ [caben] matrices.c → carga A y Z completos → bucle en RAM
            └─ [no caben] block_mul.c → bucle tiled con archivos en disco
                 └─ en ambos casos: metricas.c + perfil.c miden cada iteración
```
