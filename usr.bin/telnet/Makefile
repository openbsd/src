#	$OpenBSD: Makefile,v 1.23 2005/05/24 03:41:58 deraadt Exp $
#
# Copyright (c) 1990 The Regents of the University of California.
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
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#

PROG=	telnet

CFLAGS+=-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DSKEY -Dunix -I${.CURDIR}
CFLAGS+= -DHAS_CGETENT -Wall -Wno-unused
LDADD+=	-lcurses
DPADD=	${LIBCURSES}

SRCS=	authenc.c commands.c main.c network.c ring.c sys_bsd.c telnet.c \
	terminal.c tn3270.c utilities.c auth.c encrypt.c genget.c getent.c \
	misc.c

.include <bsd.own.mk> # for KERBEROS

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DENCRYPTION -DAUTHENTICATION -DKRB5 -DDES_ENCRYPTION
DPADD+= ${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBDES}
LDADD+= -lkrb5 -lasn1 -lcrypto -ldes
SRCS+= kerberos5.c enc_des.c
.endif

.include <bsd.prog.mk>
