#	$OpenBSD: Makefile.inc,v 1.2 2007/11/26 10:26:02 deraadt Exp $

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(obj) && !make(clean) && !make(cleandir)
.BEGIN:
	@([ X$(S) = X -o -h machine ] || ln -s $(S)/arch/landisk/include machine)
	@([ X$(S) = X -o -h sh ] || ln -s $(S)/arch/sh/include sh)
.endif

CFLAGS+=-Os -fno-stack-protector -fno-builtin
CLEANFILES+=	machine sh
BINDIR=	/usr/mdec
MANSUBDIR=landisk
