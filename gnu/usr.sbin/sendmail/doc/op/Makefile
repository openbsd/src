#	$OpenBSD: Makefile,v 1.5 2013/03/14 15:18:19 jmc Exp $
#	$Sendmail: Makefile,v 8.16 2006/01/05 22:03:31 ca Exp $

SRCS=		op.me
OBJS=		op.ps
MACROS=		-me
ROFF_CMD=	groff
PIC_CMD=	pic
EQN_CMD=	eqn
UL_CMD=		ul
PS2PDF_CMD=	ps2pdf
PIC=		${PIC_CMD} -C
EQNASCII=	${EQN_CMD} -C -Tascii
EQNPS=		${EQN_CMD} -C -Tps
ROFFASCII=	${ROFF_CMD} -Tascii ${MACROS}
ROFFPS=		${ROFF_CMD} -Tps -mps ${MACROS}
ULASCII=	${UL_CMD} -t dumb
PS2PDF=		${PS2PDF_CMD}

all: ${OBJS}

op.ps: ${SRCS}
	rm -f $@
	${PIC} ${SRCS} | ${EQNPS} | ${ROFFPS} > $@
	test -s $@ || ${ROFFPS} -p -e ${SRCS} > $@

op.txt: ${SRCS}
	rm -f $@
	@echo "Note: see README file in case of errors."
	${PIC} ${SRCS} | ${EQNASCII} | \
	env GROFF_NO_SGR=NoThanks ${ROFFASCII} | \
	${ULASCII} > $@

op.pdf: op.ps
	rm -f $@
	${PS2PDF} op.ps op.pdf

clean:
	rm -f op.ps op.txt op.pdf

install: ${OBJS}
