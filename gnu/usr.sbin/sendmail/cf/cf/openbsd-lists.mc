divert(-1)
#
# Sendmail 8 configuration file for lists.openbsd.org
#
# This machine handles all mail for openbsd.{org,com,net}
#

VERSIONID(`$OpenBSD: openbsd-lists.mc,v 1.1 2000/04/02 19:48:13 millert Exp $')
OSTYPE(openbsd)dnl
dnl
dnl Advertise ourselves as ``openbsd.org''
define(`confSMTP_LOGIN_MSG', `openbsd.org Sendmail $v/$Z/millert ready willing and able at $b')dnl
dnl
dnl Define relays, since not everyone uses internet addresses, even now
define(`UUCP_RELAY', `rutgers.edu')
define(`BITNET_RELAY', `interbit.cren.net')
define(`DECNET_RELAY', `vaxf.colorado.edu')
dnl
dnl Override some default values
define(`confPRIVACY_FLAGS', `authwarnings, nobodyreturn')dnl
define(`confTRY_NULL_MX_LIST', `True')
define(`confMAX_HOP', `30')dnl
dnl
dnl Always use fully qualified domains
FEATURE(always_add_domain)
dnl
dnl Treat mail to openbsd.{org,net,com} as local
Cw openbsd.org
Cw openbsd.net
Cw openbsd.com
Cw anonopenbsd.cs.colorado.edu
dnl
dnl Need to add domo and mailman as "trusted users" to rewrite From lines
define(`confTRUSTED_USERS', `domo mailman')
dnl
dnl Wait a day before sending mail about deferred messages
define(`confTO_QUEUEWARN', `1d')
dnl
dnl Wait 4 days before giving up and bouncing the message
define(`confTO_QUEUERETURN', `4d')
dnl
dnl Make mail appear to be from openbsd.org
MASQUERADE_AS(openbsd.org)
FEATURE(masquerade_envelope)
dnl
dnl Need this for OpenBSD mailing lists
FEATURE(stickyhost)dnl
FEATURE(virtusertable)dnl
dnl
dnl We use the access DB for spam prevention
FEATURE(access_db)dnl
FEATURE(blacklist_recipients)dnl
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
# Reject some mail based on To: header
#
HTo: $>CheckTo
SCheckTo
R$={RejectToLocalparts}@$*	$#error $: "553 Header error"
R$*@$={RejectToDomains}		$#error $: "553 Header error"

#
# Enforce valid Message-Id to help stop spammers
#
HMessage-Id: $>CheckMessageId
SCheckMessageId
R< $+ @ $+ >		$@ OK
R$*			$#error $: 553 Header Error
