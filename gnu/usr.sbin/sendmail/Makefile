#	$OpenBSD: Makefile,v 1.5 2002/07/20 17:30:26 millert Exp $

# We only use these libs internally
.if !make(install)
SUBDIR= libsmutil libsmdb libsm
.if defined(WANT_LIBMILTER)
SUBDIR+= libmilter
.endif
.else
SUBDIR=
.endif

# The bits we install
SUBDIR+=sendmail mailstats makemap praliases smrsh editmap cf/cf

# Extra bits we don't build/install right now
#SUBDIR+= libmilter mail.local rmail vacation

# These just get installed verbatim
.if make(install)
SUBDIR+= doc/op cf
.endif

.include <bsd.subdir.mk>
