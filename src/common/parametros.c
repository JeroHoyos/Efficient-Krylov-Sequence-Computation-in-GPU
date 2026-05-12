#include <stdio.h>
#include "parametros.h"

void print_parametros(Parametros p) {
    printf("\n================================================\n");
    printf("            PARAMETROS DE ENTRADA\n");
    printf("================================================\n");
    printf("  Exponente (input)  : %d\n",      p.input);
    printf("  Dimension A        : %d x %d\n", p.m, p.m);
    printf("  Columnas Z (n)     : %d\n",      p.n);
    printf("  Iteraciones (l)    : %d\n",      p.l);
    printf("================================================\n");
}
