divert(-1)
#
# Sendmail 8 configuration file for a courtesan.com machine in
# an RFC1597 internal net (ie: no direct connection to outside world).
#

divert(0)dnl
VERSIONID(`$OpenBSD: courtesan-nonet.mc,v 1.3 2006/03/22 18:43:53 millert Exp $')
OSTYPE(openbsd)
dnl
dnl Pass everything to xxx.courtesan.com for processing
FEATURE(nullclient, `xxx.courtesan.com')dnl
