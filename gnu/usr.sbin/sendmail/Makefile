#	$OpenBSD: Makefile,v 1.10 2010/10/17 22:54:37 schwarze Exp $

# We only use these libs internally
.if !make(install)
SUBDIR= libsmutil libsmdb libsm
.else
SUBDIR=
.endif

# The bits we install
SUBDIR+= libmilter sendmail mailstats makemap praliases smrsh editmap cf/cf

# Extra bits we don't build/install right now
#SUBDIR+= mail.local rmail vacation

# These just get installed verbatim
.if make(install)
SUBDIR+= cf
.endif

.include <bsd.subdir.mk>
