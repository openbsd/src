LIB_BUG_DIR=${S}/arch/${MACHINE}/stand/libbug

LIBBUG_DIR!= cd ${LIB_BUG_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

LIBBUG=${LIBBUG_DIR}/libbug.a

$(LIBBUG):	.NOTMAIN __always_make_libbug
	@echo making sure the libbug is up to date...
	@(cd ${LIB_BUG_DIR}; ${MAKE})

__always_make_libbug:	.NOTMAIN
