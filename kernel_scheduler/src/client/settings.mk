# Libraries
LIBS=commons pthread readline m utilsKS utils

# Custom libraries' paths
SHARED_LIBPATHS=
STATIC_LIBPATHS=../../utilsKS ../../../utils

IDIRS += ../../utilsKS ../../../utils /usr/local/include

# Compiler flags
CDEBUG=-g -Wall -DDEBUG -fdiagnostics-color=always
CRELEASE=-O3 -Wall -DNDEBUG