#	$OpenBSD: Makefile,v 1.8 1997/11/29 01:22:49 millert Exp $
#
# Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by SigmaSoft, Th.  Lockert.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
# THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

LIB=		termlib
SRCS=		captoinfo.c def_prog_mode.c def_shell_mode.c del_curterm.c \
		fillcap.c findcap.c findinfo.c getterm.c globals.c \
		mvcur.c _mvcur.c \
		outc.c putp.c reset_prog_mode.c reset_shell_mode.c \
		restartterm.c setterm.c setupterm.c set_curterm.c tgetent.c \
		tgetflag.c tgetnum.c tgetstr.c tgoto.c tigetflag.c tigetnum.c \
		tigetstr.c tparm.c tputs.c tty_init.c \
		vidattr.c _vidattr.c \
		vidputs.c _vidputs.c \
		captoidx.c infotoidx.c \
		boolnames.c boolcodes.c boolfnames.c \
		numnames.c  numcodes.c  numfnames.c \
		strnames.c  strcodes.c  strfnames.c
HDRS=		term.h
MAN=		terminfo.3
MLINKS+=	terminfo.3 setupterm.3 \
		terminfo.3 setterm.3 \
		terminfo.3 set_curterm.3 \
		terminfo.3 del_curterm.3 \
		terminfo.3 restartterm.3 \
		terminfo.3 tparm.3 \
		terminfo.3 tparam.3 \
		terminfo.3 tputs.3 \
		terminfo.3 putp.3 \
		terminfo.3 vidputs.3 \
		terminfo.3 vidattr.3 \
		terminfo.3 mvcur.3 \
		terminfo.3 tigetflag.3 \
		terminfo.3 tigetnum.3 \
		terminfo.3 tigetstr.3
CLEANFILES+=	${HDRS} binaries.h captoidx.c infotoidx.c mkinfo \
		boolnames.c boolcodes.c boolfnames.c \
		numnames.c  numcodes.c  numfnames.c \
		strnames.c  strcodes.c  strfnames.c
CFLAGS+=	-I. -I${.CURDIR}

beforedepend:	${HDRS} binaries.h

term.h:		mkcaps.sh term.h.head Capabilities term.h.tail
	sh ${.CURDIR}/mkcaps.sh ${.CURDIR}/term.h.head \
				${.CURDIR}/Capabilities \
				${.CURDIR}/term.h.tail >$@

boolnames.c boolcodes.c boolfnames.c \
numnames.c  numcodes.c  numfnames.c \
strnames.c  strcodes.c  strfnames.c \
captoidx.c infotoidx.c \
binaries.h:	mkinfo Capabilities
	./mkinfo ${.CURDIR}/Capabilities

includes:	${HDRS}
	@for i in ${HDRS}; do \
	    cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include; \
	done

.include <bsd.lib.mk>

mkinfo:	mkinfo.c
	${HOSTCC} ${CFLAGS} -I${.CURDIR} -o mkinfo $>

.depend:	mkinfo.c
