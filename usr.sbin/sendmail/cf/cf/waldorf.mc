divert(-1)
#	$OpenBSD: waldorf.mc,v 1.6 1998/08/15 18:17:19 millert Exp $
#
# Copyright (c) 1996 Niklas Hallqvist
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
#	This product includes software developed by Niklas Hallqvist.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
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
VERSIONID(`$OpenBSD: waldorf.mc,v 1.6 1998/08/15 18:17:19 millert Exp $')
OSTYPE(openbsd)dnl

MASQUERADE_AS(appli.se)
MASQUERADE_DOMAIN(appli.se)

FEATURE(local_procmail)dnl

MAILER(local)dnl
MAILER(smtp)dnl

FEATURE(limited_masquerade)dnl
FEATURE(always_add_domain)dnl
FEATURE(virtusertable)dnl
FEATURE(use_cw_file)dnl

define(`confAUTO_REBUILD', True)dnl

LOCAL_RULE_0
# We take care of all mail directed to either appli.se or *.appli.se
R$+<@$*$m.>	$#local $:$1
