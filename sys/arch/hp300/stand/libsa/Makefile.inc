#	$OpenBSD: Makefile.inc,v 1.3 1997/04/16 12:38:26 downsj Exp $
#	$NetBSD: Makefile.inc,v 1.1 1995/08/04 07:55:50 thorpej Exp $

#	NOTE: $S must correspond to the top of the 'sys' tree

SA_DIR=	$S/arch/hp300/stand/libsa

.if exists($(SA_DIR)/${__objdir})
SA_LIBDIR=	$(SA_DIR)/${__objdir}
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

cleandir:	.NOTMAIN __always_make_sa_lib
	@echo cleandiring the stand-alone library objects
	@(cd $(SA_DIR) ; make cleandir)

depend::	.NOTMAIN __always_make_sa_lib
	@echo depending the stand-alone library objects
	@(cd $(SA_DIR) ; make depend)

__always_make_sa_lib: .NOTMAIN
