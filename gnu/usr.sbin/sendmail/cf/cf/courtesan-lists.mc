divert(-1)
#
# Sendmail 8 configuration file for lists.courtesan.com.
#

VERSIONID(`$OpenBSD: courtesan-lists.mc,v 1.1 2000/04/02 19:48:11 millert Exp $')
OSTYPE(openbsd)
dnl
dnl First, we override some default values
define(`confPRIVACY_FLAGS', `authwarnings, nobodyreturn')dnl
define(`confTRY_NULL_MX_LIST', `True')dnl
define(`confSMTP_LOGIN_MSG', `lists.courtesan.com Sendmail $v/$Z/courtesan ready at $b')dnl
define(`confMAX_HOP', `30')dnl
define(`confMAX_MIME_HEADER_LENGTH', `256/128')dnl
dnl
dnl Next, we define the features we want
FEATURE(always_add_domain)dnl
FEATURE(use_cw_file)dnl
MASQUERADE_AS(courtesan.com)dnl
FEATURE(masquerade_envelope)dnl
dnl
dnl Then, we enumerate which mailers we support
MAILER(local)
MAILER(smtp)
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
R< $+ @ $+ >			$@ OK
R$*				$#error $: 553 Header Error

LOCAL_RULESETS
#
# Reject mail based on regexp above
#
SLocal_check_mail
R$*				$: $>Parse0 $>3 $1
R$+				$: $(checkaddress $1 $)
R@MATCH				$#error $: "553 Header error"
