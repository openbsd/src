#	$OpenBSD: Makefile,v 1.5 1998/11/01 01:38:13 smurph Exp $
#	$NetBSD: Makefile,v 1.25 1997/10/25 10:10:35 lukem Exp $
#	@(#)Makefile	6.1 (Berkeley) 3/3/91
TARGET_MACHINE_ARCH?=	$(MACHINE_ARCH)

.if	(${TARGET_MACHINE_ARCH} == "i386") || \
	(${TARGET_MACHINE_ARCH} == "m68k") || \
	(${TARGET_MACHINE_ARCH} == "ns32k") || \
	(${TARGET_MACHINE_ARCH} == "sparc") || \
	(${TARGET_MACHINE_ARCH} == "vax") || \
	(${TARGET_MACHINE_ARCH} == "mvme88k") 
.if exists(config/Makefile.$(TARGET_MACHINE_ARCH))
.include "config/Makefile.$(TARGET_MACHINE_ARCH)"
.endif

.if !defined (gas_hosttype)
gas_hosttype=$(MACHINE_ARCH)
.endif
.if !defined (gas_target)
gas_target=$(TARGET_MACHINE_ARCH)
.endif
.if !defined (gas_objformat)
gas_objformat=aout
.endif

.if ($(TARGET_MACHINE_ARCH) != $(MACHINE_ARCH))
CPPFLAGS+=	-DCROSS_COMPILE
.endif

ADDINCLUDE=-I${.OBJDIR}

PROG=		as
SRCS+=		app.c as.c atof-generic.c bignum-copy.c \
		cond.c expr.c flo-const.c flo-copy.c flonum-mult.c \
		frags.c hash.c hex-value.c input-file.c input-scrub.c \
		listing.c messages.c obstack.c output-file.c read.c subsegs.c \
		symbols.c version.c write.c xmalloc.c xrealloc.c \
		obj-$(gas_objformat).c
CPPFLAGS+=	-I$(.CURDIR) ${ADDINCLUDE} -I$(.CURDIR)/config \
		-DOLD_GAS -DSIGTY=void -Derror=as_fatal \
		-DNETBSD_AOUT

CONF_HEADERS=	targ-cpu.h obj-format.h host.h targ-env.h

.PATH: $(.CURDIR)/config

beforedepend ${PROG}: ${CONF_HEADERS}

targ-cpu.h: Makefile config/Makefile.$(TARGET_MACHINE_ARCH)
	@cmp -s $(.CURDIR)/config/tc-$(gas_target).h targ-cpu.h || \
	    ( echo "updating ${.TARGET}..." ; /bin/rm -f targ-cpu.h ; \
	    cp $(.CURDIR)/config/tc-$(gas_target).h targ-cpu.h )

obj-format.h: Makefile config/Makefile.$(TARGET_MACHINE_ARCH)
	@cmp -s $(.CURDIR)/config/obj-$(gas_objformat).h obj-format.h || \
	    ( echo "updating ${.TARGET}..." ; /bin/rm -f obj-format.h ; \
	    cp $(.CURDIR)/config/obj-$(gas_objformat).h obj-format.h )

.if exists ($(.CURDIR)/config/ho-$(gas_hosttype).h)
config_hostfile=	$(.CURDIR)/config/ho-$(gas_hosttype).h
.else
config_hostfile=	$(.CURDIR)/config/ho-generic.h
.endif

host.h: $(config_hostfile)
	@cmp -s $(config_hostfile) host.h || \
	    ( echo "updating ${.TARGET}..." ; /bin/rm -f host.h ; \
	    cp $(config_hostfile) host.h )

.if exists ($(.CURDIR)/config/te-$(TARGET_MACHINE_ARCH).h)
config_targenvfile=	$(.CURDIR)/config/te-$(TARGET_MACHINE_ARCH).h
.else
config_targenvfile=	$(.CURDIR)/config/te-generic.h
.endif

targ-env.h: Makefile config/Makefile.$(TARGET_MACHINE_ARCH)
	@cmp -s $(config_targenvfile) targ-env.h || \
	    ( echo "updating ${.TARGET}..." ; /bin/rm -f targ-env.h ; \
	    cp $(config_targenvfile) targ-env.h )

CLEANFILES+= ${CONF_HEADERS}

.endif

MAN=	as.1

.include <bsd.prog.mk>
