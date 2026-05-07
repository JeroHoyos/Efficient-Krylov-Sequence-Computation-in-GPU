# ─── Compilador ───────────────────────────────────────────────────────────────
CC      = gcc

# ─── Fuentes ──────────────────────────────────────────────────────────────────
# Archivos .c del benchmark CPU (nueva ubicación tras reorganización)
SRCS    = src/cpu/main.c src/cpu/benchmark.c src/cpu/matrices.c \
          src/cpu/parametros.c src/cpu/metricas.c src/cpu/perfil.c \
          src/cpu/block_mul.c

# Archivos .c del generador de matrices
GEN_SRCS = src/common/gen_main.c src/common/gen.c

# ─── Parámetros configurables desde línea de comandos ─────────────────────────
# EXP: exponente para el tamaño de matriz (m = 2^EXP). Ej: make EXP=10
EXP     ?= 8
# MATDIR: carpeta donde están A.txt, Z.txt y params.txt
MATDIR  ?= data
# RAM_PCT: porcentaje de RAM disponible que puede usar el modo bloques (1-100)
RAM_PCT ?= 50

# ─── Directorios de salida ────────────────────────────────────────────────────
BINDIR  = build
IFLAGS  = -Iinclude/cpu -Iinclude/common
CFLAGS  = -O0 -Wall -Wextra -DRAM_PCT=$(RAM_PCT)

# ─── Binarios (distinto nombre/extensión según SO) ────────────────────────────
ifeq ($(OS),Windows_NT)
    LIBS    = -lm -lpsapi
    BIN     = $(BINDIR)/bench_O0.exe
    BIN_PG  = $(BINDIR)/bench_pg.exe
    GEN_BIN = $(BINDIR)/gen_matrices.exe
else
    LIBS    = -lm
    BIN     = $(BINDIR)/bench_O0
    BIN_PG  = $(BINDIR)/bench_pg
    GEN_BIN = $(BINDIR)/gen_matrices
endif

# ─── Targets declarados como "falsos" (no generan archivos con ese nombre) ────
.PHONY: run gen pg clean clean-runs clean-matrices

# ─── Compilar el generador de matrices ────────────────────────────────────────
$(GEN_BIN): $(GEN_SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) $(IFLAGS) -o $@ $^ $(LIBS)

# ─── Compilar el benchmark CPU ────────────────────────────────────────────────
$(BIN): $(SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) $(IFLAGS) -o $@ $^ $(LIBS)

# ─── Crear el directorio de binarios si no existe ─────────────────────────────
$(BINDIR):
	mkdir -p $(BINDIR)

# ─── gen: genera A.txt, Z.txt y params.txt en data/ ──────────────────────────
gen: $(GEN_BIN)
	./$(GEN_BIN) $(EXP)

# ─── run: ejecuta el benchmark leyendo las matrices desde MATDIR ──────────────
run: $(BIN)
	./$(BIN) $(MATDIR)

# ─── Compilar con profiling (gprof) ───────────────────────────────────────────
$(BIN_PG): $(SRCS) | $(BINDIR)
	$(CC) -pg -no-pie $(CFLAGS) $(IFLAGS) -o $@ $^ $(LIBS)

# ─── pg: ejecuta con profiling y guarda el reporte en metricas_pg.txt ─────────
pg: $(BIN_PG)
	./$(BIN_PG) $(MATDIR)
	gprof $(BIN_PG) gmon.out | perl -pe 's/(\d+\.\d{2})(?!\d)/sprintf("%.5f",$$1)/ge' > $$(cat .last_outdir)/metricas_pg.txt
	@echo "Perfil guardado en: $$(cat .last_outdir)/metricas_pg.txt"

# ─── Limpiar binarios y archivos de profiling ─────────────────────────────────
clean:
	rm -rf $(BINDIR) gmon.out .last_outdir

# ─── Limpiar directorios de resultados de runs anteriores ─────────────────────
clean-runs:
	rm -rf cpu_*/

# ─── Limpiar matrices generadas (incluyendo binarios cacheados) ───────────────
clean-matrices:
	rm -rf data/