#
# The first 39 of these tests are from the old Bugs script.
#

name: regression-1
description:
	Lex array code had problems with this.
stdin:
	echo foo[
	n=bar
	echo "hi[ $n ]=1"
expected-stdout:
	foo[
	hi[ bar ]=1
---


name: regression-2
description:
	When PATH is set before running a command, the new path is
	not used in doing the path search
		$ echo echo hi > /tmp/q ; chmod a+rx /tmp/q
		$ PATH=/tmp q
		q: not found
		$
	in comexec() the two lines
		while (*vp != NULL)
			(void) typeset(*vp++, xxx, 0);
	need to be moved out of the switch to before findcom() is
	called - I don't know what this will break.
stdin:
	: ${PWD:-`pwd 2> /dev/null`}
	: ${PWD:?"PWD not set - can't do test"}
	mkdir Y
	cat > Y/xxxscript << EOF
	#!/bin/sh
	echo hi
	exit 0
	EOF
	chmod a+rx Y/xxxscript
	PATH=$PWD/Y xxxscript
	exit $?
expected-stdout:
	hi
---


#
#   3. Sun OS 4.0.x (This seems to be a problem with sun's PENDIN not being done
#      properly)
#   	sleep 5^J ls^J ls^J ls		[only first ls runs]
#   	vi ... ZZ (while waiting type)	[some of the input gets eaten]
#      [not present in SunOS 4.1.x]
#echo "  [No automatic test for bug 3 - interactive]"


#
#   4. (fixed)
#
#echo "  [Don't know what bug 4 was]"


#
#   5. Everywhere
#   	File name completion (^X,*) does not mesh well with cd and
#   	symbolic links.  cd does path simplification wrt $PWD before
#   	doing the actual chdir(), while file name completion does
#   	not do the simplification. E.g., you are in directory A
#   	which has a symbolic link to directory B, you create a file
#   	called foobar and you then cd to the symlink to B, and type
#   		$ echo ../foo^X
#   	and the shell beeps at you.  Would be more consistent to
#   	do the completion after simplifing the `$PWD/..'.
#echo "  [No automatic test for bug 5 - interactive]"


name: regression-6
description:
	Parsing of $(..) expressions is non-optimal.  It is
	impossible to have any parentheses inside the expression.
	I.e.,
		$ ksh -c 'echo $(echo \( )'
		no closing quote
		$ ksh -c 'echo $(echo "(" )'
		no closing quote
		$
	The solution is to hack the parsing clode in lex.c, the
	question is how to hack it: should any parentheses be
	escaped by a backslash, or should recursive parsing be done
	(so quotes could also be used to hide hem).  The former is
	easier, the later better...
stdin:
	echo $(echo \()
expected-stdout:
	(
---


#
#   7. (fixed)
#
#echo "  [Don't know what bug 7 was]"


#
#   8. Everywhere - NOT A BUG - this is what at&t ksh88 does
#   	Strange typset -x behaviour in functions.  The following function
#   	does not set the environment variable BLAH outside the function:
#   		function blah
#   		{
#   			typeset -x BLAH=foobar
#   		}
#   	This function does work:
#   		function blah
#   		{ BLAH=foobar; export BLAH
#   		}
#echo '  [Bug 8 was bogus]'


name: regression-9
description:
	Continue in a for loop does not work right:
		for i in a b c ; do
			if [ $i = b ] ; then
				continue
			fi
			echo $i
		done
	Prints a forever...
stdin:
	first=yes
	for i in a b c ; do
		if [ $i = b ] ; then
			if [ $first = no ] ; then
				echo 'continue in for loop broken'
				break	# hope break isn't broken too :-)
			fi
			first=no
			continue
		fi
	done
	echo bye
expected-stdout:
	bye
---


name: regression-10
description:
	The following:
		set -- `false`
		echo $?
	shoud not print 0. (according to /bin/sh, at&t ksh88, and the
	getopt(1) man page - not according to POSIX)
stdin:
	set -- `false`
	echo $?
expected-stdout:
	1
---


name: regression-11
description:
	The following:
		x=/foo/bar/blah
		echo ${x##*/}
	should echo blah but on some machines echos /foo/bar/blah.
stdin:
	x=/foo/bar/blah
	echo ${x##*/}
expected-stdout:
	blah
---


name: regression-12
description:
	Both of the following echos produce the same output under sh/ksh.att:
		#!/bin/sh
		x="foo	bar"
		echo "`echo \"$x\"`"
		echo "`echo "$x"`"
	pdksh produces different output for the former (foo instead of foo\tbar)
stdin:
	x="foo	bar"
	echo "`echo \"$x\"`"
	echo "`echo "$x"`"
expected-stdout:
	foo	bar
	foo	bar
---


name: regression-13
description:
	The following command hangs forever:
		$ (: ; cat /etc/termcap) | sleep 2
	This is because the shell forks a shell to run the (..) command
	and this shell has the pipe open.  When the sleep dies, the cat
	doesn't get a SIGPIPE 'cause a process (ie, the second shell)
	still has the pipe open.
	
	NOTE: this test provokes a bizarre bug in ksh93 (shell starts reading
	      commands from /etc/termcap..)
time-limit: 4
stdin:
	echo A line of text that will be duplicated quite a number of times.> t1
	cat t1 t1 t1 t1  t1 t1 t1 t1  t1 t1 t1 t1  t1 t1 t1 t1  > t2
	cat t2 t2 t2 t2  t2 t2 t2 t2  t2 t2 t2 t2  t2 t2 t2 t2  > t1
	cat t1 t1 t1 t1 > t2
	(: ; cat t2) | sleep 1
---


name: regression-14
description:
	The command
		$ (foobar) 2> /dev/null
	generates no output under /bin/sh, but pdksh produces the error
		foobar: not found
	Also, the command
		$ foobar 2> /dev/null
	generates an error under /bin/sh and pdksh, but at&t ksh88 produces
	no error (redirected to /dev/null).
stdin:
	(you/should/not/see/this/error/1) 2> /dev/null
	you/should/not/see/this/error/2 2> /dev/null
	true
---


name: regression-15
description:
	The command
		$ whence foobar
	generates a blank line under pdksh and sets the exit status to 0.
	at&t ksh88 generates no output and sets the exit status to 1.  Also,
	the command
		$ whence foobar cat
	generates no output under at&t ksh88 (pdksh generates a blank line
	and /bin/cat).
stdin:
	whence does/not/exist > /dev/null
	echo 1: $?
	echo 2: $(whence does/not/exist | wc -l)
	echo 3: $(whence does/not/exist cat | wc -l)
expected-stdout:
	1: 1
	2: 0
	3: 0
---


name: regression-16
description:
	${var%%expr} seems to be broken in many places.  On the mips
	the commands
		$ read line < /etc/passwd
		$ echo $line
		root:0:1:...
		$ echo ${line%%:*}
		root
		$ echo $line
		root
		$
	change the value of line.  On sun4s & pas, the echo ${line%%:*} doesn't
	work.  Haven't checked elsewhere...
script:
	read x
	y=$x
	echo ${x%%:*}
	echo $x
stdin:
	root:asdjhasdasjhs:0:1:Root:/:/bin/sh
expected-stdout:
	root
	root:asdjhasdasjhs:0:1:Root:/:/bin/sh
---


name: regression-17
description:
	The command
		. /foo/bar
	should set the exit status to non-zero (sh and at&t ksh88 do).
	XXX doting a non existant file is a fatal error for a script
stdin:
	. does/not/exist
expected-exit: e != 0
expected-stderr-pattern: /.?/
---


#
#   18. Everywhere
#   	In vi mode ^X (and *) can dump core:
#   		$ ab[cd^XMemory fault (core dumped)
#echo "  [No automatic test for bug 18 - interactive]"


name: regression-19
description:
	Both of the following echos should produce the same thing, but don't:
		$ x=foo/bar
		$ echo ${x%/*}
		foo
		$ echo "${x%/*}"
		foo/bar
stdin:
	x=foo/bar
	echo "${x%/*}"
expected-stdout:
	foo
---


#
#   20. (same as 18)
#


name: regression-21
description:
	backslash does not work as expected in case labels:
	$ x='-x'
	$ case $x in
	-\?) echo hi
	esac
	hi
	$ x='-?'
	$ case $x in
	-\\?) echo hi
	esac
	hi
	$
stdin:
	case -x in
	-\?)	echo fail
	esac
---


name: regression-22
description:
	Quoting backquotes inside backquotes doesn't work:
	$ echo `echo hi \`echo there\` folks`
	asks for more info.  sh and at&t ksh88 both echo
	hi there folks
stdin:
	echo `echo hi \`echo there\` folks`
expected-stdout:
	hi there folks
---


name: regression-23
description:
	)) is not treated `correctly':
	    $ (echo hi ; (echo there ; echo folks))
	    missing ((
	    $
	instead of (as sh and ksh.att)
	    $ (echo hi ; (echo there ; echo folks))
	    hi
	    there
	    folks
	    $
stdin:
	( : ; ( : ; echo hi))
expected-stdout:
	hi
---


#
#   24. strangeness with file name completion involving symlinks to nowhere
#   	$ mkdir foo foo/bar
#   	$ ln -s /stuff/junk foo/bar/xx
#   	$ echo foo/*/xx 
#	(beep)
#   	$
#echo "  [No automatic test for bug 24 - interactive]"


name: regression-25
description:
	Check reading stdin in a while loop.  The read should only read
	a single line, not a whole stdio buffer; the cat should get
	the rest.
stdin:
	(echo a; echo b) | while read x ; do
	    echo $x
	    cat > /dev/null
	done
expected-stdout:
	a
---


name: regression-26
description:
	Check reading stdin in a while loop.  The read should read both
	lines, not just the first.
script:
	a=
	while [ "$a" != xxx ] ; do
	    last=$x
	    read x
	    /bin/cat /dev/null | sed 's/x/y/'
	    a=x$a
	done
	echo $last
stdin:
	a
	b
expected-stdout:
	b
---


name: regression-27
description:
	The command
		. /does/not/exist
	should cause a script to exit.
stdin:
	. does/not/exist
	echo hi
expected-exit: e != 0
expected-stderr-pattern: /does\/not\/exist/
---


name: regression-28
description:
	variable assignements not detected well
stdin:
	a.x=1 echo hi
expected-exit: e != 0
expected-stderr-pattern: /a\.x=1/
---


name: regression-29
description:
	alias expansion different from at&t ksh88
stdin:
	alias a='for ' b='i in'
	a b hi ; do echo $i ; done
expected-stdout:
	hi
---


name: regression-30
description:
	strange characters allowed inside ${...}
stdin:
	echo ${a{b}}
expected-exit: e != 0
expected-stderr-pattern: /.?/
---


name: regression-31
description:
	Does read handle partial lines correctly
script:
	a= ret=
	while [ "$a" != xxx ] ; do
	    read x y z
	    ret=$?
	    a=x$a
	done
	echo "[$x]"
	echo $ret
stdin: !
	a A aA
	b B Bb
	c
expected-stdout:
	[c]
	1
---


name: regression-32
description:
	Does read set variables to null at eof?
script:
	a=
	while [ "$a" != xxx ] ; do
	    read x y z
	    a=x$a
	done
	echo 1: ${x-x not set} ${y-y not set} ${z-z not set} 
	echo 2: ${x:+x not null} ${y:+y not null} ${z:+z not null}
stdin:
	a A Aa
	b B Bb
expected-stdout:
	1:
	2:
---


name: regression-33
description:
	Does umask print a leading 0 when umask is 3 digits?
stdin:
	umask 222
	umask
expected-stdout:
	0222
---


#
#
#	Does umask print a umask of 0 sanely?
#	There is lots of variety here (0, 00, 000, and 0000 have all been
#	seen in various shells...)
#
#echo '  [Bug 34 was bogus]'


name: regression-35
description:
	Tempory files used for here-docs in functions get trashed after
	the function is parsed (before it is executed)
stdin:
	f1() {
		cat <<- EOF
			F1
		EOF
		f2() {
			cat <<- EOF
				F2
			EOF
		}
	}
	f1
	f2
	unset -f f1
	f2
expected-stdout:
	F1
	F2
	F2
---


name: regression-36
description:
	Command substitution breaks reading in while loop
	(test from <sjg@void.zen.oz.au>)
stdin:
	(echo abcdef; echo; echo 123) | 
	    while read line
	    do
	      # the following line breaks it
	      c=`echo $line | wc -c`
	      echo $c
	    done
expected-stdout:
	7
	1
	4
---


name: regression-37
description:
	Machines with broken times() (reported by <sjg@void.zen.oz.au>)
	time does not report correct real time
stdin:
	time sleep 1
expected-stderr-pattern: !/^\s*0\.0[\s\d]+real|^\s*real[\s]+0+\.0/
---


name: regression-38
description:
	set -e doesn't ignore exit codes for if/while/until/&&/||/!.
arguments: !-e!
stdin:
	if false; then echo hi ; fi
	false || true
	false && true
	while false; do echo hi; done 
	echo ok
expected-stdout:
	ok
---


name: regression-39
description:
	set -e: errors in command substitutions aren't ignored
	Not clear if they should be or not...
expected-fail: yes
arguments: !-e!
stdin:
	echo `false; echo hi`
expected-stdout:
	hi
---

name: regression-40
description:
	This used to cause a core dump
env-setup: !RANDOM=12!
stdin:
	echo hi
expected-stdout:
	hi
---

name: regression-41
description:
	foo should be set to bar (should not be empty)
stdin:
	foo=`
	echo bar`
	echo "($foo)"
expected-stdout:
	(bar)
---

name: regression-42
description:
	Can't use command line assignments to assign readonly parameters.
stdin:
	foo=bar
	readonly foo
	foo=stuff env | grep '^foo'
expected-exit: e != 0
expected-stderr-pattern:
	/.*read *only.*/
---

name: regression-43
description:
	Can subshells be prefixed by redirections (historical shells allow
	this)
stdin:
	< /dev/null (cat -n)
---

name: regression-44
description:
	getopts sets OPTIND correctly for unparsed option
stdin:
	set -- -a -a -x
	while getopts :a optc; do
	    echo "OPTARG=$OPTARG, OPTIND=$OPTIND, optc=$optc."
	done
	echo done
expected-stdout:
	OPTARG=, OPTIND=2, optc=a.
	OPTARG=, OPTIND=3, optc=a.
	OPTARG=x, OPTIND=3, optc=?.
	done
---

name: regression-45
description:
	Parameter assignments with [] recognized correctly
stdin:
	FOO=*[12]
	BAR=abc[
	MORE=[abc]
	JUNK=a[bc
	echo "<$FOO>"
	echo "<$BAR>"
	echo "<$MORE>"
	echo "<$JUNK>"
expected-stdout:
	<*[12]>
	<abc[>
	<[abc]>
	<a[bc>
---
