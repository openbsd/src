divert(-1)
#
# Sendmail 8 configuration file for a courtesan.com machine in
# an RFC1597 internal net (ie: no direct connection to outside world).
#

VERSIONID(`$OpenBSD: courtesan-nonet.mc,v 1.1 2000/04/02 19:48:11 millert Exp $')
OSTYPE(openbsd)
dnl
dnl Pass everything to xerxes.courtesan.com for processing
FEATURE(nullclient, `xerxes.courtesan.com')dnl
