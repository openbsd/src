divert(-1)
#
# Sendmail configuration file for lists.openbsd.org
#
# This config just accepts bulk mail from mj2 on port 24 and delivers it.
# It is a queue-only config--we use persistent queue runners to do the
# actual delivery.
#
# The queue runners are started from rc.local as follows:
#
# set -- q0 50 modification 1s q1 5 host 1m q2 5 host 2m q3 5 host 5m qold 10 host 10m
# _key=8675309
# while test $# -ge 4; do
#	/usr/sbin/sendmail -C/etc/mail/bulk.cf -Lsm-queue -OQueueSortOrder=$3 \
#	    -OMaxQueueChildren=$2 -OQueueDirectory=/var/spool/mqueue/$1 \
#	    -OProcessTitlePrefix=$1 -OPidFile=/var/run/runner-$1.pid \
#	    -OSharedMemoryKey=$_key -q$4
#	_key=$(( $_key + 10 ))
#	shift 4
# done
#
# A cron job moves failed messages progressively from q0 -> qold
#

divert(0)dnl
VERSIONID(`$OpenBSD: openbsd-bulk.mc,v 1.2 2005/01/06 17:21:03 millert Exp $')
OSTYPE(openbsd)dnl
dnl
dnl Advertise ourselves as ``openbsd.org''
define(`confSMTP_LOGIN_MSG', `openbsd.org Sendmail $v/$Z/bulk ready willing and able at $b')dnl
dnl
dnl Override some default values
define(`confDELIVERY_MODE', `q')dnl
define(`confTRY_NULL_MX_LIST', `True')dnl
define(`confMAX_HOP', `30')dnl
define(`confQUEUE_LA', `25')dnl
define(`confREFUSE_LA', `100')dnl
dnl
dnl Some alternate paths so we don't conflict with sendmail on port 25
define(`confPID_FILE', `/var/run/bulkmail.pid')dnl
define(`QUEUE_DIR', `/var/spool/mqueue/q0')dnl
dnl
dnl Add a prefix to differentiate outgoing bulk messages from incoming ones
define(`confPROCESS_TITLE_PREFIX', `bulk')dnl
dnl
dnl Resolver options:
dnl	WorkAroundBrokenAAAA works around some broken nameservers that
dnl	return SERVFAIL (a temporary failure) on T_AAAA (IPv6) lookups.
dnl	We turn off DNSRCH and DEFNAMES since we are always passed
dnl	qualified hostname (this saves us some DNS traffic).
define(`confBIND_OPTS', `WorkAroundBrokenAAAA -DNSRCH -DEFNAMES')dnl
dnl
dnl Keep host status on disk between sendmail runs in the .hoststat dir
define(`confHOST_STATUS_DIRECTORY', `/var/spool/mqueue/.hoststat')dnl
define(`confTO_HOSTSTATUS', `30m')dnl
dnl
dnl Wait at least 27 minutes before trying to redeliver a message.
define(`confMIN_QUEUE_AGE', `27m')dnl
dnl
dnl Don't prioritize a message based on the number of recepients.
dnl This prevents retries from having higher priority than new batches.
define(`confWORK_RECIPIENT_FACTOR', `0')dnl
dnl
dnl Reduce ClassFactor
define(`confWORK_CLASS_FACTOR', `1000')dnl
dnl
dnl Always use fully qualified domains
FEATURE(always_add_domain)dnl
dnl
dnl No need to do DNS lookups on addresses, they've already been done
FEATURE(nocanonify)dnl
dnl
dnl Wait a day before sending mail about deferred messages
define(`confTO_QUEUEWARN', `1d')dnl
dnl
dnl Wait 3 days before giving up and bouncing the message
define(`confTO_QUEUERETURN', `3d')dnl
dnl
dnl Shared memory key used to stash disk usage stats so they
dnl don't have to be checked by each sendmail process.
define(`confSHARED_MEMORY_KEY', `696969')dnl
dnl
dnl SSL certificate paths
define(`CERT_DIR', `MAIL_SETTINGS_DIR`'certs')dnl
define(`confCACERT_PATH', `CERT_DIR')dnl
define(`confCACERT', `CERT_DIR/mycert.pem')dnl
define(`confSERVER_CERT', `CERT_DIR/mycert.pem')dnl
define(`confSERVER_KEY', `CERT_DIR/mykey.pem')dnl
define(`confCLIENT_CERT', `CERT_DIR/mycert.pem')dnl
define(`confCLIENT_KEY', `CERT_DIR/mykey.pem')dnl
dnl
dnl List of hostname we treat as local
FEATURE(use_cw_file)dnl
dnl
dnl Make mail appear to be from openbsd.org
MASQUERADE_AS(openbsd.org)dnl
FEATURE(masquerade_envelope)dnl
dnl
dnl Need this so we can deal with user@openbsd.org
dnl XXX - could deliver to real daemon instead (and kill cw stuff as well)
FEATURE(stickyhost)dnl
FEATURE(virtusertable)dnl
dnl
dnl List the mailers we support
FEATURE(`no_default_msa')dnl
MAILER(local)dnl
MAILER(smtp)dnl
dnl
dnl Only accept connections from localhost on port 24, use ipv6 or ipv4
dnl for delivery.
DAEMON_OPTIONS(`Family=inet6, address=::1, Name=MTA6, Port=24, M=OS')dnl
DAEMON_OPTIONS(`Family=inet, address=127.0.0.1, Name=MTA, Port=24, M=S')dnl
CLIENT_OPTIONS(`Family=inet6, Address=::')dnl
CLIENT_OPTIONS(`Family=inet, Address=0.0.0.0')dnl
