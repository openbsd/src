# Target: S390 running Linux
DEPRECATED_TM_FILE= config/tm-linux.h
TDEPFILES=s390-tdep.o solib.o
# Post 5.0 tdep-files
TDEPFILES+=solib-svr4.o solib-legacy.o
