# options for all the directories below

COPTS+= -Os
MAN=
LDSTATIC=${STATIC}
NOPIE=
NOMAN=1

# Architectures which are tight for space on some media
.if (${MACHINE} == "i386") || (${MACHINE} == "alpha") || (${MACHINE} == "sparc64")
COPTS+= -fno-stack-protector
.endif
