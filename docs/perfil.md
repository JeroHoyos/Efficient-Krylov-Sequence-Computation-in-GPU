# Contadores de rendimiento de bajo nivel (`src/cpu/perfil.c`)

## Propósito

`perfil.c` encapsula el acceso a tres contadores del sistema operativo que el benchmark necesita por iteración: tiempo de CPU del proceso, RSS y fallos de página. Al aislarlos en un módulo separado con `#ifdef`, el resto del código es portátil entre Linux y Windows sin `#ifdef` dispersos.

## Tiempo de CPU del proceso

### Linux: `/proc/self/stat`

```c
fscanf(f, "%d %255s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu",
       &pid, comm, &state, &ppid, ..., &utime, &stime);
long clk_tck = sysconf(_SC_CLK_TCK);
return (long long)((utime + stime) * 1000000LL / clk_tck);
```

`/proc/self/stat` expone `utime` (ticks en modo usuario) y `stime` (ticks en modo kernel). Se suman porque el proceso puede hacer syscalls costosas (como `fread` en modo bloques) y ese tiempo de kernel debe contarse como tiempo de CPU del proceso. Los ticks se convierten a microsegundos usando `_SC_CLK_TCK` (típicamente 100 Hz en Linux, aunque puede variar).

El motivo para usar `microsegundos` como unidad interna es que `wall time` también se mide con resolución de nanosegundos, y el cociente `cpu_us / wall_us` se puede calcular sin perder precisión en divisiones de números muy pequeños.

### Windows: `GetProcessTimes`

```c
GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);
// FILETIME tiene unidades de 100 nanosegundos
return (long long)((u.QuadPart + k.QuadPart) / 10);  // → microsegundos
```

`GetProcessTimes` retorna `FILETIME` en unidades de 100 ns. Dividir por 10 convierte a microsegundos para mantener la misma unidad que Linux.

## RSS (Resident Set Size)

### Linux: `/proc/self/status`

```c
while (fgets(line, sizeof(line), f))
    if (strncmp(line, "VmRSS:", 6) == 0) { sscanf(line + 6, " %ld", &rss); break; }
```

`VmRSS` es la cantidad de páginas del proceso que están actualmente en RAM física (no en swap). Es el indicador más directo del consumo real de memoria: `VmSize` incluye memoria virtual que puede no estar cargada, y `VmPeak` no muestra el valor actual.

### Windows: `GetProcessMemoryInfo`

```c
PROCESS_MEMORY_COUNTERS pmc;
GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
return (long)(pmc.WorkingSetSize / 1024);
```

`WorkingSetSize` es el equivalente de RSS en Windows: las páginas del proceso residentes en RAM.

## Fallos de página

### Linux: `getrusage`

```c
struct rusage ru;
getrusage(RUSAGE_SELF, &ru);
return ru.ru_minflt;   // fallos menores
return ru.ru_majflt;   // fallos mayores
```

**Fallos menores** (`ru_minflt`): la página no estaba en la tabla de páginas del proceso pero sí en memoria física (ej: primera escritura a una página recién asignada, o página compartida ya cargada). Son baratos.

**Fallos mayores** (`ru_majflt`): la página tiene que leerse desde disco. Son caros (varios ms). En el modo in-RAM deberían ser casi cero; en el modo bloques reflejan las lecturas de `A.bin` y `Z.bin` que no estaban en el page cache.

Medir ambos separados permite distinguir si el overhead viene de la gestión de memoria virtual (menores) o del I/O real (mayores).

### Windows

Windows no expone fallos mayores por separado en su API pública. `leer_minflt` retorna `PageFaultCount` (suma de ambos tipos) y `leer_majflt` retorna 0. El informe refleja esta limitación.

## Dónde vive `leer_rss_kb`

`leer_rss_kb` está implementada en `perfil.c` junto al resto de los contadores de bajo nivel (CPU time, page faults). Esto mantiene una separación clara: `perfil.c` agrupa todas las lecturas de `/proc` y la API del SO; `metricas.c` solo agrega y formatea los valores que ya le llegan calculados.

## Por qué leer `/proc` en cada iteración

Abrir y parsear `/proc/self/stat` o `/proc/self/status` en cada iteración tiene un costo mínimo (son archivos virtuales generados por el kernel sin I/O real), y es más preciso que mantener contadores propios en el proceso: el kernel tiene acceso directo a sus estructuras internas y los valores son exactos en el momento de la lectura.
