#	$OpenBSD: bsd.man.mk,v 1.29 2007/11/03 10:30:40 espie Exp $
#	$NetBSD: bsd.man.mk,v 1.23 1996/02/10 07:49:33 jtc Exp $
#	@(#)bsd.man.mk	5.2 (Berkeley) 5/11/90

MANTARGET?=	cat
NROFF?=		nroff -Tascii
TBL?=		tbl
MANLINT?=	\#

.if !target(.MAIN)
.  if exists(${.CURDIR}/../Makefile.inc)
.    include "${.CURDIR}/../Makefile.inc"
.  endif

.MAIN: all
.endif

.SUFFIXES: .1 .2 .3 .3p .4 .5 .6 .7 .8 .9 \
	.1tbl .2tbl .3tbl .4tbl .5tbl .6tbl .7tbl .8tbl .9tbl \
	.cat1 .cat2 .cat3 .cat3p .cat4 .cat5 .cat6 .cat7 .cat8 .cat9 \
	.ps1 .ps2 .ps3 .ps3p .ps4 .ps5 .ps6 .ps7 .ps8 .ps9

.9.cat9 .8.cat8 .7.cat7 .6.cat6 .5.cat5 .4.cat4 .3p.cat3p .3.cat3 .2.cat2 .1.cat1:
	@echo "${NROFF} -mandoc ${.IMPSRC} > ${.TARGET}"
	@${MANLINT} ${.IMPSRC}
	@${NROFF} -mandoc ${.IMPSRC} > ${.TARGET} || (rm -f ${.TARGET}; false)

.9tbl.cat9 .8tbl.cat8 .7tbl.cat7 .6tbl.cat6 .5tbl.cat5 .4tbl.cat4 .3tbl.cat3 \
.2tbl.cat2 .1tbl.cat1:
	@echo "${TBL} ${.IMPSRC} | ${NROFF} -mandoc > ${.TARGET}"
	@${MANLINT} -tbl ${.IMPSRC}
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
.  for v s in MANALL .cat PS2ALL .ps

$v=	${MAN:S/.1$/$s1/g:S/.2$/$s2/g:S/.3$/$s3/g:S/.3p$/$s3p/g:S/.4$/$s4/g:S/.5$/$s5/g:S/.6$/$s6/g:S/.7$/$s7/g:S/.8$/$s8/g:S/.9$/$s9/g:S/.1tbl$/$s1/g:S/.2tbl$/$s2/g:S/.3tbl$/$s3/g:S/.4tbl$/$s4/g:S/.5tbl$/$s5/g:S/.6tbl$/$s6/g:S/.7tbl$/$s7/g:S/.8tbl$/$s8/g:S/.9tbl$/$s9/g}

.  endfor

.  if defined(MANPS)
PSALL=${PS2ALL}
.  endif

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

.if !defined(MCOMPRESS) || empty(MCOMPRESS)
install_manpage_fragment= \
	echo ${MINSTALL} $$page $$instpage; \
	${MINSTALL} $$page $$instpage
.else
install_manpage_fragment= \
	rm -f $$instpage; \
	echo ${MCOMPRESS} $$page \> $$instpage; \
	${MCOMPRESS} $$page > $$instpage; \
	chown ${MANOWN}:${MANGRP} $$instpage; \
	chmod ${MANMODE} $$instpage
.endif

maninstall:
.for v d s t in MANALL ${MANDIR} .cat .0 PSALL ${PSDIR} .ps .ps
.  if defined($v)
	@for page in ${$v}; do \
		set -- ${MANSUBDIR}; \
		subdir=$$1; \
		dir=${DESTDIR}$d$${page##*$s}; \
		base=$${page##*/}; \
		instpage=$${dir}$${subdir}/$${base%.*}$t${MCOMPRESSSUFFIX}; \
		${install_manpage_fragment}; \
		while test $$# -ge 2; do \
			shift; \
			extra=$${dir}$$1/$${base%.*}$t${MCOMPRESSSUFFIX}; \
			echo $$extra -\> $$instpage; \
			ln -f $$instpage $$extra; \
		done; \
	done
.  endif
.endfor

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

BEFOREMAN?=
${MANALL} ${PSALL}: ${BEFOREMAN}


cleandir: cleanman
cleanman:
	rm -f ${MANALL} ${PS2ALL}
.endif
