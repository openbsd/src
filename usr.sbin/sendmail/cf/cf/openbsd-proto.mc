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
#  This is the prototype file for a configuration that supports nothing
#  but basic SMTP connections via TCP.
#

divert(0)dnl
include(`../m4/cf.m4')
VERSIONID(`@(#)openbsd-proto.mc $Revision: 1.3 $')
OSTYPE(openbsd)
FEATURE(nouucp)
MAILER(local)
MAILER(smtp)
dnl
dnl Enforce valid Message-Id to help stop spammers
dnl
LOCAL_RULESETS
HMessage-Id: $>CheckMessageId

SCheckMessageId
R< $+ @ $+ >		$@ OK
R$*			$#error $: 553 Header Error
