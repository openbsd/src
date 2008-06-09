divert(-1)
#
# Sendmail configuration file for lists.openbsd.org
#
# This config accepts bulk mail from mj2 on port 24 and delivers it.
#

divert(0)dnl
VERSIONID(`$OpenBSD: openbsd-bulk.mc,v 1.4 2008/06/09 14:47:01 millert Exp $')
OSTYPE(openbsd)dnl
dnl
dnl Advertise ourselves as ``openbsd.org''
define(`confSMTP_LOGIN_MSG', `openbsd.org Sendmail $v/$Z/bulk ready willing and able at $b')dnl
dnl
dnl Override some default values
define(`confTRY_NULL_MX_LIST', `True')dnl
define(`confMAX_HOP', `30')dnl
define(`confQUEUE_LA', `25')dnl
define(`confREFUSE_LA', `100')dnl
dnl
dnl Disable ident queries
define(`confTO_IDENT', `0')dnl
dnl
dnl Some alternate paths so we don't conflict with sendmail on port 25
define(`confPID_FILE', `/var/run/bulkmail.pid')dnl
dnl
dnl Wait at least 27 minutes before trying to redeliver a message.
define(`confMIN_QUEUE_AGE', `27m')dnl
dnl
dnl Just queue incoming messages, we have queue runners for actual delivery
define(`confDELIVERY_MODE', `q')dnl
dnl
dnl Don't prioritize a message based on the number of recepients
dnl or Precedence header.  We only care about message size and
dnl number of retries.
define(`confWORK_RECIPIENT_FACTOR', `0')dnl
define(`confWORK_CLASS_FACTOR', `0')dnl
define(`confRETRY_FACTOR', `90000')dnl
dnl
dnl One queue group, many dirs, max 90 runners
define(`confMAX_QUEUE_CHILDREN', `90')
QUEUE_GROUP(`mqueue', `P=/var/spool/mqueue/bulk*, R=5, r=10, F=f I=1')dnl
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
dnl Always use fully qualified domains
FEATURE(always_add_domain)dnl
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
dnl for delivery and disable canonification.
DAEMON_OPTIONS(`Family=inet6, address=::1, Name=MTA6, Port=24, M=COS')dnl
DAEMON_OPTIONS(`Family=inet, address=127.0.0.1, Name=MTA, Port=24, M=CS')dnl
CLIENT_OPTIONS(`Family=inet6, Address=::')dnl
CLIENT_OPTIONS(`Family=inet, Address=0.0.0.0')dnl
