MAN=	tcpbench.1

CDIAGFLAGS=     -Wall
#CDIAGFLAGS+=   -Werror
CDIAGFLAGS+=    -Wpointer-arith
CDIAGFLAGS+=    -Wuninitialized
CDIAGFLAGS+=    -Wstrict-prototypes
CDIAGFLAGS+=    -Wmissing-prototypes
CDIAGFLAGS+=    -Wunused
CDIAGFLAGS+=    -Wsign-compare
CDIAGFLAGS+=    -Wbounded
CDIAGFLAGS+=    -Wshadow

#DEBUG=-g

PROG=tcpbench

LDADD=-lkvm

#BINGRP= kmem
#BINMODE=2555

.include <bsd.prog.mk>
