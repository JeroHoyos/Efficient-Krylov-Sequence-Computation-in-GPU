CC   = gcc
SRCS = cpu/main.c cpu/benchmark.c cpu/matrices.c cpu/parametros.c \
       cpu/metricas.c cpu/perfil.c
EXP ?= 8
BINDIR = bin

ifeq ($(OS),Windows_NT)
    LIBS = -lm -lpsapi
    BIN  = $(BINDIR)/bench_O0.exe
    BIN_PG = $(BINDIR)/bench_pg.exe
else
    LIBS = -lm
    BIN  = $(BINDIR)/bench_O0
    BIN_PG = $(BINDIR)/bench_pg
endif

.PHONY: run pg clean clean-runs

$(BIN): $(SRCS) | $(BINDIR)
	$(CC) -O0 -o $@ $^ $(LIBS)

$(BINDIR):
	mkdir -p $(BINDIR)

run: $(BIN)
	echo "$(EXP)" | ./$(BIN)

$(BIN_PG): $(SRCS) | $(BINDIR)
	$(CC) -pg -O0 -o $@ $^ $(LIBS)

pg: $(BIN_PG)
	echo "$(EXP)" | ./$(BIN_PG)
	gprof $(BIN_PG) gmon.out > $$(cat .last_outdir)/metricas_pg.txt
	@echo "Perfil guardado en: $$(cat .last_outdir)/metricas_pg.txt"

clean:
	rm -rf $(BINDIR) gmon.out .last_outdir

clean-runs:
	rm -rf cpu_*/
