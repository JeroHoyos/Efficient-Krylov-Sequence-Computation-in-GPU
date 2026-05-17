//! main.c - Lectura de parámetros, creación de directorio de salida y ejecución del benchmark.
//!
//! Este módulo contiene la función main() que lee y muestra los parámetros de entrada, crea un directorio de salida 
//! basado en el input y la fecha/hora actual, y luego llama a la función benchmark().
//!
//! Cómo funciona
//!
//! 1. Lee los parámetros desde el archivo "data/params.txt" y los muestra por consola.
//! 2. Crea un directorio de salida con un nombre basado en el input y la fecha/hora actual, y guarda su nombre en ".last_outdir".
//! 3. Llama a benchmark() pasando los parámetros leídos, el directorio de matrices y el directorio de salida.
//!
#include <stdio.h>
#include <stdlib.h>
#include "parametros.h"
#include "benchmark.h"

int main(void) {

    // stdout sin buffer: si la salida se redirige a archivo o pipe, cada línea
    // se escribe inmediatamente en vez de quedarse en un buffer hasta que
    // termine el programa. Imprescindible para monitorear benchmarks largos.
    // Se usa _IONBF en lugar de _IOLBF porque la CRT de MSVC trata _IOLBF
    // como buffer completo y corrompe el FILE si se le pasa size=0.
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *matrices_dir = "data";
    Parametros p;

    if (leer_params(matrices_dir, &p) != 0) {
        return 1;
    }

    #if defined(USE_CUDA)
        printf("\n[Modo: GPU / CUDA]\n");
    #else
        printf("\n[Modo: CPU]\n");
    #endif

    print_parametros(p);

    char outdir[64];
    if (crear_outdir(p.input, outdir, sizeof(outdir)) != 0) {
        return 1;
    }

    benchmark(p, matrices_dir, outdir);
    return 0;
}