divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#	MkLinux support contributed by Paul DuBois <dubois@primate.wisc.edu>
#

divert(0)
VERSIONID(`$Sendmail: mklinux.m4,v 8.12 1999/04/24 05:37:42 gshapiro Exp $')
ifdef(`STATUS_FILE',,
	`define(`STATUS_FILE', `/var/log/sendmail.st')')
ifdef(`PROCMAIL_MAILER_PATH',,
	define(`PROCMAIL_MAILER_PATH', `/usr/bin/procmail'))
FEATURE(local_procmail)
