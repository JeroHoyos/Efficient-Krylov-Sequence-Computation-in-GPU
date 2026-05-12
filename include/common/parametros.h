#ifndef PARAMETROS_H
#define PARAMETROS_H

typedef struct {
    int input;  /* exponente: m = 2^input        */
    int m;      /* filas de A (m x m) y Z (m x n)*/
    int n;      /* columnas de Z                  */
    int l;      /* iteraciones: l = (2*m)/n       */
} Parametros;

void print_parametros(Parametros p);

#endif
