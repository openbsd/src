divert(-1)
#
# Sendmail 8 configuration file for courtesan.com.
# This machine gets a lot of mail so we use a queue-only config and:
#	sendmail_flags="-L sm-mta -bd -qp1s"
# The queue group limits and confMIN_QUEUE_AGE keep things sane
# and prevent a sendmail DoS when thousands of messages (bounces)
# come in at once.
#

divert(0)dnl
VERSIONID(`$OpenBSD: courtesan.mc,v 1.15 2006/03/22 18:43:53 millert Exp $')
OSTYPE(openbsd)
dnl
dnl First, we override some default values
define(`confTRY_NULL_MX_LIST', `True')dnl
define(`confSMTP_LOGIN_MSG', `$j spamd IP-based SPAM blocker; $d')dnl
define(`confMAX_HOP', `20')dnl
define(`confMAX_MIME_HEADER_LENGTH', `256/128')dnl
dnl
dnl Just queue incoming messages, we have a queue runner for actual delivery
define(`confDELIVERY_MODE', `q')dnl
dnl
dnl Add X-Authentication-Warning: headers and disable EXPN and VRFY
define(`confPRIVACY_FLAGS', `authwarnings,needmailhelo,noexpn,novrfy,noetrn,noverb,nobodyreturn')dnl
dnl
dnl Some broken nameservers will return SERVFAIL (a temporary failure)
dnl on T_AAAA (IPv6) lookups.
define(`confBIND_OPTS', `WorkAroundBrokenAAAA')dnl
dnl
dnl Wait at least 15 minutes before trying to redeliver a message.
define(`confMIN_QUEUE_AGE', `15m')dnl
dnl
dnl TLS certificates for encrypted mail
define(`CERT_DIR', `MAIL_SETTINGS_DIR`'certs')dnl
define(`confCACERT_PATH', `CERT_DIR')dnl
define(`confCACERT', `CERT_DIR/mycert.pem')dnl
define(`confSERVER_CERT', `CERT_DIR/mycert.pem')dnl
define(`confSERVER_KEY', `CERT_DIR/mykey.pem')dnl
define(`confCLIENT_CERT', `CERT_DIR/mycert.pem')dnl
define(`confCLIENT_KEY', `CERT_DIR/mykey.pem')dnl
dnl
dnl Next, we define the features we want
FEATURE(nouucp, `reject')dnl
FEATURE(always_add_domain)dnl
FEATURE(use_cw_file)dnl
FEATURE(redirect)dnl
dnl
dnl Don't canonify everything, just bare hosts and things from courtesan.com
FEATURE(`nocanonify', `canonify_hosts')dnl
CANONIFY_DOMAIN(`courtesan.com')dnl
dnl
dnl All mail gets stamped as being from courtesan.com
MASQUERADE_AS(courtesan.com)dnl
FEATURE(masquerade_envelope)dnl
dnl
dnl Rewrite outgoing email addresses
FEATURE(genericstable, `hash -o /etc/mail/mailnames')dnl
FEATURE(generics_entire_domain)dnl
GENERICS_DOMAIN(`courtesan.com')dnl
dnl
dnl Virtual domains
FEATURE(stickyhost)dnl
FEATURE(virtusertable)dnl
dnl
dnl Spam blocking features
FEATURE(access_db)dnl
FEATURE(blacklist_recipients)dnl
FEATURE(dnsbl, `sbl.spamhaus.org', `Spam blocked - see http://www.spamhaus.org/')dnl
FEATURE(`dnsbl', `list.dsbl.org', `"550 Email rejected due to sending server misconfiguration - see http://dsbl.org/faq-listed"')dnl
FEATURE(`dnsbl', `relays.ordb.org', `"550 Email rejected due to sending server misconfiguration - see http://www.ordb.org/faq/\#why_rejected"')dnl
dnl FEATURE(`dnsbl', `bl.spamcop.net', `"Spam blocked - see: http://spamcop.net/bl.shtml?"$&{client_addr}')dnl
dnl FEATURE(dnsbl, `ipwhois.rfc-ignorant.org',`"550 Mail from " $&{client_addr} " refused. Rejected for bad WHOIS info on IP of your SMTP server - see http://www.rfc-ignorant.org/"')dnl
dnl
dnl Simple queue group settings:
dnl	run at most 10 concurrent processes for initial submission
dnl	max of 5 queue runners, split into at most 15 recipients per envelope
define(`confMAX_QUEUE_CHILDREN', `20')dnl
define(`confMAX_RUNNERS_PER_QUEUE', `5')dnl
define(`confFAST_SPLIT', `10')dnl
QUEUE_GROUP(`mqueue', `P=/var/spool/mqueue, R=5, r=15, F=f')dnl
dnl
dnl Then, we enumerate which mailers we support
FEATURE(`no_default_msa')dnl
MAILER(local)dnl
MAILER(smtp)dnl
dnl
dnl We want to support IPv6
DAEMON_OPTIONS(`Family=inet, address=0.0.0.0, Name=MTA')dnl
DAEMON_OPTIONS(`Family=inet6, address=::, Name=MTA6, M=O')dnl
DAEMON_OPTIONS(`Family=inet, address=0.0.0.0, Port=587, Name=MSA, M=E')dnl
DAEMON_OPTIONS(`Family=inet6, address=::, Port=587, Name=MSA6, M=O, M=E')dnl
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

# Regex to catch sobig worm
#
KSobigWormMarker regex -f -aSOBIG multipart/mixed;boundary=_NextPart_000_........$

# Body regex to catch virii
# See http://web.abnormal.com/~thogard/sendmail/
#
Krv1 regex -aVirus-Detect1 (I send you this file in order to have your advice)
Krv2 regex -aVirus-Detect2 ^TVqQAAMAAAAEAAA
Krv3 regex -aVirus-Detect3 ^TVpQAAIAAAAEAA
Krv4 regex -aVirus-Detect4 ^3sSUDhYWiuS/z9goBJ
Krv5 regex -aVirus-Detect5 ^cnVuIGluI
Krv6 regex -aVirus-Detect6 attached.file.for.details
Krv7 regex -aVirus-Detect7 ^R0lGODl
Krv8 regex -aVirus-Detect8 7-bit.ASCII.encoding.and
# Collect all regex into a single sequence to be rejected
Ksv1 sequence rv1 rv2 rv3 rv4 rv5 rv6 rv7 rv8
Kbodyregex sequence sv1

#
#  Names that won't be allowed in a To: line (local-part and domains)
#
C{RejectToLocalparts}		friend you user
C{RejectToDomains}		public.com the-internet.com

LOCAL_RULESETS
#
# Header checks
#
HTo: $>CheckTo
HMessage-Id: $>CheckMessageId
HSubject: $>Check_Subject
HX-Spanska: $>Spanska
HContent-Type: $>Check_Content

#
# Melissa worm detection (done in Check_Subject)
# See http://www.cert.org/advisories/CA-99-04-Melissa-Macro-Virus.html
#
D{MPat}Important Message From
D{MMsg}This message may contain the Melissa virus; see http://www.cert.org/advisories/CA-99-04-Melissa-Macro-Virus.html

#
# ILOVEYOU worm detection (done in Check_Subject)
# See http://www.datafellows.com/v-descs/love.htm
#
D{ILPat}ILOVEYOU
D{ILMsg}This message may contain the ILOVEYOU virus; see http://www.datafellows.com/v-descs/love.htm

#
# Life stages worm detection (done in Check_Subject)
# See http://www.f-secure.com/v-descs/stages.htm
#
D{LSPat}Fw: Life stages
D{LSMsg}This message may contain the Life stages virus; see http://www.f-secure.com/v-descs/stages.htm

#
# W32/Badtrans worm detection (done in Check_Content)
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

# Catch Sobig.F
SCheckContentType
R$+				$: $(SobigWormMarker $1 $)
RSOBIG				$#discard $: discard

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
# Happy99 worm detection
#
SSpanska
R$*				$#error $: "553 Your system is probably infected by the Happy99 worm; see http://www.symantec.com/avcenter/venc/data/happy99.worm.html"

#
# Check Subject line for worm/virus telltales
#
SCheck_Subject
R${MPat} $*			$#error $: 553 ${MMsg}
RRe: ${MPat} $*			$#error $: 553 ${MMsg}
R${ILPat}			$#error $: 553 ${ILMsg}
RRe: ${ILPat}			$#error $: 553 ${ILMsg}
R${LSPat}			$#error $: 553 ${LSMsg}
RRe: ${LSPat}			$#error $: 553 ${LSMsg}

#
# Check Content-Type header for worm/virus telltales
#
SCheck_Content
R$+ ${WPat1} $*			$#error $: 553 ${WMsg}
R$+ ${WPat2} $*			$#error $: 553 ${WMsg}
