#	$OpenBSD: Makefile,v 1.3 2016/09/23 18:56:49 bluhm Exp $

PROG=	ptrace

read_i:
	${.OBJDIR}/${PROG} -ri
read_d:
	${.OBJDIR}/${PROG} -rd
read_i_bad:
	${.OBJDIR}/${PROG} -rib
read_d_bad:
	${.OBJDIR}/${PROG} -rdb

io_read_i:
	${.OBJDIR}/${PROG} -Iri
io_read_d:
	${.OBJDIR}/${PROG} -Ird
io_read_i_bad:
	${.OBJDIR}/${PROG} -Irib
io_read_d_bad:
	${.OBJDIR}/${PROG} -Irdb

REGRESS_TARGETS=read_i read_d read_i_bad read_d_bad \
	io_read_i io_read_d io_read_i_bad io_read_d_bad

${REGRESS_TARGETS}: ${PROG}

.include <bsd.regress.mk>
