WRTVID_BASE_DIR=${S}/arch/${MACHINE}/stand/wrtvid

WRTVID_DIR!= cd ${WRTVID_BASE_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

WRTVID=${WRTVID_DIR}/wrtvid

$(WRTVID):	.NOTMAIN __always_make_WRTVID
	@echo making sure the wrtvid is up to date...
	@(cd ${WRTVID_BASE_DIR}; ${MAKE})

__always_make_WRTVID:	.NOTMAIN
