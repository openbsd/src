#	$OpenBSD: Makefile.inc,v 1.3 1997/01/13 11:51:08 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.4 1996/10/29 00:11:27 scottr Exp $
#
#	NOTE: $S must correspond to the top of the `sys' tree

FPSPSRCDIR=	$S/arch/m68k/fpsp

FPSPOBJDIR!=	cd $(FPSPSRCDIR); \
	printf "xxx:\n\techo \$${.OBJDIR}\n" | $(MAKE) -r -s -f - xxx

FPSPOBJ=	$(FPSPOBJDIR)/fpsp.o

$(FPSPOBJ): .NOTMAIN __always_make_fpsp
	@echo making sure the fpsp is up to date...
	@(cd $(FPSPSRCDIR); $(MAKE))

FPSP!=	printf "\#ifdef FPSP\n${FPSPOBJ}\n\#endif\n" | ${CPP} -P -undef ${CPPFLAGS:M-DFPSP}

__always_make_fpsp: .NOTMAIN
