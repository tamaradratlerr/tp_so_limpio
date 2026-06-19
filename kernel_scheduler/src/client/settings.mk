# Libraries
LIBS=commons pthread readline m utilsKS utils

# Custom libraries' paths
SHARED_LIBPATHS=
STATIC_LIBPATHS=../../utilsKS ../../../utils

# Include paths
IDIRS += ../../utilsKS ../../../utils/src /usr/local/include

# Compiler flags
CDEBUG=-g -Wall -DDEBUG -fdiagnostics-color=always
CRELEASE=-O3 -Wall -DNDEBUG


# # Libraries
# LIBS=commons pthread readline m utilsKS utils

# # Custom libraries' paths
# SHARED_LIBPATHS=
# STATIC_LIBPATHS=../../utilsKS ../../utils ../../../utils

# # Include paths
# IDIRS += ../../utilsKS ../../utils/src ../../../utils/src /usr/local/include

# # Compiler flags
# CDEBUG=-g -Wall -DDEBUG -fdiagnostics-color=always
# CRELEASE=-O3 -Wall -DNDEBUG