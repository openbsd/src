#	$OpenBSD: Makefile.inc,v 1.1 1997/01/17 08:33:05 downsj Exp $

#	NOTE: $S must correspond to the top of the 'sys' tree

KERN_DIR=	$S/arch/hp300/stand/libkern

.if exists($(KERN_DIR)/${__objdir})
KERN_LIBDIR=	$(KERN_DIR)/${__objdir}
.else
KERN_LIBDIR=	$(KERN_DIR)
.endif

KERN_LIB=	$(KERN_LIBDIR)/libkern.a

$(KERN_LIB):	.NOTMAIN __always_make_kern_lib
	@echo making sure the kernel library is up to date...
	@(cd $(KERN_DIR) ; make)

clean::		.NOTMAIN __always_make_kern_lib
	@echo cleaning the kernel library objects
	@(cd $(KERN_DIR) ; make clean)

__always_make_kern_lib: .NOTMAIN
