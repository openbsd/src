#	$OpenBSD: Makefile.inc,v 1.1 2002/08/21 15:40:30 art Exp $

CFLAGS += -fpic -msoft-float
ADDR=-Tdata 8000
ELF_LDFLAGS+=${ADDR}
