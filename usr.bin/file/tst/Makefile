#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:45:13 deraadt Exp $

# Make up some fake test files that are easily produced.
# By no means an exhaustive test!

all:	ar cmd emp i tarf x
ar:
	echo '<ar> fake fake fake' >$@
	echo 070707 fake fake fake >$@.asc
	echo '!<arch>.__.SYMDEF fake fake fake' >$@.ranlib
	echo -h- >$@.swt
cmd:
	echo '#! /bin/sh' >$@
	echo '#!/bin/sh' >c.sh2
	echo '#! /bin/csh' >c.csh1
	echo '#!/bin/csh' >c.csh2
	echo '#! /bin/awk' >c.awk1
	echo '#!/bin/awk' >c.awk2
	echo '#! /' >c.misc1
	echo '#!/' >c.misc2
	echo ': ' >c.broken
emp:
	touch $@
i:
	echo '@document(language impress)fake fake' >$@
	echo '@document(language diablo)fake fake' >$@.d
tarf:
	rm -f $@	# so not include self
	tar cvf $@ *
x:
	echo 'Interpress/Xerox fake fake fake' >$@

clean:
	rm -f [a-z]*
