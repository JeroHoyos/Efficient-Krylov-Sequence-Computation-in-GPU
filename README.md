# Benchmark — Multiplicación Iterativa de Matrices (CPU)

Benchmark en C que mide el rendimiento de multiplicación de matrices (`Z = A · Z`) en punto flotante simple. Genera métricas de tiempo y memoria. Compatible con Linux y Windows (MinGW).

---

## Ejecución

El programa recibe un **exponente** (`input`) que define el tamaño del problema: `m = 2^input`.

```bash
make              # compila y ejecuta con exponente 8 (por defecto)
make EXP=10       # compila y ejecuta con exponente 10
make clean        # elimina el binario
```

También se puede correr directamente tras compilar:

```bash
echo 8 | ./bench_O0
```

Para ejecutar todos los exponentes del 10 al 20:

```bash
make EXP=10
make EXP=11   
make EXP=12    
make EXP=13    
make EXP=14    
make EXP=15    
make EXP=16  
make EXP=17 
make EXP=18  
make EXP=19  
make EXP=20    
```

---

## Salida

Cada ejecución crea un directorio `cpu_{input}_MMDD_HHMM/` con:

- `resultado_N.txt` — snapshot de la matriz `Z` tras la iteración `N`
- `metricas.txt` — tiempos por iteración, tiempo total y pico de memoria

Ejemplo para `input=8` iniciado a las 17:45 del 5 de mayo: `cpu_8_0505_1745/`
