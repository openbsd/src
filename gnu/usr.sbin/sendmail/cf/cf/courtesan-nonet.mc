divert(-1)
#
# Sendmail 8 configuration file for a courtesan.com machine in
# an RFC1597 internal net (ie: no direct connection to outside world).
#

divert(0)dnl
VERSIONID(`$OpenBSD: courtesan-nonet.mc,v 1.2 2000/05/15 03:38:25 millert Exp $')
OSTYPE(openbsd)
dnl
dnl Pass everything to xerxes.courtesan.com for processing
FEATURE(nullclient, `xerxes.courtesan.com')dnl
