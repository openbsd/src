#	$NetBSD: Makefile,v 1.1 1996/01/07 22:05:31 leo Exp $

SUBDIR= libtos loadbsd aptck chg_pid rawwrite

.PHONY: all clean install

all:
	@for dir in ${SUBDIR}; do	\
	    make -C $$dir all;		\
	 done

clean:
	@for dir in ${SUBDIR}; do	\
	    make -C $$dir clean;	\
	 done

install:
	@for dir in ${SUBDIR}; do	\
	    make -C $$dir install;	\
	 done
