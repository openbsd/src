# $OpenBSD: Makefile,v 1.1 2000/03/22 02:50:49 smurph Exp $
# $FreeBSD: src/sys/dev/aic7xxx/Makefile,v 1.6 1999/08/28 00:41:22 peter Exp $

PROG=	aicasm

CSRCS=	aicasm.c aicasm_symbol.c
GENSRCS= aicasm_gram.c aicasm_scan.c

GENHDRS=	y.tab.h

SRCS=	${GENSRCS} ${CSRCS}
CLEANFILES+= ${GENSRCS} ${GENHDRS} y.output
DPADD+=	${LIBL}
LDADD+=	-ll

# Correct path for kernel builds
# Don't rely on the kernel's .depend file
.ifdef MAKESRCPATH
.PATH: ${MAKESRCPATH}
DEPENDFILE=
.endif

CFLAGS+= -I/usr/include -I.
NOMAN=	noman

.ifdef DEBUG
CFLAGS+= -DDEBUG -g
YFLAGS+= -t
LFLAGS+= -d
MFLAGS= -l seq.lst
.endif

microcode aic7xxxreg.h aic7xxx_seq.h: aic7xxx.seq aic7xxx.reg
	./aicasm -I/sys ${MFLAGS} -r tempreg.h -o tempseq.h ${.CURDIR}/aic7xxx.seq
	grep OpenBSD: ${.CURDIR}/aic7xxx.seq | cat - tempseq.h > aic7xxx_seq.h
	grep OpenBSD: ${.CURDIR}/aic7xxx.reg | cat - tempreg.h > aic7xxxreg.h
	mv aic7xxx_seq.h /sys/dev/microcode/aic7xxx/
.ifdef DEBUG
	mv seq.lst /sys/dev/microcode/aic7xxx/
.endif
	cp aic7xxxreg.h /sys/dev/ic/
	rm -f tempseq.h tempreg.h

.include <bsd.prog.mk>
