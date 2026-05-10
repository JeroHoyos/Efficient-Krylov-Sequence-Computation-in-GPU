/* Librerías estándar */
#include <stdio.h>   // printf, fopen, fprintf
#include <stdlib.h>  // atoi, malloc, free
#include <math.h>    // pow

/* Compatibilidad Windows/Linux para crear carpetas */
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)      // Windows
#else
  #include <sys/stat.h>
  #define MKDIR(p) mkdir(p, 0755) // Linux
#endif

/* Header del generador de matrices */
#include "gen.h"

int main(
    int argc,    // cantidad de argumentos
    char *argv[] // textos escritos al ejecutar el programa
) {

    /* Verifica que el usuario escriba el tamaño de la matriz */
    if (argc < 2) {
      /* Muestra un mensaje de error indicando cómo ejecutar el programa */
      fprintf(stderr, "Ejemplo de uso: %s 8\n", argv[0]);
      return 1;
    }

    /* Convierte el argumento a entero */
    int input = atoi(argv[1]);

    /* Verifica que EXP sea positivo */
    if (input <= 0) {
        fprintf(stderr, "Error: EXP debe ser un entero positivo\n");
        return 1;
    }

    /* Calcula: m, n, l */
  int m = (int)pow(2, input);
  int n = 128;
  int l = (2 * m) / n;

  /* Calcula el tamaño de la matriz A en memoria */
  long long bytes_A = (long long)m * m * sizeof(float);

  double kb_A = bytes_A / 1024.0;
  double mb_A = kb_A / 1024.0;
  double gb_A = mb_A / 1024.0;
  double tb_A = gb_A / 1024.0;

  /* Muestra información antes de generar */
  printf("\nSe generará la matriz A [%d x %d]\n", m, m);

  /* Determina y muestra solo la unidad más grande adecuada */
  if (tb_A >= 1.0) {
      printf("Tamaño aproximado: %.2f TB\n", tb_A);
  } else if (gb_A >= 1.0) {
      printf("Tamaño aproximado: %.2f GB\n", gb_A);
  } else if (mb_A >= 1.0) {
      printf("Tamaño aproximado: %.2f MB\n", mb_A);
  } else if (kb_A >= 1.0) {
      printf("Tamaño aproximado: %.2f KB\n", kb_A);
  } else {
      printf("Tamaño aproximado: %lld Bytes\n", bytes_A);
  }

  /* Pide confirmación al usuario */
  char confirmacion;

  printf("¿Continuar? (s/n): ");
  scanf(" %c", &confirmacion);

  /* Cancela si el usuario no confirma */
  if (confirmacion != 's' && confirmacion != 'S') {
      printf("Operación cancelada.\n");
      return 0;
  }

    /* Carpeta donde se guardarán los archivos generados */
    const char *dir = "data";

    /* Guarda los parámetros del experimento en data/params.txt */
    guardar_parametros(dir, input, m, n, l);

    /* Genera las matrices */
    generar_matrices(dir, m, n);

    printf("\nMatrices generadas correctamente en '%s/'\n", dir);

    return 0;
}