#ifndef PARAMETROS_H
#define PARAMETROS_H

typedef struct {
    int input; 
    int m; 
    int n;  
    int l; 
} Parametros;

typedef struct {
    long long flops;
    long long bytes_read;
    long long bytes_write;
} CostoTeorico;

CostoTeorico costo_teorico(int m, int n);

void print_parametros(Parametros p);

int leer_params(const char *dir, Parametros *p);

int crear_outdir(int input, char *outdir, size_t size);


#endif
