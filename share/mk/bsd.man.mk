#	$NetBSD: bsd.man.mk,v 1.21 1995/11/25 19:53:33 perry Exp $
#	@(#)bsd.man.mk	5.2 (Berkeley) 5/11/90

MANTARGET?=	cat

.if !target(.MAIN)
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.MAIN: all
.endif

.SUFFIXES: .1 .2 .3 .4 .5 .6 .7 .8 .9 .cat1 .cat2 .cat3 .cat4 .cat5 .cat6 \
	.cat7 .cat8 .cat9

.9.cat9 .8.cat8 .7.cat7 .6.cat6 .5.cat5 .4.cat4 .3.cat3 .2.cat2 .1.cat1:
	@echo "nroff -mandoc ${.IMPSRC} > ${.TARGET}"
	@nroff -mandoc ${.IMPSRC} > ${.TARGET} || ( rm -f ${.TARGET} ; false )

.if defined(MAN) && !empty(MAN)
MANALL=	${MAN:S/.1$/.cat1/g:S/.2$/.cat2/g:S/.3$/.cat3/g:S/.4$/.cat4/g:S/.5$/.cat5/g:S/.6$/.cat6/g:S/.7$/.cat7/g:S/.8$/.cat8/g:S/.9$/.cat9/g}
.endif

MINSTALL=	install ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}
.if defined(MANZ)
# chown and chmod are done afterward automatically
MCOMPRESS=	gzip -cf
MCOMPRESSSUFFIX= .gz
.endif

maninstall:
.if defined(MANALL)
	@for page in ${MANALL}; do \
		dir=${DESTDIR}${MANDIR}`expr $$page : '.*\.cat\([1-9]\)'`; \
		instpage=$${dir}${MANSUBDIR}/`expr $$page : '\(.*\)\.cat[1-9]'`.0${MCOMPRESSSUFFIX}; \
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
		dir=${DESTDIR}${MANDIR}`expr $$name : '.*\.\(.*\)'`; \
		l=$${dir}${MANSUBDIR}/`expr $$name : '\(.*\)\..*'`.0${MCOMPRESSSUFFIX}; \
		name=$$1; \
		shift; \
		dir=${DESTDIR}${MANDIR}`expr $$name : '.*\.\(.*\)'`; \
		t=$${dir}${MANSUBDIR}/`expr $$name : '\(.*\)\..*'`.0${MCOMPRESSSUFFIX}; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
	done
.endif

.if defined(MANALL)
all: ${MANALL}

cleandir: cleanman
cleanman:
	rm -f ${MANALL}
.endif
