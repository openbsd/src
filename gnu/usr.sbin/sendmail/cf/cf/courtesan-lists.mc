divert(-1)
#
# Sendmail 8 configuration file for lists.courtesan.com.
#
# This machine is the courtesan.com mailing list server.
#

divert(0)dnl
VERSIONID(`$OpenBSD: courtesan-lists.mc,v 1.4 2001/09/11 19:02:48 millert Exp $')
OSTYPE(openbsd)dnl
dnl
dnl Advertise ourselves as ``lists.courtesan.com''
define(`confSMTP_LOGIN_MSG', `lists.courtesan.com Sendmail $v/$Z/courtesan ready at $b')dnl
dnl
dnl Define relays, since not everyone uses internet addresses, even now
define(`UUCP_RELAY', `rutgers.edu')dnl
define(`BITNET_RELAY', `interbit.cren.net')dnl
define(`DECNET_RELAY', `vaxf.colorado.edu')dnl
dnl
dnl Override some default values
define(`confPRIVACY_FLAGS', `authwarnings, nobodyreturn')dnl
define(`confTRY_NULL_MX_LIST', `True')dnl
define(`confMAX_HOP', `30')dnl
define(`confMAX_MIME_HEADER_LENGTH', `256/128')dnl
dnl
dnl Always use fully qualified domains
FEATURE(always_add_domain)
dnl
dnl Some broken nameservers will return SERVFAIL (a temporary failure)
dnl on T_AAAA (IPv6) lookups.
define(`confBIND_OPTS', `WorkAroundBrokenAAAA')dnl
dnl
dnl Need to add domo and mailman as "trusted users" to rewrite From lines
define(`confTRUSTED_USERS', `domo mailman')dnl
dnl
dnl Wait a day before sending mail about deferred messages
define(`confTO_QUEUEWARN', `1d')dnl
dnl
dnl Wait 4 days before giving up and bouncing the message
define(`confTO_QUEUERETURN', `4d')dnl
dnl
dnl Make mail appear to be from openbsd.org
MASQUERADE_AS(courtesan.com)
FEATURE(masquerade_envelope)
dnl
dnl Spam blocking features
FEATURE(access_db)dnl
FEATURE(blacklist_recipients)dnl
dnl FEATURE(dnsbl, `rbl.maps.vix.com', `Rejected - see http://www.mail-abuse.org/rbl/')dnl
dnl FEATURE(dnsbl, `dul.maps.vix.com', `Dialup - see http://www.mail-abuse.org/dul/')dnl
dnl FEATURE(dnsbl, `relays.mail-abuse.org', `Open spam relay - see http://www.mail-abuse.org/rss/')dnl
dnl
dnl List the mailers we support
MAILER(local)dnl
MAILER(smtp)dnl
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
