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

VERSIONID(`$OpenBSD: lucifier.mc,v 1.1 2000/04/02 19:48:13 millert Exp $')dnl
OSTYPE(openbsd)dnl
MAILER(local)dnl
MAILER(smtp)dnl
MASQUERADE_AS(lucifier.dial-up.user.akula.net)dnl
MASQUERADE_DOMAIN(lucifier.dial-up.user.akula.net)dnl
FEATURE(allmasquerade)dnl

define(`BITNET_RELAY', relay.uu.net)dnl

define(`confAUTO_REBUILD', True)dnl
define(`confCHECK_ALIASES', True)dnl

define(`confMIN_FREE_BLOCKS', 1024)dnl
define(`confSEPARATE_PROC', True)dnl
define(`confBIND_OPTS', +AAONLY)dnl
define(`confFORWARD_PATH', /var/forward/$u:$z/.forward.$w:$z/.forward)dnl
define(`confUSE_ERRORS_TO', TRUE)dnl
define(`confPRIVACY_FLAGS', `noexpn novrfy needmailhelo')

define(`confDEF_CHAR_SET', `koi8-r')
define(`confSEVEN_BIT_INPUT', False)
define(`confEIGHT_BIT_HANDLING', `pass8')

LOCAL_CONFIG
O AliasFile=/home/majordomo/etc/aliases

