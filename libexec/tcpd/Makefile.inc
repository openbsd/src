#	$OpenBSD: Makefile.inc,v 1.3 2000/10/14 00:56:14 itojun Exp $

# Configuration options for libwrap.  Keep in sync with libwrap/Makefile.
CFLAGS+=-DPROCESS_OPTIONS -DFACILITY=LOG_AUTH -DSEVERITY=LOG_INFO \
	-DRFC931_TIMEOUT=10 -DHOSTS_ACCESS -DALWAYS_HOSTNAME \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DNETGROUP -DSYS_ERRLIST_DEFINED -DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-D_TCPD_PRIVATE
CFLAGS+=-DINET6

.include "../Makefile.inc"
