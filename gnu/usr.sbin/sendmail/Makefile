#	$OpenBSD: Makefile,v 1.7 2003/02/27 21:22:14 millert Exp $

# We only use these libs internally
.if !make(install)
SUBDIR= libsmutil libsmdb libsm
.else
SUBDIR=
.endif

# The bits we install
SUBDIR+=sendmail mailstats makemap praliases smrsh editmap cf/cf

# To build milter support define WANT_LIBMILTER in /etc/mk.conf (unsupported)
.if defined(WANT_LIBMILTER)
SUBDIR+= libmilter
.endif

# Extra bits we don't build/install right now
#SUBDIR+= mail.local rmail vacation

# These just get installed verbatim
.if make(install)
SUBDIR+= doc/op cf
.endif

.include <bsd.subdir.mk>
