divert(-1)
#
# Copyright (c) 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: relay_mail_from.m4,v 1.1.1.2 2001/01/15 20:52:31 millert Exp $')
divert(-1)

ifdef(`_ACCESS_TABLE_',
	`define(`_RELAY_DB_FROM_', 1)
	ifelse(_ARG_,`domain',`define(`_RELAY_DB_FROM_DOMAIN_', 1)')',
	`errprint(`*** ERROR: FEATURE(relay_mail_from) requires FEATURE(access_db)
')')
