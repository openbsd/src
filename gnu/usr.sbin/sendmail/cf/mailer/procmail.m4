PUSHDIVERT(-1)
#
# Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
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

ifdef(`PROCMAIL_MAILER_PATH',,
	`ifdef(`PROCMAIL_PATH',
		`define(`PROCMAIL_MAILER_PATH', PROCMAIL_PATH)',
		`define(`PROCMAIL_MAILER_PATH', /usr/local/bin/procmail)')')
_DEFIFNOT(`PROCMAIL_MAILER_FLAGS', `SPhnu9')
ifdef(`PROCMAIL_MAILER_ARGS',,
	`define(`PROCMAIL_MAILER_ARGS', `procmail -Y -m $h $f $u')')

POPDIVERT

######################*****##############
###   PROCMAIL Mailer specification   ###
##################*****##################

VERSIONID(`$Sendmail: procmail.m4,v 8.21 2000/09/02 17:46:43 ca Exp $')

Mprocmail,	P=PROCMAIL_MAILER_PATH, F=_MODMF_(CONCAT(`DFM', PROCMAIL_MAILER_FLAGS), `PROCMAIL'), S=EnvFromSMTP/HdrFromSMTP, R=EnvToSMTP/HdrFromSMTP,
		ifdef(`PROCMAIL_MAILER_MAX', `M=PROCMAIL_MAILER_MAX, ')T=DNS/RFC822/X-Unix,
		A=PROCMAIL_MAILER_ARGS
