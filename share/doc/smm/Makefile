#	$OpenBSD: Makefile,v 1.2 1996/04/22 01:26:42 deraadt Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93

# The following modules do not build/install:
# 10.named, 13.amd

# Missing:
# 02.config 11.timedop 12.timed

# Missing from 4.4BSD-Lite:
# 14.uucpimpl 15.uucpnet 16.security 17.password

BINDIR=	/usr/share/doc/smm
FILES=	00.contents Makefile Title
SUBDIR=	01.setup 04.quotas 05.fastfs 06.nfs 18.net
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

contents.ps: ${FILES}
	groff -ms 00.contents > ${.TARGET}

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}

.include <bsd.subdir.mk>
