#
#	$OpenBSD: Makefile.inc,v 1.2 1996/05/30 22:13:56 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.1 1996/05/15 21:06:19 is Exp $
#
#	NOTE: $S must correspond to the top of the `sys' tree

060SPSRCDIR=	$S/arch/m68k/060sp

060SPOBJDIR!=	cd $(060SPSRCDIR); \
	printf "xxx:\n\techo \$${.OBJDIR}\n" | $(MAKE) -r -s -f - xxx

060SPOBJ=	$(060SPOBJDIR)/060sp.o

$(060SPOBJ): .NOTMAIN __always_make_060sp
	@echo making sure the 060sp is up to date...
	@(cd $(060SPSRCDIR) ; $(MAKE))

060SP!=	printf "\#ifdef M060SP\n${060SPOBJ}\n\#endif\n" | cpp -P -undef ${CPPFLAGS:M-DM060SP}

__always_make_060sp: .NOTMAIN
