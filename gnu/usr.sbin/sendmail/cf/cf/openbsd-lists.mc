divert(-1)
#
# Sendmail configuration file for lists.openbsd.org
#
# This config handles incoming mail for openbsd.{org,com,net}
# Mailing list fanout is handled by a separate exploder running on
# port 24 that is fed by mj2 (see openbsd-bulk.mc).
#

divert(0)dnl
VERSIONID(`$OpenBSD: openbsd-lists.mc,v 1.16 2005/01/06 17:21:03 millert Exp $')
OSTYPE(openbsd)dnl
dnl
dnl Advertise ourselves as ``openbsd.org''
define(`confSMTP_LOGIN_MSG', `openbsd.org Sendmail $v/$Z/millert ready willing and able at $b')dnl
dnl
dnl Override some default values
define(`confPRIVACY_FLAGS', `authwarnings,needmailhelo,noexpn,novrfy,noetrn,noverb,nobodyreturn')dnl
define(`confTRY_NULL_MX_LIST', `True')dnl
define(`confMAX_HOP', `30')dnl
define(`confQUEUE_LA', `25')dnl
define(`confREFUSE_LA', `50')dnl
dnl
dnl Some broken nameservers will return SERVFAIL (a temporary failure)
dnl on T_AAAA (IPv6) lookups.
define(`confBIND_OPTS', `WorkAroundBrokenAAAA')dnl
dnl
dnl Keep host status on disk between sendmail runs in the .hoststat dir
define(`confHOST_STATUS_DIRECTORY', `/var/spool/mqueue/.hoststat')dnl
define(`confTO_HOSTSTATUS', `30m')dnl
dnl
dnl Just queue incoming messages, we have a queue runner for actual delivery
define(`confDELIVERY_MODE', `q')dnl
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
dnl Simple queue group settings:
dnl     run at most 10 concurrent processes for initial submission
dnl     max of 3 queue runners.
define(`confMAX_QUEUE_CHILDREN', `10')dnl
define(`confMAX_RUNNERS_PER_QUEUE', `3')dnl
define(`confFAST_SPLIT', `10')dnl
QUEUE_GROUP(`mqueue', `P=/var/spool/mqueue, R=3, F=f')dnl
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
dnl milter-regex
INPUT_MAIL_FILTER(`milter-regex', `S=local:/var/run/milter-regex/sock, T=S:30s;R:2m')dnl
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
HSubject: $>Check_Subject
HContent-Type: $>CheckContentType
HContent-Disposition:	$>CheckContentDisposition

#
# Beagle.k@mm worm detection (done in Check_Subject)
# See http://securityresponse.symantec.com/avcenter/venc/data/w32.beagle.k@mm.html?Open
#
D{BKPat1}E-mail account disabling warning.
D{BKPat2}E-mail account security warning.
D{BKPat3}Email account utilization warning.
D{BKPat4}Important notify about your e-mail account.
D{BKPat5}Notify about using the e-mail account.
D{BKPat6}Notify about your e-mail account utilization.
D{BKPat7}Warning about your e-mail account.

#
# Sobig.F worm detection (done in Check_Subject)
# See http://securityresponse.symantec.com/avcenter/venc/data/w32.sobig.f@mm.html
#
D{SBJPat1}Re: Details
D{SBJPat2}Re: Approved
D{SBJPat3}Re: Re: My details
D{SBJPat4}Re: Thank You!
D{SBJPat5}Re: That Movie
D{SBJPat6}Re: Wicked screensaver
D{SBJPat7}Re: Your application
D{SBJPat8}Thank You!
D{SBJPat9}Your details

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
# Check Subject line for worm/virus telltales
#
SCheck_Subject
R${SBJPat1}			$#discard $: discard
R${SBJPat2}			$#discard $: discard
R${SBJPat3}			$#discard $: discard
R${SBJPat4}			$#discard $: discard
R${SBJPat5}			$#discard $: discard
R${SBJPat6}			$#discard $: discard
R${SBJPat7}			$#discard $: discard
R${SBJPat8}			$#discard $: discard
R${SBJPat9}			$#discard $: discard
R${BKPat1}			$#discard $: discard
R${BKPat2}			$#discard $: discard
R${BKPat3}			$#discard $: discard
R${BKPat4}			$#discard $: discard
R${BKPat5}			$#discard $: discard
R${BKPat6}			$#discard $: discard
R${BKPat7}			$#discard $: discard

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
