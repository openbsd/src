#
#	$OpenBSD: Makefile,v 1.27 2004/05/03 15:18:19 drahn Exp $
#
# Copyright (c) 1997 Tobias Weingartner
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "cats")
PROG=	fdisk
SRCS=	fdisk.c user.c misc.c disk.c mbr.c part.c cmd.c manual.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
CLEANFILES+=manual.c

.include <bsd.own.mk>

.ifdef NOMAN
manual.c:
	(echo 'char manpage[] = "\\'; \
	echo "no manual\\"; \
	echo '";' ) > manual.c
.else
manual.c:	fdisk.cat8
	(echo 'char manpage[] = "\\'; \
	sed -e 's/[\\"]/\\&/g' -e 's/$$/\\n\\/' fdisk.cat8; \
	echo '";' ) > manual.c
.endif

.else
NOPROG=yes
.endif

MAN=	fdisk.8
MANSUBDIR=i386 amd64 macppc alpha cats

.include <bsd.prog.mk>
