#	$OpenBSD: Makefile.inc,v 1.2 1997/04/16 12:38:25 downsj Exp $

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

cleandir:	.NOTMAIN __always_make_kern_lib
	@echo cleandiring the kernel library objects
	@(cd $(KERN_DIR) ; make cleandir)

depend::	.NOTMAIN __always_make_kern_lib
	@echo depending the kernel library objects
	@(cd $(KERN_DIR) ; make depend)

__always_make_kern_lib: .NOTMAIN
