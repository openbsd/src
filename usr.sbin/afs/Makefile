#	$OpenBSD: Makefile,v 1.4 2002/06/07 05:02:58 hin Exp $

# To the future maintainer (if I get hit by a car or something): The structure
# is like this (everything in src, except the makefiles), because we want
# the upgrade process to be easy, without building and installing 5 different
# libs and without making a hairy Makefile.bsd-wrapper
#
# Anomalies:
#  - config.h and arla-version.h have to be pre-built and put in src/include.
#    config.h contains macros for endianness, but they are only used in one
#    place and will be replaced on OpenBSD. They have to be placed in
#    src/include because we want to reduce the memory footprint of mkdep
#    (yes, it's a real problem).
#

.if !make(install)
SUBDIR		+= usr.sbin/ydr
.endif
SUBDIR		+= lib libexec usr.sbin

## To be able to make depend in libarla we have to have ydr (that needs
## libroken) and libroken used by gensysname in (libarla/Makefile.ko.inc)

beforedepend:
	cd usr.sbin/ydr && make depend && exec make

.include <bsd.dep.mk>
.include <bsd.subdir.mk>
