divert(-1)
#
# Sendmail configuration file for lists.openbsd.org
#
# This config handles incoming mail for openbsd.{org,com,net}
# Mailing list fanout is handled by a separate exploder running on
# port 24 that is fed by mj2.
#

divert(0)dnl
VERSIONID(`$OpenBSD: openbsd-lists.mc,v 1.15 2003/11/10 19:11:55 millert Exp $')
OSTYPE(openbsd)dnl
dnl
dnl Advertise ourselves as ``openbsd.org''
define(`confSMTP_LOGIN_MSG', `openbsd.org Sendmail $v/$Z/millert ready willing and able at $b')dnl
dnl
dnl Override some default values
define(`confPRIVACY_FLAGS', `authwarnings,needmailhelo,noexpn,novrfy,noetrn,noverb,nobodyreturn')dnl
define(`confTRY_NULL_MX_LIST', `True')dnl
define(`confMAX_HOP', `30')dnl
define(`confQUEUE_LA', `6')dnl
define(`confREFUSE_LA', `100')dnl
dnl
dnl Some broken nameservers will return SERVFAIL (a temporary failure)
dnl on T_AAAA (IPv6) lookups.
define(`confBIND_OPTS', `WorkAroundBrokenAAAA')dnl
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
dnl Need to add domo and mj2 as "trusted users" to rewrite From lines
define(`confTRUSTED_USERS', `domo mj2')dnl
dnl
dnl Wait a day before sending mail about deferred messages
define(`confTO_QUEUEWARN', `1d')dnl
dnl
dnl Wait 3 days before giving up and bouncing the message
define(`confTO_QUEUERETURN', `3d')dnl
dnl
dnl Shared memory key used to stash disk usage stats so they
dnl don't have to be checked by each sendmail process.
define(`confSHARED_MEMORY_KEY', `666666')dnl
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
dnl Need this for OpenBSD mailing lists
FEATURE(stickyhost)dnl
FEATURE(virtusertable)dnl
dnl
dnl Spam blocking features
FEATURE(access_db)dnl
dnl
dnl List the mailers we support
FEATURE(`no_default_msa')dnl
MAILER(local)dnl
MAILER(smtp)dnl
dnl
dnl We don't bother with the MSA sockets since they are not used here.
dnl Note that there is another sendmail daemon listening on port 24.
DAEMON_OPTIONS(`Family=inet, address=0.0.0.0, Name=MTA')dnl
DAEMON_OPTIONS(`Family=inet6, address=::, Name=MTA6, M=O')dnl
CLIENT_OPTIONS(`Family=inet6, Address=::')dnl
CLIENT_OPTIONS(`Family=inet, Address=0.0.0.0')dnl
dnl
dnl Finally, we have the local cf-style goo
LOCAL_CONFIG
#
#  Regular expression to reject:
#    * numeric-only localparts from aol.com and msn.com
#    * localparts starting with a digit from juno.com
#    * localparts longer than 20 characters from aol.com
#
Kcheckaddress regex -a@MATCH
   ^([0-9]+<@(aol|msn)\.com|[0-9][^<]*<@juno\.com|.{20}[^<]+<@aol\.com)\.?>

#
# SirCam worm, see below
#
KSirCamWormMarker regex -f -aSUSPECT multipart/mixed;boundary=----.+_Outlook_Express_message_boundary

#
#  Names that won't be allowed in a To: line (local-part and domains)
#
C{RejectToLocalparts}		friend you user
C{RejectToDomains}		public.com the-internet.com

LOCAL_RULESETS
#########################################################################
#
# w32.sircam.worm@mm
#
# There are serveral patterns that appear common ONLY to SirCam worm and
# not to Outlook Express, which claims to have sent the worm.  There are
# four headers that always appear together and in this order:
#
#  X-MIMEOLE: Produced By Microsoft MimeOLE V5.50.4133.2400
#  X-Mailer: Microsoft Outlook Express 5.50.4133.2400
#  Content-Type: multipart/mixed; boundary="----27AA9124_Outlook_Express_message_boundary"
#  Content-Disposition: Multipart message
#
# Empirical study of the worm message headers vs. true Outlook Express
# (5.50.4133.2400 & 5.50.4522.1200) messages with multipart/mixed attachments
# shows Outlook Express does:
#
#  a) NOT supply a Content-Disposition header for multipart/mixed messages.
#  b) NOT specify the header X-MimeOLE header name in all-caps
#  c) NOT specify boundary tag with the expression "_Outlook_Express_message_boundary"
#
# The solution below catches any one of this three issues. This is not an ideal
# solution, but a temporary measure. A correct solution would be to check for
# the presence of ALL three header attributes. Also the solution is incomplete
# since Outlook Express 5.0 and 4.0 were not compared.
#
# NOTE regex keys are first dequoted and spaces removed before matching.
# This caused me no end of grief.
#
#########################################################################

#
# Header checks
#
HTo: $>CheckTo
HMessage-Id: $>CheckMessageId
HContent-Type: $>CheckContentType
HContent-Disposition:	$>CheckContentDisposition

#
# W32/Badtrans worm detection (done in CheckContentType)
# See see http://vil.nai.com/vil/virusSummary.asp?virus_k=99069
#
D{WPat1}boundary= \"====_ABC1234567890DEF_====\"
D{WPat2}boundary= \"====_ABC0987654321DEF_====\"
D{WMsg}This message may contain the W32/Badtrans@MM virus; see http://vil.nai.com/vil/virusSummary.asp?virus_k=99069

#
# Reject mail based on regexp above
#
SLocal_check_mail
R$*				$: $>Parse0 $>3 $1
R$+				$: $(checkaddress $1 $)
R@MATCH				$#error $: "553 Header error"

#
# Reject some mail based on To: header
#
SCheckTo
R$={RejectToLocalparts}@$*	$#error $: "553 Header error"
R$*@$={RejectToDomains}		$#error $: "553 Header error"

#
# Enforce valid Message-Id to help stop spammers
#
SCheckMessageId
R< $+ @ $+ >			$@ OK
R$*				$#error $: 553 Header Error

#
# Check Content-Type header for worm/virus telltales
#
SCheckContentType
R$+				$: $(SirCamWormMarker $1 $)
R$+ ${WPat1} $*			$#error $: 553 ${WMsg}
R$+ ${WPat2} $*			$#error $: 553 ${WMsg}
RSUSPECT			$#error $: "553 Possible virus, see http://www.symantec.com/avcenter/venc/data/w32.sircam.worm@mm.html"

#
# Check Content-Disposition header for worm/virus telltales
#
SCheckContentDisposition
R$-			$@ OK
R$- ; $+		$@ OK
R$*			$#error $: "553 Illegal Content-Disposition"
