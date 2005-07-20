#	$OpenBSD: Makefile,v 1.2 2005/07/20 15:41:51 deraadt Exp $

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

.include <bsd.regress.mk>
