divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

#
#  This configuration only runs sendmail on the localhost interface.
#  This allows mail on the local host to work without accepting
#  connections from the net at large.
#

divert(0)dnl
VERSIONID(`@(#)openbsd-localhost.mc $Revision: 1.1 $')
OSTYPE(openbsd)
FEATURE(nouucp, `reject')
MAILER(local)
MAILER(smtp)
DAEMON_OPTIONS(`Family=inet, address=127.0.0.1, Name=MTA')dnl
DAEMON_OPTIONS(`Family=inet6, address=::1, Name=MTA6, M=O')dnl
dnl
dnl Some broken nameservers will return SERVFAIL (a temporary failure) 
dnl on T_AAAA (IPv6) lookups.
define(`confBIND_OPTS', `WorkAroundBrokenAAAA')dnl
