#
#	$OpenBSD: Makefile.inc,v 1.3 1997/07/06 07:46:18 downsj Exp $
#	$NetBSD: Makefile.inc,v 1.2 1997/05/07 07:15:43 mycroft Exp $
#
#	NOTE: $S must correspond to the top of the `sys' tree

060SPSRCDIR=	$S/arch/m68k/060sp

060SPOBJDIR!=	cd $(060SPSRCDIR); \
	printf "xxx: .MAKE\n\t@echo \$${.OBJDIR}\n" | $(MAKE) -s -f-

060SPOBJ=	$(060SPOBJDIR)/060sp.o

$(060SPOBJ): .NOTMAIN __always_make_060sp
	@echo making sure the 060sp is up to date...
	@(cd $(060SPSRCDIR) ; $(MAKE))

060SP!=	printf "\#ifdef M060SP\n${060SPOBJ}\n\#endif\n" | cpp -P -undef ${CPPFLAGS:M-DM060SP}

__always_make_060sp: .NOTMAIN
