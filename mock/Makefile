# Project name
NAME = cliente_mock

# Compiler and flags
CC = gcc
CFLAGS = -g -Wall -Wextra -D_DEFAULT_SOURCE -fdiagnostics-color=always

# Paths to dependencies (ajustar si tu estructura real difiere)
UTILS_DIR = ../utils/src
UTILSKS_DIR = ../utilsKS

IDIRS = -I./src -I$(UTILS_DIR) -I$(UTILSKS_DIR) -I/usr/local/include
LIBDIRS = -L$(UTILSKS_DIR)/lib -L/usr/local/lib

LIBS = -lutilsKS -lcommons -lpthread -lreadline -lm

# Sources
SRCS = src/mock.c $(UTILS_DIR)/global_utils.c
OBJS = $(SRCS:.c=.o)

OUT = ../bin/$(NAME)

.PHONY: all clean

all: $(OUT)

$(OUT): $(OBJS)
	mkdir -p $(dir $(OUT))
	$(CC) $(CFLAGS) -o $(OUT) $(OBJS) $(LIBDIRS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(IDIRS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OUT)