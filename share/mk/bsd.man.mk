#	$OpenBSD: bsd.man.mk,v 1.17 1999/11/27 04:31:12 millert Exp $
#	$NetBSD: bsd.man.mk,v 1.23 1996/02/10 07:49:33 jtc Exp $
#	@(#)bsd.man.mk	5.2 (Berkeley) 5/11/90

MANTARGET?=	cat
NROFF?=		nroff -Tascii
TBL?=		tbl

.if !target(.MAIN)
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.MAIN: all
.endif

.SUFFIXES: .1 .2 .3 .3p .4 .5 .6 .7 .8 .9 .1tbl .2tbl .3tbl .4tbl .5tbl .6tbl \
	.7tbl .8tbl .9tbl .cat1 .cat2 .cat3 .cat4 .cat5 .cat6 .cat7 .cat8 .cat9

.9.cat9 .8.cat8 .7.cat7 .6.cat6 .5.cat5 .4.cat4 .3p.cat3p .3.cat3 .2.cat2 .1.cat1:
	@echo "${NROFF} -mandoc ${.IMPSRC} > ${.TARGET}"
	@${NROFF} -mandoc ${.IMPSRC} > ${.TARGET} || (rm -f ${.TARGET}; false)

.9tbl.cat9 .8tbl.cat8 .7tbl.cat7 .6tbl.cat6 .5tbl.cat5 .4tbl.cat4 .3tbl.cat3 \
.2tbl.cat2 .1tbl.cat1:
	@echo "${TBL} ${.IMPSRC} | ${NROFF} -mandoc > ${.TARGET}"
	@${TBL} ${.IMPSRC} | ${NROFF} -mandoc > ${.TARGET} || (rm -f ${.TARGET}; false)

.if defined(MAN) && !empty(MAN) && !defined(MANALL)
MANALL=	${MAN:S/.1$/.cat1/g:S/.2$/.cat2/g:S/.3$/.cat3/g:S/.3p$/.cat3p/g:S/.4$/.cat4/g:S/.5$/.cat5/g:S/.6$/.cat6/g:S/.7$/.cat7/g:S/.8$/.cat8/g:S/.9$/.cat9/g:S/.1tbl$/.cat1/g:S/.2tbl$/.cat2/g:S/.3tbl$/.cat3/g:S/.4tbl$/.cat4/g:S/.5tbl$/.cat5/g:S/.6tbl$/.cat6/g:S/.7tbl$/.cat7/g:S/.8tbl$/.cat8/g:S/.9tbl$/.cat9/g}
.endif

MINSTALL=	${INSTALL} ${INSTALL_COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}
.if defined(MANZ)
# chown and chmod are done afterward automatically
MCOMPRESS=	gzip -cf
MCOMPRESSSUFFIX= .gz
.endif

maninstall:
.if defined(MANALL)
	@for page in ${MANALL}; do \
		dir=${DESTDIR}${MANDIR}$${page##*.cat}; \
		base=$${page##*/}; \
		instpage=$${dir}${MANSUBDIR}/$${base%.*}.0${MCOMPRESSSUFFIX}; \
		if [ X"${MCOMPRESS}" = X ]; then \
			echo ${MINSTALL} $$page $$instpage; \
			${MINSTALL} $$page $$instpage; \
		else \
			rm -f $$instpage; \
			echo ${MCOMPRESS} $$page \> $$instpage; \
			${MCOMPRESS} $$page > $$instpage; \
			chown ${MANOWN}:${MANGRP} $$instpage; \
			chmod ${MANMODE} $$instpage; \
		fi \
	done
.endif
.if defined(MLINKS) && !empty(MLINKS)
	@set ${MLINKS}; \
	while test $$# -ge 2; do \
		name=$$1; \
		shift; \
		dir=${DESTDIR}${MANDIR}$${name##*.}; \
		l=$${dir}${MANSUBDIR}/$${name%.*}.0${MCOMPRESSSUFFIX}; \
		name=$$1; \
		shift; \
		dir=${DESTDIR}${MANDIR}$${name##*.}; \
		t=$${dir}${MANSUBDIR}/$${name%.*}.0${MCOMPRESSSUFFIX}; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
	done
.endif

.if defined(MANALL) && !defined(MANLOCALBUILD)
all: ${MANALL}

cleandir: cleanman
cleanman:
	rm -f ${MANALL}
.endif
