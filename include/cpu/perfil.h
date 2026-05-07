#ifndef PERFIL_H
#define PERFIL_H

/* Retorna el número acumulado de fallos de página menores del proceso. */
long      leer_minflt(void);

/* Retorna el número acumulado de fallos de página mayores del proceso. */
long      leer_majflt(void);

/* Retorna el tiempo de CPU consumido por el proceso en microsegundos. */
long long leer_cpu_us_proceso(void);

/* Retorna el RSS actual del proceso en kilobytes. */
long leer_rss_kb(void);

#endif
