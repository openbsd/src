divert(-1)
#
# Copyright (c) 2001-2003 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

#
#  This is the prototype file for a set-group-ID sm-msp sendmail that
#  acts as a initial mail submission program.
#

divert(0)dnl
VERSIONID(`$OpenBSD: openbsd-submit.mc,v 1.2 2013/04/24 07:01:59 ajacoutot Exp $')
define(`confCF_VERSION', `Submit')dnl
OSTYPE(openbsd)dnl
define(`_USE_DECNET_SYNTAX_', `1')dnl support DECnet
define(`confTIME_ZONE', `USE_TZ')dnl
define(`confBIND_OPTS', `WorkAroundBrokenAAAA')dnl
define(`confDONT_INIT_GROUPS', `True')dnl
define(`confCT_FILE', `-o MAIL_SETTINGS_DIR`'trusted-users')dnl
define(`confTO_IDENT', `0')dnl
FEATURE(`use_ct_file')dnl
FEATURE(`accept_unresolvable_domains')dnl
dnl
dnl If you use IPv6 only, change [127.0.0.1] to [IPv6:::1]
FEATURE(`msp', `[127.0.0.1]')dnl
