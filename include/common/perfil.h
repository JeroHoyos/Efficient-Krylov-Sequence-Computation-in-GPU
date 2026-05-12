#ifndef PERFIL_H
#define PERFIL_H

/* Imprime "  [nombre]: filas x cols = X.XX MB" */
void print_tamano(const char *nombre, int filas, int cols);

/* Imprime resumen de RAM/VRAM estimada para el experimento */
void print_memoria(int m, int n);

#endif
