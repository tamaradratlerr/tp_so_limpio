# Makefile dentro de /utilsKS
include ../src/server/settings.mk # O usa la ruta donde esté tu settings.mk

all: lib/libutilsKS.a

lib/libutilsKS.a: utilsKS.o
	mkdir -p lib
	ar rcs lib/libutilsKS.a utilsKS.o  # Esto crea el archivo que el linker busca

utilsKS.o: utilsKS.c
	gcc -c -o utilsKS.o utilsKS.c -I.