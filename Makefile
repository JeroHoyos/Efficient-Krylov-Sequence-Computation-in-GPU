# Compilador
CC = gcc

# Archivos fuente del benchmark
SRCS = src/cpu/main.c \
       src/cpu/benchmark.c \
       src/cpu/matrices.c \
       src/cpu/parametros.c \
       src/cpu/metricas.c \
       src/cpu/perfil.c \
       src/cpu/block_mul.c

# Archivos fuente del generador
GEN_SRCS = src/common/gen_main.c \
           src/common/gen.c

# Parámetros
EXP = 8
MATDIR = data

# Carpetas
BINDIR = build

# Includes y flags
IFLAGS = -Iinclude/cpu -Iinclude/common
CFLAGS = -O0 -Wall -Wextra

# Ejecutables
BIN = $(BINDIR)/bench
GEN_BIN = $(BINDIR)/gen_matrices

# Librerías
LIBS = -lm

# Crear carpeta build
$(BINDIR):
	mkdir -p $(BINDIR)

# Compilar benchmark
$(BIN): $(SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) $(IFLAGS) -o $@ $^ $(LIBS)

# Compilar generador
$(GEN_BIN): $(GEN_SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) $(IFLAGS) -o $@ $^ $(LIBS)

# Generar matrices
gen: $(GEN_BIN)
	./$(GEN_BIN) $(EXP)

# Ejecutar benchmark
run: $(BIN)
	./$(BIN) $(MATDIR)

# Limpiar
clean:
	rm -rf $(BINDIR)