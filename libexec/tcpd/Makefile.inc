#	$OpenBSD: Makefile.inc,v 1.1 1997/02/26 06:00:34 downsj Exp $

# Configuration options for libwrap.  Keep in sync with libwrap/Makefile.
CFLAGS+=-DPROCESS_OPTIONS -DFACILITY=LOG_AUTH -DSEVERITY=LOG_INFO \
	-DRFC931_TIMEOUT=10 -DHOSTS_ACCESS -DALWAYS_HOSTNAME \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DNETGROUP -DSYS_ERRLIST_DEFINED -DREAL_DAEMON_DIR=\"/usr/libexec\"

.include "../Makefile.inc"
