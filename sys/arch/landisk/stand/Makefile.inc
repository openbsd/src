#	$OpenBSD: Makefile.inc,v 1.3 2016/11/06 16:04:20 tb Exp $

.if !make(clean) && !make(cleandir) && !make(includes) && !make(libdep) && \
    !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ X$(S) = X -o -h machine ] || ln -s $(S)/arch/landisk/include machine)
	@([ X$(S) = X -o -h sh ] || ln -s $(S)/arch/sh/include sh)
.endif

CFLAGS+=-Os -fno-stack-protector -fno-builtin
CLEANFILES+=	machine sh
BINDIR=	/usr/mdec
MANSUBDIR=landisk
