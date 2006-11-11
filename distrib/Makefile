#	$OpenBSD: Makefile,v 1.30 2006/11/11 21:09:22 deraadt Exp $

SUBDIR=	special

.if	make(obj)
SUBDIR+=crunch
SUBDIR+=alpha amd64 armish cats hp300 hppa i386 landisk mac68k macppc \
	mvme68k mvme88k mvmeppc sgi sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.if	make(clean) || make(cleandir)
SUBDIR+=crunch
.endif

SUBDIR+=notes

#all: crunch-tools _SUBDIRUSE

crunch-tools:
	(cd $(.CURDIR)/crunch; $(MAKE) obj; \
	 $(MAKE) depend && ${MAKE} && ${SUDO} ${MAKE} DESTDIR= install)

.include <bsd.subdir.mk>
