# Makefile for lpansi
#
VERSION=2.1
# For hp-ux
# CFLAGS=-Aa -O 
# For others:
CFLAGS=-O
BINDIR=/usr/local/bin
MANDIR=/usr/local/man/man1

lpansi:	lpansi.o Makefile
	$(CC) -o lpansi lpansi.o

clean:
	rm -f lpansi.o

shar:
	shar -v README lpansi.c lpansi.1 Makefile > lpansi-$(VERSION).shar

install: lpansi
	chmod o+rx,g+rx,u+rx lpansi; cp -p lpansi $(BINDIR)/lpansi

install_man:
	chmod o+rx,g+rx,u+rx lpansi.1; cp -p lpansi.1 $(MANDIR)/lpansi.1
