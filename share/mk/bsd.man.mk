#	$OpenBSD: bsd.man.mk,v 1.23 2001/07/20 23:02:21 espie Exp $
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
	.7tbl .8tbl .9tbl .cat1 .cat2 .cat3 .cat4 .cat5 .cat6 .cat7 .cat8 .cat9 \
	.ps1 .ps2 .ps3 .ps4 .ps5 .ps6 .ps7 .ps8 .ps9

.9.cat9 .8.cat8 .7.cat7 .6.cat6 .5.cat5 .4.cat4 .3p.cat3p .3.cat3 .2.cat2 .1.cat1:
	@echo "${NROFF} -mandoc ${.IMPSRC} > ${.TARGET}"
	@${NROFF} -mandoc ${.IMPSRC} > ${.TARGET} || (rm -f ${.TARGET}; false)

.9tbl.cat9 .8tbl.cat8 .7tbl.cat7 .6tbl.cat6 .5tbl.cat5 .4tbl.cat4 .3tbl.cat3 \
.2tbl.cat2 .1tbl.cat1:
	@echo "${TBL} ${.IMPSRC} | ${NROFF} -mandoc > ${.TARGET}"
	@${TBL} ${.IMPSRC} | ${NROFF} -mandoc > ${.TARGET} || \
	    (rm -f ${.TARGET}; false)

.9.ps9 .8.ps8 .7.ps7 .6.ps6 .5.ps5 .4.ps4 .3p.ps3p .3.ps3 .2.ps2 .1.ps1:
	@echo "nroff -Tps -mandoc ${.IMPSRC} > ${.TARGET}"
	@nroff -Tps -mandoc ${.IMPSRC} > ${.TARGET} || (rm -f ${.TARGET}; false)

.9tbl.ps9 .8tbl.ps8 .7tbl.ps7 .6tbl.ps6 .5tbl.ps5 .4tbl.ps4 .3tbl.ps3 \
.2tbl.ps2 .1tbl.ps1:
	@echo "${TBL} ${.IMPSRC} | nroff -Tps -mandoc > ${.TARGET}"
	@${TBL} ${.IMPSRC} | nroff -Tps -mandoc > ${.TARGET} || (rm -f ${.TARGET}; false)

.if defined(MAN) && !empty(MAN) && !defined(MANALL)

MANALL=	${MAN:S/.1$/.cat1/g:S/.2$/.cat2/g:S/.3$/.cat3/g:S/.3p$/.cat3p/g:S/.4$/.cat4/g:S/.5$/.cat5/g:S/.6$/.cat6/g:S/.7$/.cat7/g:S/.8$/.cat8/g:S/.9$/.cat9/g:S/.1tbl$/.cat1/g:S/.2tbl$/.cat2/g:S/.3tbl$/.cat3/g:S/.4tbl$/.cat4/g:S/.5tbl$/.cat5/g:S/.6tbl$/.cat6/g:S/.7tbl$/.cat7/g:S/.8tbl$/.cat8/g:S/.9tbl$/.cat9/g}

.if defined(MANPS)

PSALL= ${MAN:S/.1$/.ps1/g:S/.2$/.ps2/g:S/.3$/.ps3/g:S/.3p$/.ps3p/g:S/.4$/.ps4/g:S/.5$/.ps5/g:S/.6$/.ps6/g:S/.7$/.ps7/g:S/.8$/.ps8/g:S/.9$/.ps9/g:S/.1tbl$/.ps1/g:S/.2tbl$/.ps2/g:S/.3tbl$/.ps3/g:S/.4tbl$/.ps4/g:S/.5tbl$/.ps5/g:S/.6tbl$/.ps6/g:S/.7tbl$/.ps7/g:S/.8tbl$/.ps8/g:S/.9tbl$/.ps9/g}

.endif

.endif

MINSTALL=	${INSTALL} ${INSTALL_COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}
.if defined(MANZ)
# chown and chmod are done afterward automatically
MCOMPRESS=	gzip -cf
MCOMPRESSSUFFIX= .gz
.endif

.if defined(MANSUBDIR)
# Add / so that we don't have to specify it. Better arch -> MANSUBDIR mapping
MANSUBDIR:=${MANSUBDIR:S,^,/,}
.else
# XXX MANSUBDIR must be non empty for the mlink loops to work
MANSUBDIR=''
.endif

maninstall:
.if defined(MANALL)
	@for page in ${MANALL}; do \
		set -- ${MANSUBDIR}; \
		subdir=$$1; \
		dir=${DESTDIR}${MANDIR}$${page##*.cat}; \
		base=$${page##*/}; \
		instpage=$${dir}$${subdir}/$${base%.*}.0${MCOMPRESSSUFFIX}; \
		if [ X"${MCOMPRESS}" = X ]; then \
			echo ${MINSTALL} $$page $$instpage; \
			${MINSTALL} $$page $$instpage; \
		else \
			rm -f $$instpage; \
			echo ${MCOMPRESS} $$page \> $$instpage; \
			${MCOMPRESS} $$page > $$instpage; \
			chown ${MANOWN}:${MANGRP} $$instpage; \
			chmod ${MANMODE} $$instpage; \
		fi; \
		while test $$# -ge 2; do \
			shift; \
			extra=$${dir}$$1/$${base%.*}.0${MCOMPRESSSUFFIX}; \
			echo $$extra -\> $$instpage; \
			ln -f $$instpage $$extra; \
		done; \
	done
.endif
.if defined(PSALL)
	@for page in ${PSALL}; do \
		set -- ${MANSUBDIR}; \
		subdir=$$1; \
		dir=${DESTDIR}${PSDIR}$${page##*.ps}; \
		base=$${page##*/}; \
		instpage=$${dir}$${subdir}/$${base%.*}.ps${MCOMPRESSSUFFIX}; \
		if [ X"${MCOMPRESS}" = X ]; then \
			echo ${MINSTALL} $$page $$instpage; \
			${MINSTALL} $$page $$instpage; \
		else \
			rm -f $$instpage; \
			echo ${MCOMPRESS} $$page \> $$instpage; \
			${MCOMPRESS} $$page > $$instpage; \
			chown ${PSOWN}:${PSGRP} $$instpage; \
			chmod ${PSMODE} $$instpage; \
		fi; \
		while test $$# -ge 2; do \
			shift; \
			extra=$${dir}$$1/$${base%.*}.ps${MCOMPRESSSUFFIX}; \
			echo $$extra -\> $$instpage; \
			ln -f $$instpage $$extra; \
		done; \
	done
.endif
.if defined(MLINKS) && !empty(MLINKS)
.  for sub in ${MANSUBDIR}
.     for lnk file in ${MLINKS}
	@l=${DESTDIR}${MANDIR}${lnk:E}${sub}/${lnk:R}.0${MCOMPRESSSUFFIX}; \
	t=${DESTDIR}${MANDIR}${file:E}${sub}/${file:R}.0${MCOMPRESSSUFFIX}; \
	echo $$t -\> $$l; \
	rm -f $$t; ln $$l $$t;
.     endfor
.  endfor
.endif

.if (defined(MANALL) || defined(PSALL)) && !defined(MANLOCALBUILD)
all: ${MANALL} ${PSALL}

cleandir: cleanman
cleanman:
	rm -f ${MANALL} ${PSALL}
.endif
