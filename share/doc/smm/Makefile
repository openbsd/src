#	$OpenBSD: Makefile,v 1.7 2004/10/10 02:44:18 mickey Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93

# Missing or not installed:
# 02.config 14.uucpimpl 15.uucpnet 16.security

DOCDIR=	/usr/share/doc/smm
FILES=	00.contents Makefile Title
SUBDIR=	01.setup 04.quotas 05.fastfs 06.nfs 17.password 18.net
.if exists(03.fsck_ffs)
SUBDIR+= 03.fsck_ffs
.endif
.if exists(07.lpd)
SUBDIR+= 07.lpd
.endif
.if exists(08.sendmailop)
SUBDIR+= 08.sendmailop
.endif
.if exists(09.sendmail)
SUBDIR+= 09.sendmail
.endif

Title.ps: ${FILES}
	groff Title > ${.TARGET}
Title.txt: ${FILES}
	groff -Tascii Title > ${.TARGET}

contents.ps: ${FILES}
	groff -ms 00.contents > ${.TARGET}
contents.txt: ${FILES}
	groff -Tascii -ms 00.contents > ${.TARGET}

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
