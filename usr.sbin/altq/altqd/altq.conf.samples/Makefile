#	$OpenBSD: Makefile,v 1.2 2002/02/07 23:15:34 art Exp $

FILES = cbq.bandwidthtest cbq.diffedge cbq.lo0 cbq.sample0 cbq.sample1
FILES+= cbq.sample2 cbq.sharing cbq.v6 cbq.widecamp cdnr.sample
FILES+= hfsc.diffedge hfsc.grate hfsc.lo0 hfsc.pvc0 hfsc.share
FILES+= cbq.ssh_tos

BINDIR?=	/usr/share

obj all clean cleandir depend lint obj tags:

install:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
            ${DESTDIR}${BINDIR}/altq

.include <bsd.prog.mk>
