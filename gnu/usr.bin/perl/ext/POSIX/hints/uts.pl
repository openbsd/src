# UTS - Leaving -lm in there results in death of make with the message:
#         LD_RUN_PATH="/usr/ccs/lib" ld  -G -z text POSIX.o \
#         -o ../../lib/auto/POS IX/POSIX.so   -lm
# relocations referenced
#         from file(s)
#         /usr/ccs/lib/libm.a(acos.o)
#               ...

$self->{LIBS} = [''];
