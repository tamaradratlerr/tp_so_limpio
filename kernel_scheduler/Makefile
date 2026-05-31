# Makefile (Raíz)
# Solo orquestamos, no compilamos archivos aquí directamente.

.PHONY: all clean server client

# Regla principal: compila ambos
all: server client

# Entra en los directorios y ejecuta el make allí
server:
	@$(MAKE) -C src/server

client:
	@$(MAKE) -C src/client

# Limpieza: recorre ambos directorios y limpia
clean:
	@$(MAKE) -C src/server clean
	@$(MAKE) -C src/client clean