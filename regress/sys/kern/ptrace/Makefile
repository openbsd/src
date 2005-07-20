#	$OpenBSD: Makefile,v 1.1 2005/07/20 15:31:43 art Exp $

PROG=	ptrace

read_i:
	${PROG} -ri
read_d:
	${PROG} -rd
read_i_bad:
	${PROG} -rib
read_d_bad:
	${PROG} -rdb

io_read_i:
	${PROG} -Iri
io_read_d:
	${PROG} -Ird
io_read_i_bad:
	${PROG} -Irib
io_read_d_bad:
	${PROG} -Irdb

REGRESS_TARGETS=read_i read_d read_i_bad read_d_bad \
	io_read_i io_read_d io_read_i_bad io_read_d_bad

.include <bsd.regress.mk>
