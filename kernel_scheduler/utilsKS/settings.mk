CC=gcc
AR=ar
ARFLAGS=rcs

CDEBUG=-g -Wall -Wextra -DDEBUG -fdiagnostics-color=always -fPIC
CRELEASE=-O3 -Wall -Wextra -DNDEBUG -fPIC