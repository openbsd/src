#	$OpenBSD: Makefile.inc,v 1.2 1997/03/04 09:01:39 downsj Exp $

# Configuration options for libwrap.  Keep in sync with libwrap/Makefile.
CFLAGS+=-DPROCESS_OPTIONS -DFACILITY=LOG_AUTH -DSEVERITY=LOG_INFO \
	-DRFC931_TIMEOUT=10 -DHOSTS_ACCESS -DALWAYS_HOSTNAME \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DNETGROUP -DSYS_ERRLIST_DEFINED -DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-D_TCPD_PRIVATE

.include "../Makefile.inc"
