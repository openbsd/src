#	$OpenBSD: GNUmakefile,v 1.3 2002/09/03 18:40:18 markus Exp $

#
# Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
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
#	This product includes software developed by Ericsson Radio Systems.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
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

#
# This code was written under funding by Ericsson Radio Systems.
#

.CURDIR:=	$(shell pwd)

LIB=		sysdep
SRCS=		arc4random.c blowfish.c cast.c md5.c sha1.c strlcat.c strlcpy.c
NOMAN=
CFLAGS+=	-I${.CURDIR}/.. -I/usr/include/machine

lib${LIB}.a: ${SRCS:%.c=%.o}
	ar cq $@ ${SRCS:%.c=%.o}

clean:
	rm -f lib${LIB}.a ${SRCS:%.c=%.o}

cleandir: clean cleandepend

depend: .depend

.depend: ${SRCS}
	@rm -f .depend
	${MKDEP} ${CFLAGS} ${SRCS} > .depend

cleandepend:
	rm -f .depend

ifneq ($(findstring clean,$(MAKECMDGOALS)),clean)
-include .depend
endif
