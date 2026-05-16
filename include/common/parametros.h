#ifndef PARAMETROS_H
#define PARAMETROS_H

typedef struct {
    int input; 
    int m; 
    int n;  
    int l; 
} Parametros;

void print_parametros(Parametros p);

int leer_params(const char *dir, Parametros *p);

int crear_outdir(int input, char *outdir, size_t size);


#endif
