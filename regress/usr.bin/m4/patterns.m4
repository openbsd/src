dnl $OpenBSD: patterns.m4,v 1.3 2001/09/27 12:34:52 espie Exp $
patsubst(`quote s in string', `(s)', `\\\1')
patsubst(`check whether subst
over several lines
works as expected', `^', `>>>')
patsubst(`# This is a line to zap
# and a second line
keep this one', `^ *#.*
')
