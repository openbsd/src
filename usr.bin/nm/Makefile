#	$OpenBSD: Makefile,v 1.7 2004/10/10 16:45:00 mickey Exp $

PROG=	nm
SRCS=	nm.c elf32.c elf64.c
CLEANFILES+=elf32.c elf64.c
CPPFLAGS+=-I${.CURDIR}
LINKS=	${BINDIR}/nm ${BINDIR}/size
MAN=	nm.1 size.1

elf32.c: ${.CURDIR}/elf.c
	echo '#define ELFSIZE 32' | cat - $> > ${.TARGET}

elf64.c: ${.CURDIR}/elf.c
	echo '#define ELFSIZE 64' | cat - $> > ${.TARGET}

.include <bsd.prog.mk>
