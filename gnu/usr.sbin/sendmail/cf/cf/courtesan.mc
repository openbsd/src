divert(-1)
#
# Sendmail 8 configuration file for courtesan.com.
#

divert(0)dnl
VERSIONID(`$OpenBSD: courtesan.mc,v 1.10 2002/09/15 02:23:35 millert Exp $')
OSTYPE(openbsd)
dnl
dnl First, we override some default values
define(`confTRY_NULL_MX_LIST', `True')dnl
define(`confSMTP_LOGIN_MSG', `$m Sendmail $v/$Z/courtesan ready at $b')dnl
define(`confMAX_HOP', `20')dnl
define(`confMAX_MIME_HEADER_LENGTH', `256/128')dnl
dnl
dnl Deliver interactively so we don't end up with lots of messages
dnl waiting on a procmail lock.
define(`confDELIVERY_MODE', `interactive')dnl
define(`confSAFE_QUEUE', `Interactive')dnl
dnl
dnl Some broken nameservers will return SERVFAIL (a temporary failure)
dnl on T_AAAA (IPv6) lookups.
define(`confBIND_OPTS', `WorkAroundBrokenAAAA')dnl
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
MASQUERADE_AS(courtesan.com)dnl
FEATURE(masquerade_envelope)dnl
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
dnl FEATURE(dnsbl, `inputs.orbz.org', `Open spam relay - see http://www.orbz.org/sender.php')dnl
dnl FEATURE(dnsbl, `rbl.maps.vix.com', `Rejected - see http://www.mail-abuse.org/rbl/')dnl
dnl FEATURE(dnsbl, `dul.maps.vix.com', `Dialup - see http://www.mail-abuse.org/dul/')dnl
dnl FEATURE(dnsbl, `relays.mail-abuse.org', `Open spam relay - see http://www.mail-abuse.org/rss/')dnl
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
