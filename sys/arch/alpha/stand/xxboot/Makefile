#	$NetBSD: Makefile,v 1.1 1995/02/13 23:08:46 cgd Exp $

all: xxboot

xxboot: makexxboot
	/bin/rm -f xxboot
	./makexxboot > xxboot

clean cleandir:
	/bin/rm -f xxboot makexxboot

install:
	install -c -o bin -g bin -m 444 xxboot ${DESTDIR}/usr/mdec/sdboot
