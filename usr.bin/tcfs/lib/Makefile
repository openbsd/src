#	$OpenBSD: Makefile,v 1.2 2000/06/20 08:36:17 fgsch Exp $

.PATH:		${.CURDIR}/..

LIB=	tcfs

SRCS= tcfs_dbmaint.c gentcfskey.c tcfs_keymaint.c tcfserrors.c \
	unix_auth.c tcfs_getfspath.c tcfs_getstatus.c tcfs_flags.c

NOPROFILE= yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.own.mk>
.include <bsd.lib.mk>
