#	$OpenBSD: Makefile,v 1.2 1997/03/25 17:05:57 niklas Exp $
#	$NetBSD: Makefile,v 1.1.1.1 1996/11/29 23:36:30 is Exp $

LIB=	sa

DIR_SA =	${.CURDIR}/../../../../../lib/libsa
DIR_KERN =	${.CURDIR}/../../../../../lib/libkern
DIR_KERN_MD =	${.CURDIR}/../../../../../lib/libkern/arch/$(MACHINE_ARCH)

TXLT!=	cd $(.CURDIR)/../txlt; \
	printf "xxx:\n\techo \$${.OBJDIR}/txlt\n" | \
	${MAKE} -r -s -f - xxx | grep txlt

.PATH:	$(DIR_SA) $(DIR_KERN) $(DIR_KERN_MD)

CFLAGS+=-DSTANDALONE -DINSECURE ${DEBUGFLAGS}
# -DCOMPAT_UFS
CFLAGS+=-I${.CURDIR}/../../../../..  -I${.CURDIR}/../../../../../lib/libsa
CFLAGS+=-O2 -fomit-frame-pointer -fno-function-cse -m68060 -Wa,-l -Wa,-m68030
CFLAGS+=$(COPTS)

# stand routines
SRCS=	gets.c memset.c
# memcpy.c bcopy.c strerror.c

# io routines
SRCS+=	close.c lseek.c open.c read.c dev.c

# dev.c stat.c

# boot filesystems
SRCS+=	ufs.c

# kernlib routines
SRCS+=	strlen.S strcmp.S strncpy.S
#$(DIR_KERN)/bcmp.c


#ashrdi3.c bzero.S muldi3.S

NOPROFILE=
NOPIC=
OBJMACHINE=

install:

.include <bsd.lib.mk>
.include "../Makefile.txlt"
