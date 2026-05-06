CC      = gcc
SRCS    = cpu/main.c cpu/benchmark.c cpu/matrices.c cpu/parametros.c \
          cpu/metricas.c cpu/perfil.c
GEN_SRCS = gen_matrices/main.c gen_matrices/gen.c
EXP    ?= 8
MATDIR ?= matrices
BINDIR  = bin

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

.PHONY: run gen pg clean clean-runs clean-matrices

$(GEN_BIN): $(GEN_SRCS) | $(BINDIR)
	$(CC) -O0 -o $@ $^ $(LIBS)

$(BIN): $(SRCS) | $(BINDIR)
	$(CC) -O0 -o $@ $^ $(LIBS)

$(BINDIR):
	mkdir -p $(BINDIR)

# Genera A.txt, Z.txt y params.txt en la carpeta matrices/
gen: $(GEN_BIN)
	echo "$(EXP)" | ./$(GEN_BIN)

# Ejecuta el benchmark leyendo las matrices desde MATDIR
run: $(BIN)
	echo "$(MATDIR)" | ./$(BIN)

$(BIN_PG): $(SRCS) | $(BINDIR)
	$(CC) -pg -no-pie -O0 -o $@ $^ $(LIBS)

pg: $(BIN_PG)
	echo "$(MATDIR)" | ./$(BIN_PG)
	gprof $(BIN_PG) gmon.out | perl -pe 's/(\d+\.\d{2})(?!\d)/sprintf("%.5f",$$1)/ge' > $$(cat .last_outdir)/metricas_pg.txt
	@echo "Perfil guardado en: $$(cat .last_outdir)/metricas_pg.txt"

clean:
	rm -rf $(BINDIR) gmon.out .last_outdir

clean-runs:
	rm -rf cpu_*/

clean-matrices:
	rm -rf matrices/
