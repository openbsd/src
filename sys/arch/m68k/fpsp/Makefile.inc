#	$NetBSD: Makefile.inc,v 1.2 1994/10/26 07:48:47 cgd Exp $
#
#	NOTE: $S must correspond to the top of the `sys' tree

FPSPSRCDIR=	$S/arch/m68k/fpsp

FPSPOBJDIR!=	cd $(FPSPSRCDIR); \
	printf "xxx:\n\techo \$${.OBJDIR}\n" | $(MAKE) -r -s -f - xxx

FPSPOBJ=	$(FPSPOBJDIR)/fpsp.o

$(FPSPOBJ): .NOTMAIN __always_make_fpsp
	@echo making sure the fpsp is up to date...
	@(cd $(FPSPSRCDIR) ; $(MAKE))

FPSP!=	printf "\#ifdef FPSP\n${FPSPOBJ}\n\#endif\n" | cpp -P -undef ${COPTS:M-DFPSP}

__always_make_fpsp: .NOTMAIN
