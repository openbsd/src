#	$OpenBSD: Makefile.inc,v 1.1 1997/01/16 09:26:54 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.1.1.1 1996/11/29 23:36:30 is Exp $

#	NOTE: $S must correspond to the top of the 'sys' tree

SA_DIR=	$S/arch/amiga/stand/boot/libsa

.if exists($(SA_DIR)/obj.${MACHINE})
SA_LIBDIR=	$(SA_DIR)/obj.${MACHINE}
.else
SA_LIBDIR=	$(SA_DIR)
.endif

SA_LIB=	$(SA_LIBDIR)/libsa.a

$(SA_LIB):	.NOTMAIN __always_make_sa_lib
	@echo making sure the stand-alone library is up to date...
	@(cd $(SA_DIR) ; make)

clean::		.NOTMAIN __always_make_sa_lib
	@echo cleaning the stand-alone library objects
	@(cd $(SA_DIR) ; make clean)

__always_make_sa_lib: .NOTMAIN
