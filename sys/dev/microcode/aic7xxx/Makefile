# $OpenBSD: Makefile,v 1.8 2004/05/19 00:35:18 krw Exp $
# $FreeBSD: src/sys/dev/aic7xxx/Makefile,v 1.6 1999/08/28 00:41:22 peter Exp $

PROG=	aicasm

CSRCS=	aicasm.c aicasm_symbol.c
GENSRCS= aicasm_gram.c aicasm_scan.c aicasm_macro_gram.c aicasm_macro_scan.c

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
YFLAGS= ${.TARGET:M*macro*:S/$(.TARGET)/-p mm/} -d
LFLAGS+= ${.TARGET:M*macro*:S/$(.TARGET)/-olex.yy.c/} ${.TARGET:M*macro*:S/$(.TARGET)/-Pmm/}
NOMAN=	noman

.ifdef DEBUG
CFLAGS+= -DDEBUG -g
YFLAGS+= -t
LFLAGS+= -d
SEQFLAGS= -l seq.lst
.endif

microcode:	7xxx_headers 79xx_headers

7xxx_headers:	aic7xxx.seq aic7xxx.reg
	${OBJDIR}./aicasm -I/sys ${SEQFLAGS} -r tempreg.h -o tempseq.h ${.CURDIR}/aic7xxx.seq
	grep OpenBSD: ${.CURDIR}/aic7xxx.seq | cat - tempseq.h > aic7xxx_seq.h
	grep OpenBSD: ${.CURDIR}/aic7xxx.reg | cat - tempreg.h > aic7xxx_reg.h
	mv aic7xxx_seq.h /sys/dev/microcode/aic7xxx/
	mv aic7xxx_reg.h /sys/dev/microcode/aic7xxx/
.ifdef DEBUG
	mv seq.lst /sys/dev/microcode/aic7xxx/seq7xxx.lst
.endif
	rm -f tempseq.h tempreg.h

79xx_headers:	aic79xx.seq aic79xx.reg
	${OBJDIR}./aicasm -I/sys ${SEQFLAGS} -r tempreg.h -o tempseq.h ${.CURDIR}/aic79xx.seq
	grep OpenBSD: ${.CURDIR}/aic79xx.seq | cat - tempseq.h > aic79xx_seq.h
	grep OpenBSD: ${.CURDIR}/aic79xx.reg | cat - tempreg.h > aic79xx_reg.h
	mv aic79xx_seq.h /sys/dev/microcode/aic7xxx/
	mv aic79xx_reg.h /sys/dev/microcode/aic7xxx/
.ifdef DEBUG
	mv seq.lst /sys/dev/microcode/aic7xxx/seq79xx.lst
.endif
	rm -f tempseq.h tempreg.h

.include <bsd.prog.mk>
