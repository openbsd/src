divert(-1)
#
# Copyright (c) 2004 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Sendmail: greet_pause.m4,v 1.1 2004/02/23 21:36:04 gshapiro Exp $')
divert(-1)

ifelse(len(X`'_ARG_),`1',`ifdef(`_ACCESS_TABLE_', `',
	`errprint(`*** ERROR: FEATURE(`greet_pause') requires FEATURE(`access_db')
')')')

LOCAL_RULESETS
######################################################################
###  greet_pause: lookup pause time before 220 greeting
###
###	Parameters:
###		$1: {client_name}
###		$2: {client_addr}
######################################################################
Sgreet_pause
ifdef(`_ACCESS_TABLE_', `dnl
R$+ $| $+		$: $>D < $1 > <?> <! GreetPause> < $2 >
R   $| $+		$: $>A < $1 > <?> <! GreetPause> <>	empty client_name
R<?> <$+>		$: $>A < $1 > <?> <! GreetPause> <>	no: another lookup
ifelse(len(X`'_ARG_),`1',
`R<?> <$*>		$@',
`R<?> <$*>		$# _ARG_')
R<$* <TMPF>> <$*>	$@
R<$+> <$*>		$# $1',`dnl
R$*			$# _ARG_')
