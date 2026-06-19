include settings.mk

NAME=utilsKS

SRC=utilsKS.c
HEADER=utilsKS.h
OBJ=utilsKS.o
OUT=lib/lib$(NAME).a

CFLAGS=$(CDEBUG)

.PHONY: all clean debug release

all: debug

debug: CFLAGS=$(CDEBUG)
debug: $(OUT)

release: CFLAGS=$(CRELEASE)
release: $(OUT)

$(OUT): $(OBJ) | lib
	$(AR) $(ARFLAGS) $@ $^

$(OBJ): $(SRC) $(HEADER)
	$(CC) $(CFLAGS) -I. -c $< -o $@

lib:
	mkdir -p lib

clean:
	rm -f $(OBJ)
	rm -f $(OUT)


	





















# # Makefile dentro de /utilsKS
# include ../src/server/settings.mk # O usa la ruta donde esté tu settings.mk

# all: lib/libutilsKS.a

# lib/libutilsKS.a: utilsKS.o
# 	mkdir -p lib
# 	ar rcs lib/libutilsKS.a utilsKS.o  # Esto crea el archivo que el linker busca

# utilsKS.o: utilsKS.c
# 	gcc -c -o utilsKS.o utilsKS.c -I.