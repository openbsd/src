divert(-1)
#
# Copyright (c) 1997 Michael Shalayeff
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by Michael Shalayeff.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

include(`../m4/cf.m4')
VERSIONID(`$OpenBSD: lucifier.mc,v 1.2 1998/03/25 16:50:03 mickey Exp $')
OSTYPE(bsd4.4)dnl
DOMAIN(generic)dnl

MAILER(local)dnl
MAILER(smtp)dnl

dnl FEATURE(allmasquerade)dnl
dnl MASQUERADE_AS(9netave.com)dnl
dnl MASQUERADE_DOMAIN(9netave.com)dnl

dnl FEATURE(always_add_domain)dnl
FEATURE(mailertable)dnl
FEATURE(virtusertable)dnl

# hacks
HACK(use_ip)dnl
dnl HACK(use_names)dnl
HACK(use_relayto)dnl

define(`_CHECK_FROM_',1)dnl
define(`_MAPS_RBL_',1)dnl
define(`_IP_LOOKUP_',1)dnl
define(`_DNSVALID_',1)dnl
define(`_DNSRELAY_',1)dnl                                 

HACK(check_mail3, `hash -a@JUNK /etc/mail/junk')dnl
HACK(check_rcpt5, `hash -a@ALLOW /etc/mail/allow')dnl
HACK(check_relay3)dnl

# relays
define(`UUCP_RELAY', ucbvax.Berkeley.EDU)dnl
define(`BITNET_RELAY', mailhost.Berkeley.EDU)dnl
define(`CSNET_RELAY', mailhost.Berkeley.EDU)dnl

# other defines
define(`confSMTP_LOGIN_MSG', `$j ESMTP spoken here; $b')
define(`confAUTO_REBUILD', True)dnl
define(`confCHECK_ALIASES', True)dnl
define(`confTRY_NULL_MX_LIST', True)dnl
define(`confCHECKPOINT_INTERVAL', `4')dnl

define(`confMIN_FREE_BLOCKS', 1024)dnl
define(`confMAX_MESSAGE_SIZE', 1000000)dnl
define(`confSEPARATE_PROC', True)dnl
define(`confBIND_OPTS', +AAONLY)dnl
define(`confFORWARD_PATH', /var/forward/$u:$z/.forward.$w:$z/.forward)dnl
define(`confUSE_ERRORS_TO', TRUE)dnl
define(`confPRIVACY_FLAGS', `authwarnings,noexpn,novrfy,needmailhelo,restrictmailq,restrictqrun')dnl

define(`confSMTP_MAILER', `smtp8')dnl
define(`confDEF_CHAR_SET', `koi8-r')dnl
define(`confSEVEN_BIT_INPUT', False)dnl
define(`confEIGHT_BIT_HANDLING', `pass8')dnl
define(`confME_TOO', `True')dnl
define(`confNO_RCPT_ACTION', `add-to-undisclosed')dnl

define(`confMCI_CACHE_TIMEOUT', `10m')dnl
define(`confMIN_QUEUE_AGE', `30m')dnl
define(`confMAX_DAEMON_CHILDREN', `128')dnl
define(`confCONNECTION_THROTTLE_RATE', `1')dnl

define(`confTO_CONNECT', `5m')dnl
define(`confTO_COMMAND', `10m')dnl
define(`confTO_DATABLOCK', `10m')dnl
define(`confTO_DATAFINAL', `10m')dnl
define(`confTO_HOSTSTATUS', `30m')dnl
define(`confTO_IDENT', `15s')dnl
define(`confTO_QUEUEWARN', `1d')dnl
define(`confTO_QUEUEWARN_NORMAL', `1d')dnl
define(`confTO_RCPT', `10m')dnl

#LOCAL_CONFIG
#O AliasFile=/home/majordomo/etc/aliases
