# Makefile para Trabajo Práctico Final Integrador (TPFI) - Computación Avanzada 2026
# UADER

CC = gcc
MPICC = mpicc
CFLAGS = -O3 -Wall -Wextra -I./include
OMPFLAGS = -fopenmp
LDFLAGS = -lm

BIN_DIR = bin
SRC_DIR = src
INC_DIR = include

COMMON_SRCS = $(SRC_DIR)/image.c $(SRC_DIR)/cartoon_ops.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

TARGETS = $(BIN_DIR)/secuencial $(BIN_DIR)/openmp $(BIN_DIR)/mpi $(BIN_DIR)/mpi_openmp

.PHONY: all clean dirs

all: dirs $(TARGETS)

dirs:
	mkdir -p $(BIN_DIR) output data

# Objetos comunes secuenciales
$(SRC_DIR)/image.o: $(SRC_DIR)/image.c $(INC_DIR)/image.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/cartoon_ops.o: $(SRC_DIR)/cartoon_ops.c $(INC_DIR)/cartoon_ops.h $(INC_DIR)/image.h
	$(CC) $(CFLAGS) -c $< -o $@

# 1. Programa Secuencial
$(BIN_DIR)/secuencial: $(SRC_DIR)/secuencial.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# 2. Programa Memoria Compartida (OpenMP)
$(BIN_DIR)/openmp: $(SRC_DIR)/openmp.c $(SRC_DIR)/image.c $(INC_DIR)/image.h $(INC_DIR)/cartoon_ops.h
	$(CC) $(CFLAGS) $(OMPFLAGS) $(SRC_DIR)/openmp.c $(SRC_DIR)/image.c -o $@ $(LDFLAGS)

# 3. Programa Memoria Distribuida (MPI)
$(BIN_DIR)/mpi: $(SRC_DIR)/mpi.c $(SRC_DIR)/image.c $(SRC_DIR)/cartoon_ops.c $(INC_DIR)/image.h $(INC_DIR)/cartoon_ops.h
	$(MPICC) $(CFLAGS) $(SRC_DIR)/mpi.c $(SRC_DIR)/image.c $(SRC_DIR)/cartoon_ops.c -o $@ $(LDFLAGS)

# 4. Programa Híbrido (MPI + OpenMP)
$(BIN_DIR)/mpi_openmp: $(SRC_DIR)/mpi_openmp.c $(SRC_DIR)/image.c $(SRC_DIR)/cartoon_ops.c $(INC_DIR)/image.h $(INC_DIR)/cartoon_ops.h
	$(MPICC) $(CFLAGS) $(OMPFLAGS) $(SRC_DIR)/mpi_openmp.c $(SRC_DIR)/image.c $(SRC_DIR)/cartoon_ops.c -o $@ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR) $(SRC_DIR)/*.o output/*.ppm output/*.bmp

