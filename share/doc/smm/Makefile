#	$OpenBSD: Makefile,v 1.4 2003/11/10 20:32:17 otto Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93

# The following modules do not build/install:
# 10.named, 13.amd

# Missing:
# 02.config

# Missing from 4.4BSD-Lite:
# 14.uucpimpl 15.uucpnet 16.security

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

contents.ps: ${FILES}
	groff -ms 00.contents > ${.TARGET}

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
