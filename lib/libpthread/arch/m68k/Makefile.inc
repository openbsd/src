# Machine dependent sources
.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+= machdep.c syscall.S
