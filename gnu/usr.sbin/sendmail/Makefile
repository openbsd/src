#	$OpenBSD: Makefile,v 1.6 2003/02/19 06:18:10 millert Exp $

# We only use these libs internally
.if !make(install)
SUBDIR= libsmutil libsmdb libsm
.else
SUBDIR=
.endif

# The bits we install
SUBDIR+=sendmail mailstats makemap praliases smrsh editmap cf/cf

# To build with mail filter support (untested)
#SUBDIR+= libmilter
#.MAKEFLAGS+="WANT_LIBMILTER=1"

# Extra bits we don't build/install right now
#SUBDIR+= mail.local rmail vacation

# These just get installed verbatim
.if make(install)
SUBDIR+= doc/op cf
.endif

.include <bsd.subdir.mk>
