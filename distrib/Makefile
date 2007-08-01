#	$OpenBSD: Makefile,v 1.32 2007/08/01 21:31:06 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=crunch
SUBDIR+=alpha amd64 armish hp300 hppa i386 landisk mac68k macppc \
	mvme68k mvme88k mvmeppc sgi sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.if	make(clean) || make(cleandir)
SUBDIR+=crunch
.endif

#all: crunch-tools _SUBDIRUSE

crunch-tools:
	(cd $(.CURDIR)/crunch; $(MAKE) obj; \
	 $(MAKE) depend && ${MAKE} && ${SUDO} ${MAKE} DESTDIR= install)

.include <bsd.subdir.mk>
