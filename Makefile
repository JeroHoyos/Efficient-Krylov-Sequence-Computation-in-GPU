# ============================================================
#  Makefile — EFFICIENT-KRYLOV
#  Uso:
#    make                 → benchmark CPU + generador
#    make GPU=1           → benchmark GPU (CUDA) + generador
#    make gen EXP=10      → genera matrices para 2^10
#    make run             → ejecuta el benchmark con datos en data/
#    make bench-all       → genera y ejecuta benchmark para EXP=12..16 (CPU)
#    make bench-all GPU=1 → ídem en GPU
#    make clean           → limpia build/
#    make help            → muestra este mensaje de ayuda
# ============================================================

CC      = gcc
NVCC    = nvcc
CFLAGS  = -O2 -Wall -Wextra
NVARCH  ?= sm_89              # RTX 40xx; se puede cambiar con NVARCH=sm_XX
NVFLAGS = -O2 -arch=$(NVARCH)
LIBS    = -lm

EXE_EXT := .exe
MKDIR    = mkdir -p $(BINDIR)
RMDIR    = rm -rf $(BINDIR)

BINDIR   = build
SRC_COM  = src/common
SRC_CPU  = src/cpu
SRC_GPU  = src/gpu
INC_COM  = include/common
INC_CPU  = include/cpu
INC_GPU  = include/gpu

IFLAGS_COM  = -I$(INC_COM)
IFLAGS_CPU  = $(IFLAGS_COM) -I$(INC_CPU)
IFLAGS_GPU  = $(IFLAGS_COM) -I$(INC_GPU)

COMMON_SRCS = $(SRC_COM)/parametros.c \
              $(SRC_COM)/matrices.c    \
              $(SRC_COM)/metricas.c

# main.c y benchmark.c se compilan distinto según CPU/GPU (ver abajo).
BENCH_SHARED = $(SRC_COM)/main.c      \
               $(SRC_COM)/benchmark.c \
               $(COMMON_SRCS)

GEN_SRCS = $(SRC_COM)/gen_main.c \
           $(SRC_COM)/gen.c

GEN_BIN   = $(BINDIR)/gen_matrices$(EXE_EXT)
BENCH_BIN = $(BINDIR)/bench$(EXE_EXT)

# EXP controla el tamaño de la matriz (m = 2^EXP).
# DATA es el directorio donde gen_matrices escribe los .bin.
EXP  ?= 8
DATA ?= data

# ============================================================
#  Selección CPU / GPU
# ============================================================
ifdef GPU
  # nvcc compila en un solo paso todos los .c y .cu juntos;
  # no hace falta separar objetos intermedios.
  BENCH_BIN := $(BINDIR)/bench_gpu$(EXE_EXT)
  MUL_SRC    = $(SRC_GPU)/matmul_gpu.cu

  $(BENCH_BIN): $(BENCH_SHARED) $(MUL_SRC) | $(BINDIR)
	$(NVCC) $(NVFLAGS) $(IFLAGS_GPU) -DUSE_CUDA \
	    -o $@ $^
	@echo "[OK] Benchmark GPU compilado -> $@"

else
  MUL_SRC = $(SRC_CPU)/matmul_cpu.c

  $(BENCH_BIN): $(BENCH_SHARED) $(MUL_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(IFLAGS_CPU) \
	    -o $@ $^ $(LIBS)
	@echo "[OK] Benchmark CPU compilado -> $@"
endif

# ============================================================
#  Targets comunes
# ============================================================
.PHONY: all gen run bench-all all-run clean help

all: $(BENCH_BIN) $(GEN_BIN)

# '| $(BINDIR)' es una dependencia de orden: crea el directorio
# si no existe, pero no re-linka si su timestamp cambia.
$(BINDIR):
	$(MKDIR)

$(GEN_BIN): $(GEN_SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) $(IFLAGS_COM) -o $@ $^ $(LIBS)
	@echo "[OK] Generador compilado -> $@"

gen: $(GEN_BIN)
	./$(GEN_BIN) $(EXP)

run: $(BENCH_BIN)
	./$(BENCH_BIN)

# Genera y ejecuta el benchmark para cada tamaño EXP=12..16.
# Pasar GPU=1 para correr en GPU: make bench-all GPU=1
bench-all: $(BENCH_BIN) $(GEN_BIN)
	@echo "=== Benchmark EXP=12 (m=2^12) ===" && echo s | ./$(GEN_BIN) 12 && ./$(BENCH_BIN)
	@echo "=== Benchmark EXP=13 (m=2^13) ===" && echo s | ./$(GEN_BIN) 13 && ./$(BENCH_BIN)
	@echo "=== Benchmark EXP=14 (m=2^14) ===" && echo s | ./$(GEN_BIN) 14 && ./$(BENCH_BIN)
	@echo "=== Benchmark EXP=15 (m=2^15) ===" && echo s | ./$(GEN_BIN) 15 && ./$(BENCH_BIN)
	@echo "=== Benchmark EXP=16 (m=2^16) ===" && echo s | ./$(GEN_BIN) 16 && ./$(BENCH_BIN)

all-run: all run

clean:
	$(RMDIR)
	@echo "[OK] build/ eliminado"

help:
	@echo "Uso:"
	@echo "  make                 -> benchmark CPU + generador"
	@echo "  make GPU=1           -> benchmark GPU (CUDA) + generador"
	@echo "  make gen EXP=N       -> genera matrices para 2^N"
	@echo "  make run             -> ejecuta el benchmark con datos en data/"
	@echo "  make bench-all       -> genera y ejecuta benchmark para EXP=12..16 (CPU)"
	@echo "  make bench-all GPU=1 -> idem en GPU"
	@echo "  make clean           -> limpia build/"
