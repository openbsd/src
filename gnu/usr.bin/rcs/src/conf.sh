#! /bin/sh
# Output RCS compile-time configuration.
Id='$Id: conf.sh,v 1.2 1996/08/16 18:01:24 millert Exp $'
#	Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
#	Distributed under license by the Free Software Foundation, Inc.

# This file is part of RCS.
#
# RCS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# RCS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with RCS; see the file COPYING.
# If not, write to the Free Software Foundation,
# 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# Report problems and direct all questions to:
#
#	rcs-bugs@cs.purdue.edu


# Standard output should already be directed to "a.h";
# later parts of this procedure need it.
# Standard error can be ignored if a.h is OK,
# and can be inspected for clues otherwise.

# The Makefile overrides the following defaults.
: ${RCSPREFIX=/usr/local/bin/}
: ${ALL_CFLAGS=-Dhas_conf_h}
: ${CC=cc}
: ${COMPAT2=0}
: ${DIFF=${RCSPREFIX}diff}
: ${DIFF3=${DIFF}3}
: ${DIFF3_BIN=1}
: ${DIFFFLAGS=-an}
: ${DIFF_L=1}
: ${DIFF_SUCCESS=0} ${DIFF_FAILURE=1} ${DIFF_TROUBLE=2}
: ${ED=/bin/ed}
: ${SENDMAIL='"/usr/lib/sendmail"'}
# : ${LDFLAGS=} ${LIBS=} tickles old shell bug

C="$CC $ALL_CFLAGS"
CL="$CC $ALL_CFLAGS $LDFLAGS -o a.out"
L=$LIBS

cat <<EOF
/* RCS compile-time configuration */

	/* $Id */

/*
 * This file is generated automatically.
 * If you edit it by hand your changes may be lost.
 * Instead, please try to fix conf.sh,
 * and send your fixes to rcs-bugs@cs.purdue.edu.
 */

EOF

n='
'
case `echo -n` in
-n)
	ech='echo' dots='... \c';;
*)
	ech='echo -n' dots='... '
esac

#$ech >&3 "$0: testing permissions $dots"
#rm -f a.d &&
#date >a.d &&
#chmod 0 a.d &&
#{ test -w a.d || cp /dev/null a.d 2>/dev/null; } && {
#	echo >&3 "$n$0: This command should not be run with superuser permissions."
#	exit 1
#}
#echo >&3 OK
#rm -f a.d || exit

$ech >&3 "$0: testing compiler for plausibility $dots"
echo 'main() { return 0; }' >a.c
rm -f a.exe a.out || exit
$CL a.c $L >&2 || {
	echo >&3 "$n$0: The command '$CL a.c $L' failed on a trivial program."
	exit 1
}
echo 'this is not a C source file' >a.c
rm -f a.exe a.out || exit
$CL a.c $L >&2 && {
	echo >&3 "$n$0: The command '$CL a.c $L' succeeds when it should fail."
	exit 1
}
echo >&3 OK

$ech >&3 "$0: configuring exitmain $dots"
cat >a.c <<EOF
#include "a.h"
int main(argc,argv) int argc; char **argv; { return argc-1; }
EOF
rm -f a.exe a.out || exit
if $CL a.c $L >&2
then A_H=a.h
else
	echo >&3 failed
	$ech >&3 "$0: attempting to work around Domain/OS brain damage $dots"
	cat >a.c <<EOF
#include "a.hap"
int main(argc,argv) int argc; char **argv; { return argc-1; }
EOF
	cat <a.h >a.hap &&
	$CL a.c $L >&2 || exit 1
	# The Domain/OS C compiler refuses to read a.h because the file
	# is currently open for writing.  Work around this brain damage by
	# copying it to a.hap before each compilation; include a.hap instead.
	A_H=a.hap
fi
if test -f a.out
then aout=./a.out
elif test -f a.exe
then aout=./a.exe
else
	echo >&3 "$n$0: C compiler creates neither a.out nor a.exe."
	exit 1
fi
e='exit(n), 3 /* lint fodder */'
if $aout -
then :
elif $aout
then e=n
fi
case $e in
n) echo >&3 OK;;
*) echo >&3 "return does not work; using exit instead"
esac
echo "#define exitmain(n) return $e /* how to exit from main() */"

: PREPARE_CC
case $A_H in
a.h)
	PREPARE_CC="rm -f $aout";;
*)
	echo "rm -f $aout \$1 && cat <a.h >$A_H" >a.pre
	PREPARE_CC="sh a.pre"
esac

for id in _POSIX_C_SOURCE _POSIX_SOURCE
do
	$ech >&3 "$0: configuring $id $dots"
	cat >a.c <<EOF
#include "$A_H"
#include <stdio.h>
int
main() {
#	ifdef fileno
#		define f(x) fileno(x)
#	else
		/* Force a compile-time error if fileno isn't declared.  */
		int (*p)() = fileno;
#		define f(x) (*p)(x)
#	endif
	/* Some buggy linkers seem to need the getchar.  */
	exitmain(getchar() != '#' || fileno(stdout) != 1);
}
#if syntax_error
syntax error
#endif
EOF
	a='/* ' z='*/ '
	case $id in
	_POSIX_SOURCE)
		version=1003.1-1990
		value=;;
	_POSIX_C_SOURCE)
		version='1003.1b-1993 or later'
		value='2147483647L ';;
	esac
	$PREPARE_CC || exit
	if ($CL a.c $L && $aout <a.c) >&2
	then :
	elif $PREPARE_CC || exit; ($CL -D$id=$value a.c $L && $aout <a.c) >&2
	then a= z=
	fi
	case $a in
	?*) echo >&3 OK;;
	'') echo >&3 "must define it, unfortunately"
	esac
	echo "$a#define $id $value$z/* if strict C + Posix $version */"
done

cat <<'EOF'

#include <errno.h>
#include <stdio.h>
#include <time.h>

/* Comment out #include lines below that do not work.  */
EOF

$ech >&3 "$0: configuring how to check for syntax errors $dots"
# Run `$CS a.c $LS' instead of `$CL a.c $L' for compile-time checking only.
# This speeds up the configuration process.
if
	rm -f a.s && $C -S a.c >&2 && test -s a.s && rm -f a.s &&
	if $C -S -Dsyntax_error=1 a.c >&2 && test -s a.s
	then false
	else :
	fi
then
	# Generate assembly language output.
	CS="$C -S" LS= o=a.s PREPARE_CC="$PREPARE_CC $o"
elif
	rm -f a.o a.obj && $C -c a.c >&2 &&
	if test -s a.o
	then o=a.o
	elif test -s a.obj
	then o=a.obj
	else false
	fi &&
	if $C -c -Dsyntax_error=1 a.c >&2 && test -s $o
	then false
	else :
	fi
then
	# Generate object code.
	CS="$C -c" LS= PREPARE_CC="$PREPARE_CC $o"
else
	# Generate an executable.
	CS=$CL LS=$L o=$aout
fi
CS_OK="test -s $o"
echo >&3 $CS

# standard include files
# sys/types.h and sys/stat.h must come first because others depend on them.
has_signal=1
for h in \
	sys/types sys/stat \
	dirent fcntl limits mach/mach net/errno \
	pwd siginfo signal stdlib string \
	sys/mman sys/wait ucontext unistd utime vfork
do
	i="#include <$h.h>"
	$ech >&3 "$0: configuring $i $dots"
	cat >a.c <<EOF
#include "$A_H"
$i
int main(){ exitmain(0); }
EOF
	ok=OK
	$PREPARE_CC || exit
	$CS a.c $LS >&2 && $CS_OK || {
		case $h in
		string)
			i='#include <strings.h>'
			ok="$i instead";;
		*)
			i="/* $i */"
			ok="commenting it out"
		esac
		case $h in
		signal) has_signal=0
		esac
	}
	echo >&3 "$ok"
	echo "$i"
done

cat <<'EOF'

/* Define boolean symbols to be 0 (false, the default), or 1 (true).  */
EOF

# has_sys_param_h
$ech >&3 "$0: configuring has_sys_param_h $dots"
cat >a.c <<EOF
#include "$A_H"
#include <sys/param.h>
int main() { exitmain(0); }
EOF
$PREPARE_CC || exit
if $CS a.c $LS >&2 && $CS_OK
then h=1 ok=OK
else h=0 ok=absent
fi
echo >&3 $ok
echo "#define has_sys_param_h $h /* Does #include <sys/param.h> work?  */"

# We must do errno next, because has_readlink needs it.
/* <errno.h> */
$ech >&3 "$0: configuring errno $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(errno != errno); }
EOF
$PREPARE_CC || exit
if $CS a.c $LS >&2
then a='/* ' z=' */' ok=OK
else a= z= ok='declaration missing'
fi
echo >&3 $ok
echo "${a}extern int errno;$z /* Uncomment if <errno.h> doesn't declare errno.  */"
rm -f a.c || exit

# We must do has_readlink next, because it might generate
# #include directives that affect later definitions.

$ech >&3 "$0: configuring has_readlink, readlink_isreg_errno $dots"
cat >a.c <<EOF
#include "$A_H"
static char b[7];
int
main() {
	if (readlink("a.sym2",b,7) == 6  &&  strcmp(b,"a.sym1") == 0  &&
		readlink("a.c",b,7) == -1  &&  errno != ENOENT
	) {
		if (errno == EINVAL)
			printf("EINVAL\n");
		else
			printf("%d\n", errno);
		exitmain(ferror(stdout) || fclose(stdout)!=0);
	}
	exitmain(1);
}
EOF
$PREPARE_CC a.sym* || exit
readlink_isreg_errno='?'
if (ln -s a.sym1 a.sym2 && $CL a.c $L) >&2 && readlink_isreg_errno=`$aout`
then h=1
else h=0
fi
echo >&3 $h, $readlink_isreg_errno
cat <<EOF
#define has_readlink $h /* Does readlink() work?  */
#define readlink_isreg_errno $readlink_isreg_errno /* errno after readlink on regular file */

#if has_readlink && !defined(MAXSYMLINKS)
#	if has_sys_param_h
#		include <sys/param.h>
#	endif
#	ifndef MAXSYMLINKS
#		define MAXSYMLINKS 20 /* BSD; not standard yet */
#	endif
#endif
EOF

# *_t
cat <<'EOF'

/* Comment out the typedefs below if the types are already declared.  */
/* Fix any uncommented typedefs that are wrong.  */
EOF
cat >a.c <<EOF
#include "$A_H"
t x;
int main() { exitmain(0); }
EOF
for t in mode_t off_t pid_t sig_atomic_t size_t ssize_t time_t uid_t
do
	$ech >&3 "$0: configuring $t $dots"
	case $t in
	size_t) i=unsigned;;
	off_t|time_t) i=long;;
	*) i=int;;
	esac
	$PREPARE_CC || exit
	if $CS -Dt=$t a.c $LS >&2 && $CS_OK
	then ok=OK a='/* ' z=' */'
	else ok=$i a= z=
	fi
	echo >&3 $ok
	echo "${a}typedef $i $t;$z"
done

cat <<'EOF'

/* Comment out the keyword definitions below if the keywords work.  */
EOF

for i in const volatile
do
	$ech >&3 "$0: configuring $i $dots"
	cat >a.c <<EOF
#	include "$A_H"
	int main();
	enum Boolean { false, true };
	int hpux8_05barf();
	static enum Boolean $i zero;
	int hpux8_05barf(x) int x; { return x; }
	static enum Boolean $i * $i azero = &zero;
	static enum Boolean $i * $i * $i aazero = &azero;
	static enum Boolean * $i arzero[1];
	static sig_atomic_t $i sigzero;
	int sco3_2v4barf() {
		/* SCO 3.2v4 native compiler barfs on this.  */
		char *t;
		char $i *s = 0 ? (char *)0 : (char $i *)0;
		*t++ = '\0';
	}
	int main() {
		enum Boolean *p = arzero[sigzero];
		switch (zero) {
			case false: exitmain(!p || **aazero);
			default: exitmain(hpux8_05barf(1));
		}
	}
EOF
	a= z= ok='broken'
	$PREPARE_CC || exit
	if $CS a.c $LS >&2 && $CS_OK
	then
		cat >a.c <<EOF
			char $i *p;
			char *q;
			typedef unsigned char $i *Iptr_type;
			struct { Iptr_type lim; } s, *f = &s;
			int main() {
				Iptr_type lim;
				lim = f->lim;
				p = p == q ? p : "";
				p = p == "" ? p : q;
				return !!lim;
			}
EOF
		$PREPARE_CC || exit
		if $CS a.c $LS >&2 && $CS_OK
		then
			case $i in
			const)
				# Check for common execv misdeclaration.
				cat >a.c <<EOF
#					include "$A_H"
					static char * const *p;
					int main() { return execv("/bin/sh", p); }
EOF
				$PREPARE_CC || exit
				$CS a.c $LS >&2 && $CS_OK
			esac && a='/* ' z=' */' ok=OK
		fi
	fi
	echo >&3 $ok
	echo "$a#define $i$z"
done

echo >&3 "$0: configuring has_prototypes, has_stdarg, has_varargs, va_start_args $dots"
cat >a.ha <<'EOF'
#if has_prototypes
#	define P(params) params
#else
#	define P(params) ()
#endif
#if has_stdarg
#	include <stdarg.h>
#else
#	if has_varargs
#		include <varargs.h>
#	else
		typedef char *va_list;
#		define va_dcl int va_alist;
#		define va_start(ap) ((ap) = (va_list)&va_alist)
#		define va_arg(ap,t) (((t*) ((ap)+=sizeof(t)))  [-1])
#		define va_end(ap)
#	endif
#endif
#if va_start_args == 2
#	define vararg_start va_start
#else
#	define vararg_start(ap,p) va_start(ap)
#endif
EOF
cat >a.c <<EOF
#include "$A_H"
#include "a.ha"

struct buf { int x; };
int pairnames P((int,char**,FILE*(*)P((struct buf*,struct stat*,int)),int,int)); /* a la rcsbase.h */
FILE *(*rcsopen)P((struct buf*,struct stat*,int));  /* a la rcsfnms.c */

static char *e(p,i) char **p; int i; { return p[i]; }
#if has_prototypes
static char *f(char *(*g)(char**,int), char **p, ...)
#else
static char *f(g, p, va_alist) char *(*g)(); char **p; va_dcl
#endif
{
	char *s;
	va_list v;
	vararg_start(v,p);
	s = g(p, va_arg(v,int));
	va_end(v);
	return s;
}
int main P((int, char**));
int
main(argc, argv) int argc; char **argv; {
	exitmain(f(e,argv,0) != argv[0]  ||  f(e,argv,1) != argv[1]);
}
EOF
for has_prototypes in 1 0
do
	for has_stdarg in 1 v 0
	do
		case $has_stdarg in
		1) has_varargs=-1;;
		v) has_varargs=1 has_stdarg=0;;
		*) has_varargs=0
		esac
		case $has_stdarg in
		0) as='1 2';;
		1) as='2 1'
		esac
		for va_start_args in $as
		do
			$PREPARE_CC || exit
			$CL \
				-Dhas_prototypes=$has_prototypes \
				-Dhas_stdarg=$has_stdarg \
				-Dhas_varargs=$has_varargs \
				-Dva_start_args=$va_start_args \
				a.c $L >&2 && $aout && break
		done && break
	done && break
done || {
	echo >&3 $0: cannot deduce has_prototypes, has_stdarg, va_start_args
	exit 1
}
echo >&3 $has_prototypes, $has_stdarg, $has_varargs, $va_start_args
case $has_varargs in
-1) a='/* ' z='*/ ' has_varargs='?';;
*) a= z=
esac
cat - a.ha <<EOF

/* Define boolean symbols to be 0 (false, the default), or 1 (true).  */
#define has_prototypes $has_prototypes /* Do function prototypes work?  */
#define has_stdarg $has_stdarg /* Does <stdarg.h> work?  */
$a#define has_varargs $has_varargs $z/* Does <varargs.h> work?  */
#define va_start_args $va_start_args /* How many args does va_start() take?  */

#if O_BINARY
	/* Text and binary i/o behave differently.  */
	/* This is incompatible with Posix and Unix.  */
#	define FOPEN_RB "rb"
#	define FOPEN_R_WORK (Expand==BINARY_EXPAND ? "r" : "rb")
#	define FOPEN_WB "wb"
#	define FOPEN_W_WORK (Expand==BINARY_EXPAND ? "w" : "wb")
#	define FOPEN_WPLUS_WORK (Expand==BINARY_EXPAND ? "w+" : "w+b")
#	define OPEN_O_BINARY O_BINARY
#else
	/*
	* Text and binary i/o behave the same.
	* Omit "b", since some nonstandard hosts reject it.
	*/
#	define FOPEN_RB "r"
#	define FOPEN_R_WORK "r"
#	define FOPEN_WB "w"
#	define FOPEN_W_WORK "w"
#	define FOPEN_WPLUS_WORK "w+"
#	define OPEN_O_BINARY 0
#endif

/* This may need changing on non-Unix systems (notably DOS).  */
#define OPEN_CREAT_READONLY (S_IRUSR|S_IRGRP|S_IROTH) /* lock file mode */
#define OPEN_O_LOCK 0 /* extra open flags for creating lock file */
#define OPEN_O_WRONLY O_WRONLY /* main open flag for creating a lock file */

/* Define or comment out the following symbols as needed.  */
EOF

$ech >&3 "$0: configuring bad_chmod_close $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef O_RDONLY
#	define O_RDONLY 0
#endif
int
main() {
	int f;
	exitmain(
		(f = open("a.c", O_RDONLY)) < 0 ||
		chmod("a.c", 0) != 0 ||
		close(f) != 0
	);
}
EOF
$PREPARE_CC || exit
if $CL a.c $L >&2 && $aout
then b=0 ok=OK
else b=1 ok='will work around bug'
fi
echo >&3 $ok
echo "#define bad_chmod_close $b /* Can chmod() close file descriptors?  */"
rm -f a.c || exit

$ech >&3 "$0: configuring bad_creat0 $dots"
cat >a.c <<EOF
#include "$A_H"
#if defined(O_CREAT) && defined(O_WRONLY)
#	define creat0(f) open(f, O_CREAT|O_WRONLY, 0)
#else
#	define creat0(f) creat(f, 0)
#endif
char buf[17000];
int
main() {
	int f;
	exitmain(
		(f = creat0("a.d")) < 0  ||
		write(f, buf, sizeof(buf)) != sizeof(buf) ||
		close(f) != 0
	);
}
EOF
$PREPARE_CC a.d || exit
if $CL a.c $L >&2 && $aout && test -f a.d && test ! -w a.d
then b=0 ok=OK
else b=1 ok='will work around bug'
fi
echo >&3 $ok
echo "#define bad_creat0 $b /* Do writes fail after creat(f,0)?  */"
rm -f a.d || exit

$ech >&3 "$0: configuring bad_fopen_wplus $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(!fopen("a.d","w+")); }
EOF
$PREPARE_CC || exit
if echo nonempty >a.d && $CL a.c $L >&2 && $aout && test ! -s a.d
then b=0 ok=OK
else b=1 ok='will work around bug'
fi
echo >&3 $ok
echo "#define bad_fopen_wplus $b /* Does fopen(f,\"w+\") fail to truncate f?  */"

echo "#define getlogin_is_secure 0 /* Is getlogin() secure?  Usually it's not.  */"

$ech >&3 "$0: configuring has_attribute_noreturn $dots"
cat >a.c <<EOF
#include "$A_H"
static void e P((int)) __attribute__((noreturn));
static void e(i) int i; { exit(i); }
int main() { e(0); }
EOF
$PREPARE_CC || exit
h=0 ok='does not work'
if out=`$CS a.c $LS 2>&1`
then
	case $out in
	*noreturn*) ;;
	*) h=1 ok=OK
	esac
fi
echo >&2 "$out"
echo >&3 $ok
cat <<EOF
#define has_attribute_noreturn $h /* Does __attribute__((noreturn)) work?  */
#if has_attribute_noreturn
#	define exiting __attribute__((noreturn))
#else
#	define exiting
#endif
EOF
rm -f a.c || exit

$ech >&3 "$0: configuring has_dirent, void_closedir $dots"
cat >a.c <<EOF
#include "$A_H"
#if void_closedir
#	define close_directory(d) (closedir(d), 0)
#else
#	define close_directory(d) closedir(d)
#endif
int
main() {
	DIR *d = opendir(".");
	struct dirent *e;
	while ((e = readdir(d)))
		if (strcmp(e->d_name, "a.c") == 0  &&  close_directory(d) == 0)
			exitmain(0);
	exitmain(1);
}
EOF
$PREPARE_CC || exit
has_dirent=0 ok='does not work'
void_closedir=? a='/* ' z='*/ '
for v in 0 1
do
	if $CL -Dvoid_closedir=$v a.c $L >&2 && $aout
	then
		has_dirent=1 ok=OK
		void_closedir=$v a= z=
		case $v in
		1) ok='OK, but closedir yields void'
		esac
		break
	fi
done
echo >&3 $ok
echo "#define has_dirent $has_dirent /* Do opendir(), readdir(), closedir() work?  */"
echo "$a#define void_closedir $void_closedir $z/* Does closedir() yield void?  */"

$ech >&3 "$0: configuring has_fchmod $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
int main() { exitmain(fchmod(STDIN_FILENO,0) != 0); }
EOF
$PREPARE_CC || exit
if $CL a.c $L >&2 && $aout <a.c && test ! -r a.c
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_fchmod $h /* Does fchmod() work?  */"
rm -f a.c || exit

$ech >&3 "$0: configuring has_fflush_input $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
int main() {
	exitmain(
		getchar() == EOF
		|| fseek(stdin, 0L, SEEK_SET) != 0
		|| fflush(stdin) != 0
		|| lseek(STDIN_FILENO, (off_t)0, SEEK_CUR) != 0
	);
}
EOF
$PREPARE_CC || exit
if $CL a.c $L >&2 && $aout <a.c
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_fflush_input $h /* Does fflush() work on input files?  */"
rm -f a.c || exit

$ech >&3 "$0: configuring has_fputs $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(fputs("Hello\"\nworld", stdout) < 0); }
EOF
Hello='Hello"
world'
$PREPARE_CC a.a || exit
if $CL a.c $L >&2 && $aout >a.a && x=`$aout` && test " $x" = " $Hello"
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_fputs $h /* Does fputs() work?  */"

$ech >&3 "$0: configuring has_ftruncate $dots"
cat >a.c <<EOF
#include "$A_H"
/*
 * We'd like to test ftruncate(creat(f,0), 0),
 * since that's the way RCS uses it,
 * but a common bug causes it to fail over NFS.
 * Since we must defend against this bug at run time anyway,
 * we don't bother to check for it at compile time.
 * So we test ftruncate(creat(f,0200), 0) instead.
 */
#if defined(O_CREAT) && defined(O_WRONLY) && defined(S_IWUSR)
#	define creat0200(f) open(f, O_CREAT|O_WRONLY, S_IWUSR)
#else
#	define creat0200(f) creat(f, 0200)
#endif
int
main(argc, argv) int argc; char **argv; {
	int f = creat0200(argv[1]);
	if (f<0 || write(f,"abc",3)!=3 || ftruncate(f,(off_t)0)!=0 || close(f)!=0)
		exitmain(1);
	exitmain(0);
}
EOF
$PREPARE_CC a.a || exit
if ($CL a.c $L && $aout a.a && test -w a.a && test ! -s a.a) >&2
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_ftruncate $h /* Does ftruncate() work?  */"

$ech >&3 "$0: configuring has_getuid $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(getuid()!=getuid()); }
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then has_getuid=1 ok=OK
else has_getuid=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_getuid $has_getuid /* Does getuid() work?  */"

case $has_getuid in
0)
	a='/* ' z='*/ ' h=?;;
*)
	$ech >&3 "$0: configuring has_getpwuid $dots"
	a= z=
	cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(!getpwuid(0)); }
EOF
	$PREPARE_CC || exit
	if ($CL a.c $L && $aout) >&2
	then h=1 ok=OK
	else h=0 ok='does not work'
	fi
	echo >&3 $ok
esac
echo "$a#define has_getpwuid $h $z/* Does getpwuid() work?  */"

$ech >&3 "$0: configuring has_kill $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(kill(getpid(), 0) != 0); }
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then has_kill=1 ok=OK
else has_kill=0 ok='does not work'
fi
echo >&3 $ok
# Used only by this script, not by RCS, so we don't output it to stdout.

$ech >&3 "$0: configuring has_memcmp $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(memcmp("beautiful","beautiful",10) != 0); }
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_memcmp $h /* Does memcmp() work?  */"

$ech >&3 "$0: configuring has_memcpy $dots"
cat >a.c <<EOF
#include "$A_H"
char a[3];
int
main() {
	memcpy(a,"xy",3);
	exitmain(strcmp(a,"xy")!=0);
}
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_memcpy $h /* Does memcpy() work?  */"

$ech >&3 "$0: configuring has_memmove $dots"
cat >a.c <<EOF
#include "$A_H"
static char a[4];
int
main() {
	strcpy(a, "xy");
	memmove(a+1, a, 3);
	exitmain(strcmp(a,"xxy")!=0);
}
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_memmove $h /* Does memmove() work?  */"

$ech >&3 "$0: configuring has_map_fd, has_mmap, has_madvise, mmap_signal $dots"
rm -f a.c a.d a.e || exit
cat >a.c <<EOF
#define CHAR1 '#' /* the first character in this file */
#include "$A_H"
static char *a;
static struct stat b;
#ifndef MADVISE_OK
#define MADVISE_OK (madvise(a,b.st_size,MADV_SEQUENTIAL)==0 && madvise(a,b.st_size,MADV_NORMAL)==0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(s) ((s)&0177)
#undef WIFSIGNALED /* Avoid 4.3BSD incompatibility with Posix.  */
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(s) (((s)&0377) != 0177  &&  WTERMSIG(s) != 0)
#endif
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
int
main(argc, argv) int argc; char **argv; {
	int s = 0;
#if TRY_MAP_FD
	kern_return_t kr;
	vm_address_t va;
#endif

	if (fstat(STDIN_FILENO, &b) != 0) {
		perror("fstat");
		exitmain(1);
	}
#	if TRY_MAP_FD
		kr = map_fd(STDIN_FILENO, 0, &va, TRUE, b.st_size);
		if (kr != KERN_SUCCESS) {
			mach_error("map_fd", kr);
			exitmain(1);
		}
		a = (char *) va;
#	else
		a = mmap(
			(char *)0, b.st_size, PROT_READ, MAP_SHARED,
			STDIN_FILENO, (off_t)0
		);
		if (a == (char *)MAP_FAILED) {
			perror("mmap");
			exitmain(1);
		}
		if (!MADVISE_OK) {
			perror("madvise");
			exitmain(1);
		}
#	endif
	if (*a != CHAR1)
		exitmain(1);
	if (1 < argc) {
		pid_t p, w;
		int f = creat(argv[1], 0);
		/*
		* Some buggy hosts yield ETXTBSY if you try to use creat
		* to truncate a file that is mmapped.  On such hosts,
		* don't bother to try to figure out what mmap_signal is.
		*/
#		ifndef ETXTBSY
#			define ETXTBSY (-1)
#		endif
		if (f<0 ? errno!=ETXTBSY : close(f)!=0) {
			perror(argv[1]);
			exitmain(1);
		}
		if ((p = fork()) < 0) {
			perror("fork");
			exitmain(1);
		}
		if (!p)
			/* Refer to nonexistent storage, causing a signal in the child.  */
			_exit(a[0] != 0);
		while ((w = wait(&s)) != p)
			if (w < 0) {
				perror("wait");
				exitmain(1);
			}
		s = WIFSIGNALED(s) ? WTERMSIG(s) : 0;
	}
#	if TRY_MAP_FD
		kr = vm_deallocate(task_self(), va, (vm_size_t) b.st_size);
		if (kr != KERN_SUCCESS) {
			mach_error("vm_deallocate", kr);
			exitmain(1);
		}
#	else
		if (munmap(a, b.st_size)  !=  0) {
			perror("munmap");
			exitmain(1);
		}
#	endif
	if (1 < argc) {
#		ifdef SIGBUS
			if (s == SIGBUS) { printf("SIGBUS\n"); s = 0; }
#		endif
#		ifdef SIGSEGV
			if (s == SIGSEGV) { printf("SIGSEGV\n"); s = 0; }
#		endif
		if (s) printf("%d\n", s);
	}
	exitmain(ferror(stdout) || fclose(stdout)!=0);
}
EOF
# AIX 3.2.0 read-only mmap updates last-modified time of file!  Check for this.
sleep 2
cp a.c a.d || exit
sleep 2
has_map_fd=? has_mmap=? has_madvise=? mmap_signal=
case `(uname -s -r -v) 2>/dev/null` in
'HP-UX '[A-Z].08.07*) ;;
	# mmap can crash the OS under HP-UX 8.07, so don't even test for it.
'HP-UX '[A-Z].09.*) ;;
	# HP-UX 9.0[135]? s700 mmap has a data integrity problem
	# when a diskless cnode accesses data on the cnode's server disks.
	# We don't know of any way to test whether the bug is present.
	# HP patches PHKL_4605 and PHKL_4607 should fix the bug;
	# see <http://support.mayfield.hp.com/slx/html/ptc_hpux.html>.
	# The above code (perhaps rashly) assumes HP-UX 10 supports mmap.
'SunOS 5.4 Generic' | 'SunOS 5.4 Generic_101945-?') ;;
	# Early editions of SunOS 5.4 are reported to have problems with mmap
	# that generate NUL bytes in RCS files with a Solaris 2.2 NFS server.
	# This has been reported to be fixed as of patch 101945-10.
*)
	$PREPARE_CC || exit
	if ($CL -DTRY_MAP_FD=1 a.c $L && $aout <a.c) >&2
	then
		has_map_fd=1
	else
		has_map_fd=0 has_mmap=0 has_madvise=0
		if ($CL -DMADVISE_OK=1 a.c $L && $aout <a.c) >&2
		then
			case `ls -t a.c a.d` in
			a.d*)
				has_mmap=1
				rm -f a.ous
				mv $aout a.ous
				$PREPARE_CC || exit
				if ($CL a.c $L && $aout <a.c) >&2
				then has_madvise=1; rm -f a.ous
				else rm -f $aout && mv a.ous $aout
				fi || exit
			esac
		fi
	fi
	case $has_map_fd$has_mmap in
	*1*)
		# Find out what signal is sent to RCS
		# when someone unexpectedly truncates a file
		# while RCS has it mmapped.
		rm -f a.e && cp a.c a.e &&
		mmap_signal=`$aout a.e <a.e` || exit
	esac
esac
echo >&3 $has_map_fd, $has_mmap, $has_madvise, $mmap_signal
case $has_map_fd in
'?') a='/* ' z='*/ ';;
*) a= z=;;
esac
echo "$a#define has_map_fd $has_map_fd $z/* Does map_fd() work?  */"
case $has_mmap in
'?') a='/* ' z='*/ ';;
*) a= z=;;
esac
echo "$a#define has_mmap $has_mmap $z/* Does mmap() work on regular files?  */"
echo "$a#define has_madvise $has_madvise $z/* Does madvise() work?  */"
case $mmap_signal in
?*) a= z=;;
'') a='/* ' z='*/ ' mmap_signal='?'
esac
echo "$a#define mmap_signal $mmap_signal $z/* signal received if you reference nonexistent part of mmapped file */"

$ech >&3 "$0: configuring has_rename, bad_a_rename, bad_b_rename $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(rename("a.a","a.b") != 0); }
EOF
echo a >a.a && $PREPARE_CC a.b || exit
if ($CL a.c $L && $aout && test -f a.b) >&2
then
	h=1
	rm -f a.a a.b &&
	echo a >a.a && chmod -w a.a || exit
	if $aout && test ! -f a.a && test -f a.b
	then a=0
	else a=1
	fi
	rm -f a.a a.b &&
	echo a >a.a && echo b >a.b && chmod -w a.b || exit
	if $aout && test ! -f a.a && test -f a.b
	then b=0
	else b=1
	fi
	rm -f a.a a.b || exit
else h=0 a=0 b=0
fi
echo >&3 $h, $a, $b
echo "#define has_rename $h /* Does rename() work?  */"
echo "#define bad_a_rename $a /* Does rename(A,B) fail if A is unwritable?  */"
echo "#define bad_b_rename $b /* Does rename(A,B) fail if B is unwritable?  */"
echo "#define bad_NFS_rename 0 /* Can rename(A,B) falsely report success?  */"

$ech >&3 "$0: configuring void, VOID $dots"
cat >a.c <<EOF
#include "$A_H"
void f() {}
int main() {f(); exitmain(0);}
EOF
$PREPARE_CC || exit
if $CS a.c $LS >&2 && $CS_OK
then
	v='(void) '
	echo '/* typedef int void; */ /* Some ancient compilers need this.  */'
	ok=OK
else
	v=
	echo 'typedef int void;'
	ok='your compiler is a museum piece'
fi
echo >&3 $ok
echo "#define VOID $v/* 'VOID e;' discards the value of an expression 'e'.  */"

case $has_getuid in
0)
	a='/* ' z='*/ ' has_seteuid=?;;
*)
	$ech >&3 "$0: configuring has_seteuid $dots"
	a= z=
	cat >a.c <<EOF
#include "$A_H"
int
main() {
/* Guess, don't test.  Ugh.  Testing would require running conf.sh setuid.  */
/* If the guess is wrong, a setuid RCS will detect the problem at runtime.  */
#if !_POSIX_VERSION
	exitmain(1);
#else
	exitmain(seteuid(geteuid()) != 0);
#endif
}
EOF
	$PREPARE_CC || exit
	if ($CL a.c $L && $aout) >&2
	then has_seteuid=1 ok='OK, I guess'
	else has_seteuid=0 ok='does not work'
	fi
	echo >&3 $ok
esac
echo "$a#define has_seteuid $has_seteuid $z/* Does seteuid() work?  See ../INSTALL.RCS.  */"

echo "#define has_setreuid 0 /* Does setreuid() work?  See ../INSTALL.RCS.  */"

$ech >&3 "$0: configuring has_setuid $dots"
h=$has_seteuid
case $h in
0)
	cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(setuid(getuid()) != 0); }
EOF
	$PREPARE_CC || exit
	($CL a.c $L && $aout) >&2 && h=1 ok='OK, I guess'
esac
echo >&3 $ok
echo "$a#define has_setuid $h $z/* Does setuid() exist?  */"

$ech >&3 "$0: configuring has_sigaction $dots"
cat >a.c <<EOF
#include "$A_H"
static sig_atomic_t volatile gotsig;
#ifdef SA_SIGINFO
  static void catchsig(i, s, v) int i; siginfo_t *s; void *v; { gotsig = 1; }
#else
  static void catchsig(i) int i; { gotsig = 1; }
#endif
int
main(argc, argv) int argc; char **argv; {
	struct sigaction s;
	if (sigaction(SIGINT, (struct sigaction*)0, &s) != 0)
		exitmain(1);
#	if has_sa_sigaction
		s.sa_sigaction = catchsig;
#	else
		s.sa_handler = catchsig;
#	endif
#	ifdef SA_SIGINFO
		s.sa_flags |= SA_SIGINFO;
#	endif
	if (sigaddset(&s.sa_mask, SIGINT) != 0)
		exitmain(1);
	if (sigaction(SIGINT, &s, (struct sigaction*)0) != 0)
		exitmain(1);
#	if has_kill
		kill(getpid(), SIGINT);
#	else
		raise(SIGINT);
#	endif
	exitmain(gotsig != 1);
}
EOF
$PREPARE_CC || exit
if ($CL -Dhas_kill=$has_kill a.c $L && $aout) >&2
then has_sigaction=1 ok=OK
else has_sigaction=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_sigaction $has_sigaction /* Does struct sigaction work?  */"
$ech >&3 "$0: configuring has_sa_sigaction $dots"
has_sa_sigaction=0 ok='does not work'
case $has_sigaction in
1)
	$PREPARE_CC || exit
	if ($CL -Dhas_kill=$has_kill -Dhas_sa_sigaction=1 a.c $L && $aout) >&2
	then has_sa_sigaction=1 ok=OK
	fi
esac
echo >&3 $ok
echo "#define has_sa_sigaction $has_sa_sigaction /* Does struct sigaction have sa_sigaction?  */"

$ech >&3 "$0: configuring has_signal, signal_type, sig_zaps_handler $dots"
case $has_signal,$has_sigaction in
1,0)
	cat >a.c <<EOF
#include "$A_H"
#if !defined(signal) && declare_signal
	signal_type (*signal P((int,signal_type(*)signal_args)))signal_args;
#endif
static signal_type nothing(i) int i; {}
int
main(argc, argv) int argc; char **argv; {
	signal(SIGINT, nothing);
#	if has_kill
		while (--argc)
			kill(getpid(), SIGINT);
		exitmain(0);
#	else
		/* Pretend that sig_zaps_handler; better safe than sorry.  */
		exitmain(2 < argc);
#	endif
}
EOF
	for declare_signal in 1 0
	do
		for signal_type in void int
		do
			for signal_args in 'P((int))' '()'
			do
				$PREPARE_CC || exit
				($CL \
					-Ddeclare_signal=$declare_signal \
					-Dhas_kill=$has_kill \
					-Dsignal_args="$signal_args" \
					-Dsignal_type=$signal_type \
						a.c $L && $aout 1) >&2 && break
			done && break
		done && break
	done || {
		echo >&3 $0: cannot deduce signal_type
		exit 1
	}
	if $aout 1 2 >&2
	then sig_zaps_handler=0
	else sig_zaps_handler=1
	fi;;
*)
	signal_type=void
	sig_zaps_handler=0
esac
echo >&3 $has_signal, $signal_type, $sig_zaps_handler
cat <<EOF
#define has_signal $has_signal /* Does signal() work?  */
#define signal_type $signal_type /* type returned by signal handlers */
#define sig_zaps_handler $sig_zaps_handler /* Must a signal handler reinvoke signal()?  */
EOF

a='/* ' z='*/ '
b='/* ' y='*/ '
case $has_sigaction in
1)
	h=?;;
*)
	$ech >&3 "$0: configuring has_sigblock, sigmask $dots"
	ok=OK
	a= z=
	cat >a.c <<EOF
#include "$A_H"
#include <signal.h>
#if define_sigmask
#	define sigmask(s) (1 << ((s)-1))
#endif
int
main() {
	sigblock(sigmask(SIGHUP));
#	if has_kill
		exitmain(kill(getpid(), SIGHUP) != 0);
#	else
		exitmain(raise(SIGHUP) != 0);
#	endif
}
EOF
	if
		$PREPARE_CC || exit
		($CL -Dhas_kill=$has_kill a.c $L && $aout) >&2
	then h=1
	elif
		$PREPARE_CC || exit
		($CL -Dhas_kill=$has_kill -Ddefine_sigmask=1 a.c $L && $aout) >&2
	then h=1 b= y= ok='definition needed'
	else h=0
	fi
	echo >&3 "$h, $ok"
esac
echo "$a#define has_sigblock $h $z/* Does sigblock() work?  */"
echo "$b#define sigmask(s) (1 << ((s)-1)) $y/* Yield mask for signal number.  */"

$ech >&3 "$0: configuring fread_type, freadarg_type $dots"
cat >a.c <<EOF
#define CHAR1 '#' /* the first character in this file */
#include "$A_H"
#if !defined(fread) && declare_fread
	fread_type fread P((void*,freadarg_type,freadarg_type,FILE*));
#endif
int
main() {
	char b;
	exitmain(!(
		fread(&b, (freadarg_type)1, (freadarg_type)1, stdin) == 1  &&
		b==CHAR1
	));
}
EOF
for declare_fread in 1 0
do
	for fread_type in ssize_t size_t int unsigned
	do
		for freadarg_type in size_t ssize_t unsigned int
		do
			$PREPARE_CC || exit
			(
				$CL \
					-Ddeclare_fread=$declare_fread \
					-Dfreadarg_type=$freadarg_type \
					-Dfread_type=$fread_type \
					a.c $L &&
				$aout <a.c
			) >&2 && break
		done && break
	done && break
done || {
	echo >&3 $0: cannot deduce fread types
	exit 1
}
echo >&3 $fread_type, $freadarg_type
cat <<EOF
typedef $fread_type fread_type; /* type returned by fread() and fwrite() */
typedef $freadarg_type freadarg_type; /* type of their size arguments */
EOF

$ech >&3 "$0: configuring malloc_type $dots"
cat >a.c <<EOF
#include "$A_H"
typedef void *malloc_type;
#ifndef malloc
	malloc_type malloc();
#endif
static malloc_type identity P((malloc_type));
static malloc_type identity(x) malloc_type x; { return x; }
int main() { exitmain(!identity(malloc(1))); }
EOF
$PREPARE_CC || exit
if $CS a.c $LS >&2 && $CS_OK
then t=void
else t=char
fi
echo >&3 $t
echo "typedef $t *malloc_type; /* type returned by malloc() */"

$ech >&3 "$0: configuring has_getcwd $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef getcwd
	char *getcwd();
#endif
static char buf[10000];
int main() { exitmain(!getcwd(buf,10000)); }
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then has_getcwd=1 ok=OK
else has_getcwd=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_getcwd $has_getcwd /* Does getcwd() work?  */"

case $has_getcwd in
1)
	a='/* ' z='*/ ' h=?;;
*)
	a= z=
	$ech >&3 "$0: configuring has_getwd $dots"
	cat >a.c <<EOF
#include "$A_H"
#include <sys/param.h>
#ifndef getwd
	char *getwd();
#endif
static char buf[MAXPATHLEN];
int main() { exitmain(!getwd(buf)); }
EOF
	$PREPARE_CC || exit
	if ($CL a.c $L && $aout) >&2
	then h=1 ok=OK
	else h=0 ok='does not work'
	fi
	echo >&3 $ok
esac
echo "$a#define has_getwd $h $z/* Does getwd() work?  */"
echo "#define needs_getabsname 0 /* Must we define getabsname?  */"

$ech >&3 "$0: configuring has_mktemp $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef mktemp
	char *mktemp();
#endif
int
main() {
	char b[9];
	strcpy(b, "a.XXXXXX");
	exitmain(!mktemp(b));
}
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then h=1 ok=OK
else h=0 ok=absent
fi
echo >&3 $ok
echo "#define has_mktemp $h /* Does mktemp() work?  */"

: configuring has_NFS
echo "#define has_NFS 1 /* Might NFS be used?  */"

case $has_signal,$has_sigaction in
1,0)
	has_psiginfo=0;;
*)
	$ech >&3 "$0: configuring has_psiginfo $dots"
	cat >a.c <<EOF
#include "$A_H"
static signal_type
catchsig(s, i, c) int s; siginfo_t *i; void *c; {
	if (i)
		psiginfo(i, "test");
	exit(0);
}
int
main() {
	struct sigaction s;
	if (sigaction(SIGINT, (struct sigaction*)0, &s) != 0)
		exitmain(1);
#	if has_sa_sigaction
		s.sa_sigaction = catchsig;
#	else
		s.sa_handler = catchsig;
#	endif
	if (sigaddset(&s.sa_mask, SIGINT) != 0)
		exitmain(1);
	s.sa_flags |= SA_SIGINFO;
	if (sigaction(SIGINT, &s, (struct sigaction*)0) != 0)
		exitmain(1);
#	if has_kill
		kill(getpid(), SIGINT);
#	else
		raise(SIGINT);
#	endif
	exitmain(1);
}
EOF
	$PREPARE_CC || exit
	if ($CL a.c $L && $aout) >&2
	then has_psiginfo=1 ok=OK
	else has_psiginfo=0 ok=absent
	fi
	echo >&3 $ok
esac
echo "#define has_psiginfo $has_psiginfo /* Does psiginfo() work?  */"

case $has_signal in
1)
	$ech >&3 "$0: configuring has_psignal $dots"
	cat >a.c <<EOF
#include "$A_H"
int main() { psignal(SIGINT, ""); exitmain(0); }
EOF
	$PREPARE_CC || exit
	if ($CL a.c $L && $aout) >&2
	then has_psignal=1 ok=OK
	else has_psignal=0 ok=absent
	fi
	echo >&3 $ok;;
*)	has_psignal=0
esac
echo "#define has_psignal $has_psignal /* Does psignal() work?  */"

case $has_psiginfo in
1)
	$ech >&3 "$0: configuring has_si_errno $dots"
	cat >a.c <<EOF
#include "$A_H"
siginfo_t a;
int main() { exitmain(a.si_errno); }
EOF
	$PREPARE_CC || exit
	if $CS a.c $LS >&2 && $CS_OK
	then h=1 ok=OK
	else h=0 ok=absent
	fi
	echo >&3 $ok
	a= z=;;
*)	h=? a='/* ' z='*/ '
esac
echo "$a#define has_si_errno $h $z/* Does siginfo_t have si_errno?  */"

case $has_signal,$has_psignal in
1,0)
	$ech >&3 "$0: configuring has_sys_siglist $dots"
	cat >a.c <<EOF
#include "$A_H"
#if !defined(sys_siglist) && declare_sys_siglist
	extern char const * const sys_siglist[];
#endif
int main() { exitmain(!sys_siglist[1][0]); }
EOF
	$PREPARE_CC || exit
	h=0 ok=absent
	for d in 1 0
	do ($CL -Ddeclare_sys_siglist=$d a.c $L && $aout) >&2 &&
		h=1 && ok=OK && break
	done
	echo >&3 $ok
	a= z=;;
*)	h=? a='/* ' z='*/ '
esac
echo "$a#define has_sys_siglist $h $z/* Does sys_siglist[] work?  */"

$ech >&3 "$0: configuring strchr $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef strchr
	char *strchr();
#endif
int main() {exitmain(!strchr("abc", 'c'));}
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then a='/* ' z='*/ ' ok=OK
else a= z= ok='does not work'
fi
echo >&3 $ok
echo "$a#define strchr index $z/* Use old-fashioned name for strchr()?  */"

$ech >&3 "$0: configuring strrchr $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef strrchr
	char *strrchr();
#endif
int main() {exitmain(!strrchr("abc", 'c'));}
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then a='/* ' z='*/ ' ok=OK
else a= z= ok='does not work'
fi
echo >&3 $ok
echo "$a#define strrchr rindex $z/* Use old-fashioned name for strrchr()?  */"

$ech >&3 "$0: configuring bad_unlink $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(unlink("a.c") != 0); }
EOF
$PREPARE_CC && chmod -w a.c || exit
if ($CL a.c $L && $aout) >&2 && test ! -f a.c
then b=0 ok=OK
else b=1 ok='will work around bug'
fi
rm -f a.c || exit
echo >&3 $ok
echo "#define bad_unlink $b /* Does unlink() fail on unwritable files?  */"

$ech >&3 "$0: configuring has_vfork $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#if !TRY_VFORK
#	undef vfork
#	define vfork fork
#endif
#if !TRY_WAITPID
#	undef waitpid
#	define waitpid(p,s,o) wait(s)
#endif

int
main() {
	pid_t parent = getpid();
	pid_t child = vfork();

	if (child == 0) {
		/*
		 * On sparc systems, changes by the child to local and incoming
		 * argument registers are propagated back to the parent.
		 * The compiler is told about this with #include <vfork.h>,
		 * but some compilers (e.g. gcc -O) don't grok <vfork.h>.
		 * Test for this by using lots of local variables, at least
		 * as many local variables as 'main' has allocated so far
		 * including compiler temporaries.  4 locals are enough for
		 * gcc 1.40.3 on a sparc, but we use 8 to be safe.
		 * A buggy compiler should reuse the register of 'parent'
		 * for one of the local variables, since it will think that
		 * 'parent' can't possibly be used any more in this routine.
		 * Assigning to the local variable will thus munge 'parent'
		 * in the parent process.
		 */
		pid_t
			p = getpid(),
			p1 = getpid(), p2 = getpid(),
			p3 = getpid(), p4 = getpid(),
			p5 = getpid(), p6 = getpid(),
			p7 = getpid();
		/*
		 * Convince the compiler that p..p7 are live; otherwise, it might
		 * use the same hardware register for all 8 local variables.
		 */
		if (p!=p1 || p!=p2 || p!=p3 || p!=p4 || p!=p5 || p!=p6 || p!=p7)
			_exit(1);

		/*
		 * On some systems (e.g. IRIX 3.3),
		 * vfork doesn't separate parent from child file descriptors.
		 * If the child closes a descriptor before it execs or exits,
		 * this munges the parent's descriptor as well.
		 * Test for this by closing stdout in the child.
		 */
		_exit(close(STDOUT_FILENO) != 0);

	} else {
		int status;
		struct stat st;
		exit(
			/* Was there some problem with vforking?  */
			child < 0

			/* Was there some problem in waiting for the child?  */
			|| waitpid(child, &status, 0) != child

			/* Did the child fail?  (This shouldn't happen.)  */
			|| status

			/* Did the vfork/compiler bug occur?  */
			|| parent != getpid()

			/* Did the file descriptor bug occur?  */
			|| fstat(STDOUT_FILENO, &st) != 0
		);
	}
}
EOF
$PREPARE_CC || exit
if ($CL -DTRY_VFORK=1 a.c $L && $aout) >&2
then has_vfork=1 ok=OK
else has_vfork=0 ok='absent or broken'
fi
echo >&3 $ok
echo "#define has_vfork $has_vfork /* Does vfork() work?  */"
h=$has_vfork
case $h in
0)
	$ech >&3 "$0: configuring has_fork $dots"
	$PREPARE_CC || exit
	ok='does not work'
	($CL a.c $L && $aout) >&2 && h=1 ok=OK
	echo >&3 $ok
esac
echo "#define has_fork $h /* Does fork() work?  */"
$PREPARE_CC || exit
$ech >&3 "$0: configuring has_waitpid $dots"
if ($CL -DTRY_VFORK=$has_vfork -DTRY_WAITPID=1 a.c $L && $aout) >&2
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_spawn 0 /* Does spawn*() work?  */"
echo "#define has_waitpid $h /* Does waitpid() work?  */"

$ech >&3 "$0: configuring bad_wait_if_SIGCHLD_ignored $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif
int main() {
	signal(SIGCHLD, SIG_IGN);
	{
#	if has_fork
		int status;
		pid_t p = fork();
		if (p < 0) {
			perror("fork");
			exitmain(2);
		}
		if (p == 0)
			_exit(0);
		while (wait(&status) != p) {
			if (errno == ECHILD)
				exitmain(1);
			if (errno != EINTR) {
				perror("wait");
				exitmain(2);
			}
		}
#	else
#		if has_system
			if (system("true") != 0)
				exitmain(1);
#		endif
#	endif
	}
	exitmain(0);
}
EOF
$PREPARE_CC || exit
b=0 ok=OK
if $CL a.c $L >&2
then
	$aout >&2
	case $? in
	0) ;;
	1) b=1 ok='will work around bug';;
	*) exit
	esac
fi
rm -f a.c || exit
echo >&3 $ok
echo "#define bad_wait_if_SIGCHLD_ignored $b /* Does ignoring SIGCHLD break wait()?  */"


echo '#define RCS_SHELL "/bin/sh" /* shell to run RCS subprograms */'

$ech >&3 "$0: configuring has_printf_dot $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { printf("%.2d", 1); exitmain(ferror(stdout) || fclose(stdout)!=0); }
EOF
$PREPARE_CC && $CL a.c $L >&2 && r=`$aout` || exit
case $r in
01)	h=1 ok=OK;;
*)	h=0 ok='does not work'
esac
echo >&3 $ok
echo "#define has_printf_dot $h /* Does \"%.2d\" print leading 0?  */"

$ech >&3 "$0: configuring has_vfprintf, has_attribute_format_printf $dots"
cat >a.c <<EOF
#include "$A_H"
#if has_attribute_format_printf
#	define printf_string(m, n) __attribute__((format(printf, m, n)))
#else
#	define printf_string(m, n)
#endif
int p P((char const*,...)) printf_string(1, 2);
#if has_prototypes
int p(char const*format,...)
#else
/*VARARGS1*/ int p(format, va_alist) char *format; va_dcl
#endif
{
	int r;
	va_list args;
	vararg_start(args, format);
	r = vfprintf(stderr, format, args);
	va_end(args);
	return r;
}
int main() { exitmain(p("hello") != 5); }
EOF
$PREPARE_CC || exit
h=0 p=0
if ($CL a.c $L && sh -c 'pid=$$; (sleep 3; kill $pid)& exec '$aout) >&2
then
	h=1
	$PREPARE_CC || exit
	$CS -Dhas_attribute_format_printf=1 a.c >&2 && $CS_OK && p=1
else
	status=$?
	sh -c 'pid=$$; (sleep 3; kill $pid)& exec sleep 6' >&2
	if test $? = $status
	then
		echo >&3 "$0: stdio library loops forever.  Giving up.
$0: (Perhaps you are using Solaris 2.x /usr/ucb/cc?)
$0: Please use a working stdio library instead."
		exit 1
	fi
fi
echo >&3 $h, $p
cat <<EOF
#define has_vfprintf $h /* Does vfprintf() work?  */
#define has_attribute_format_printf $p /* Does __attribute__((format(printf,N,N+1))) work?  */
#if has_attribute_format_printf
#	define printf_string(m, n) __attribute__((format(printf, m, n)))
#else
#	define printf_string(m, n)
#endif
#if has_attribute_format_printf && has_attribute_noreturn
	/* Work around a bug in GCC 2.5.x.  */
#	define printf_string_exiting(m, n) __attribute__((format(printf, m, n), noreturn))
#else
#	define printf_string_exiting(m, n) printf_string(m, n) exiting
#endif
EOF

case $h in
1)
	h=? a='/* ' z='*/ ';;
*)
	$ech >&3 "$0: configuring has__doprintf $dots"
	a= z=
	cat >a.c <<EOF
#include "$A_H"
#if has_prototypes
static int
p(char const*format,...)
#else
/*VARARGS1*/ static int p(format, va_alist) char *format; va_dcl
#endif
{
	va_list args;
	vararg_start(args, format);
#	if TRY__DOPRINTF
		_doprintf(stderr, format, args);
#	else
		_doprnt(format, args, stderr);
#	endif
	va_end(args);
}
int main() { p(""); exitmain(0); }
EOF
	$PREPARE_CC || exit
	if ($CL -DTRY__DOPRINTF=1 a.c $L && $aout) >&2
	then h=1 ok=OK
	else h=0 ok='does not work'
	fi
	echo >&3 $ok
esac
echo "$a#define has__doprintf $h $z/* Does _doprintf() work?  */"
case $h in
0)
	$ech >&3 "$0: configuring has__doprnt $dots"
	$PREPARE_CC || exit
	if ($CL a.c $L && $aout) >&2
	then h=1 ok=OK
	else h=0 ok='does not work'
	fi
	echo >&3 $ok
	a= z=;;
*)
	h=? a='/* ' z='*/ '
esac
echo "$a#define has__doprnt $h $z/* Does _doprnt() work?  */"

$ech >&3 "$0: configuring EXIT_FAILURE $dots"
cat >a.c <<EOF
#include "$A_H"
int main() { exitmain(EXIT_FAILURE); }
EOF
$PREPARE_CC || exit
if $CL a.c $L >&2 && $aout
then a= z= ok='will work around bug'
else a='/* ' z='*/ ' ok=OK
fi
echo >&3 $ok
echo "$a#undef EXIT_FAILURE $z/* Uncomment this if EXIT_FAILURE is broken.  */"

: configuring large_memory
case "$has_map_fd$has_mmap" in
*1*) l=1;;
*) l=0
esac
echo "#define large_memory $l /* Can main memory hold entire RCS files?  */"

$ech >&3 "$0: configuring LONG_MAX $dots"
cat >a.c <<EOF
#include "$A_H"
static unsigned long ulong_max;
static long long_max;
int
main() {
	ulong_max--;
	long_max = ulong_max >> 1;
	printf("#ifndef LONG_MAX\n");
	printf("#define LONG_MAX %ldL /* long maximum */\n", long_max);
	printf("#endif\n");
	exitmain(ferror(stdout) || fclose(stdout)!=0);
}
EOF
$PREPARE_CC && $CL a.c $L >&2 && $aout || exit
echo >&3 OK

: configuring same_file
echo "/* Do struct stat s and t describe the same file?  Answer d if unknown.  */"
echo "#define same_file(s,t,d) ((s).st_ino==(t).st_ino && (s).st_dev==(t).st_dev)"

$ech >&3 "$0: configuring struct utimbuf $dots"
cat >a.c <<EOF
#include "$A_H"
static struct utimbuf s;
int main() { s.actime = s.modtime = 1; exitmain(utime("a.c", &s) != 0); }
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then h=1 ok=OK
else h=0 ok='does not work'
fi
echo >&3 $ok
echo "#define has_utimbuf $h /* Does struct utimbuf work?  */"

: configuring CO
echo "#define CO \"${RCSPREFIX}co\" /* name of 'co' program */"

: configuring COMPAT2
echo "#define COMPAT2 $COMPAT2 /* Are version 2 files supported?  */"

: configuring DIFF
echo "#define DIFF \"${DIFF}\" /* name of 'diff' program */"

: configuring DIFF3
echo "#define DIFF3 \"${DIFF3}\" /* name of 'diff3' program */"

: configuring DIFF3_BIN
echo "#define DIFF3_BIN $DIFF3_BIN /* Is diff3 user-visible (not the /usr/lib auxiliary)?  */"

: configuring DIFFFLAGS
echo "#define DIFFFLAGS \"$DIFFFLAGS\" /* Make diff output suitable for RCS.  */"

: configuring DIFF_L
echo "#define DIFF_L $DIFF_L /* Does diff -L work?  */"

: configuring DIFF_SUCCESS, DIFF_FAILURE, DIFF_TROUBLE
cat <<EOF
#define DIFF_SUCCESS $DIFF_SUCCESS /* DIFF status if no differences are found */
#define DIFF_FAILURE $DIFF_FAILURE /* DIFF status if differences are found */
#define DIFF_TROUBLE $DIFF_TROUBLE /* DIFF status if trouble */
EOF

: configuring ED
echo "#define ED \"${ED}\" /* name of 'ed' program (used only if !DIFF3_BIN) */"

: configuring MERGE
echo "#define MERGE \"${RCSPREFIX}merge\" /* name of 'merge' program */"

: configuring '*SLASH*', ROOTPATH, TMPDIR, X_DEFAULT
case ${PWD-`pwd`} in
/*) # Posix
	SLASH=/
	qSLASH="'/'"
	SLASHes=$qSLASH
	isSLASH='#define isSLASH(c) ((c) == SLASH)'
	ROOTPATH='isSLASH((p)[0])'
	X_DEFAULT=",v$SLASH";;
?:[/\\\\]*) # MS-DOS # \\\\ instead of \\ doesn't hurt, and avoids common bugs
	SLASH='\'
	qSLASH="'\\\\'"
	SLASHes="$qSLASH: case '/': case ':'"
	isSLASH='int isSLASH P((int));'
	ROOTPATH="(isSLASH((p)[0]) || (p)[0] && (p)[1]==':')"
	X_DEFAULT="$SLASH,v";;
*)
	echo >&3 $0: cannot deduce SLASH
	exit 1
esac
cat <<EOF
#define TMPDIR "${SLASH}tmp" /* default directory for temporary files */
#define SLASH $qSLASH /* principal filename separator */
#define SLASHes $SLASHes /* \`case SLASHes:' labels all filename separators */
$isSLASH /* Is arg a filename separator?  */
#define ROOTPATH(p) $ROOTPATH /* Is p an absolute pathname?  */
#define X_DEFAULT "$X_DEFAULT" /* default value for -x option */
EOF

$ech >&3 "$0: configuring SLASHSLASH_is_SLASH $dots"
cat >a.c <<EOF
#include "$A_H"
static struct stat s, ss;
static char f[3];
int
main() {
	f[0] = SLASH; if (stat(f, &s ) != 0) exitmain(1);
	f[1] = SLASH; if (stat(f, &ss) != 0) exitmain(1);
	exitmain(!same_file(s, ss, 0));
}
EOF
$PREPARE_CC || exit
if ($CL a.c $L && $aout) >&2
then eq=1 ok=OK
else eq=0 ok=no
fi
echo >&3 $ok
echo "#define SLASHSLASH_is_SLASH $eq /* Are // and / the same directory?  */"

$ech >&3 "$0: configuring ALL_ABSOLUTE, DIFF_ABSOLUTE $dots"
cat >a.c <<EOF
#include "$A_H"
#ifndef isSLASH
static int
isSLASH(c) int c; {
	switch (c) { case SLASHes: return 1; } return 0;
}
#endif
int
main(argc, argv) int argc; char **argv; {
	exitmain(1<argc && !ROOTPATH(argv[1]));
}
EOF
$PREPARE_CC && ($CL a.c $L && $aout) >&2 || exit
a=1
for i in "$DIFF" "$DIFF3" "$ED" "$RCSPREFIX" "$SENDMAIL"
do
	case $i in
	\"*\") i=`expr "$i" : '"\(.*\)"'`
	esac
	case $i in
	?*) $aout "$i" || { a=0; break; }
	esac
done
echo "#define ALL_ABSOLUTE $a /* Do all subprograms satisfy ROOTPATH?  */"
if $aout "$DIFF"
then a=1
else a=0
fi
echo "#define DIFF_ABSOLUTE $a /* Is ROOTPATH(DIFF) true?  */"
echo >&3 OK

: configuring SENDMAIL
case $SENDMAIL in
'') a='/* ' z='*/ ';;
*) a= z=
esac
echo "$a#define SENDMAIL $SENDMAIL $z/* how to send mail */"

: configuring TZ_must_be_set
echo "#define TZ_must_be_set 0 /* Must TZ be set for gmtime() to work?  */"


$ech >&3 "$0: configuring standard library declarations $dots"

cat <<'EOF'



/* Adjust the following declarations as needed.  */
EOF

cat >a.ha <<EOF


/* The rest is for the benefit of non-standard, traditional hosts.  */
/* Don't bother to declare functions that in traditional hosts do not appear, */
/* or are declared in .h files, or return int or void.  */


/* traditional BSD */

#if has_sys_siglist && !defined(sys_siglist)
	extern char const * const sys_siglist[];
#endif


/* Posix (ISO/IEC 9945-1: 1990 / IEEE Std 1003.1-1990) */

/* <fcntl.h> */
#ifdef O_CREAT
#	define open_can_creat 1
#else
#	define open_can_creat 0
#	define O_RDONLY 0
#	define O_WRONLY 1
#	define O_RDWR 2
#	define O_CREAT 01000
#	define O_TRUNC 02000
#endif
#ifndef O_EXCL
#define O_EXCL 0
#endif

/* <sys/stat.h> */
#ifndef S_IRUSR
#	ifdef S_IREAD
#		define S_IRUSR S_IREAD
#	else
#		define S_IRUSR 0400
#	endif
#	ifdef S_IWRITE
#		define S_IWUSR S_IWRITE
#	else
#		define S_IWUSR (S_IRUSR/2)
#	endif
#endif
#ifndef S_IRGRP
#	if has_getuid
#		define S_IRGRP (S_IRUSR / 0010)
#		define S_IWGRP (S_IWUSR / 0010)
#		define S_IROTH (S_IRUSR / 0100)
#		define S_IWOTH (S_IWUSR / 0100)
#	else
		/* single user OS -- not Posix or Unix */
#		define S_IRGRP 0
#		define S_IWGRP 0
#		define S_IROTH 0
#		define S_IWOTH 0
#	endif
#endif
#ifndef S_ISREG
#define S_ISREG(n) (((n) & S_IFMT) == S_IFREG)
#endif

/* <sys/wait.h> */
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#undef WIFEXITED /* Avoid 4.3BSD incompatibility with Posix.  */
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val)  &  0377) == 0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(stat_val) ((stat_val) & 0177)
#undef WIFSIGNALED /* Avoid 4.3BSD incompatibility with Posix.  */
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(stat_val) ((unsigned)(stat_val) - 1  <  0377)
#endif

/* <unistd.h> */
char *getlogin P((void));
#ifndef STDIN_FILENO
#	define STDIN_FILENO 0
#	define STDOUT_FILENO 1
#	define STDERR_FILENO 2
#endif
#if has_fork && !has_vfork
#	undef vfork
#	define vfork fork
#endif
#if has_getcwd || !has_getwd
	char *getcwd P((char*,size_t));
#else
	char *getwd P((char*));
#endif
#if has_setuid && !has_seteuid
#	undef seteuid
#	define seteuid setuid
#endif
#if has_spawn
#	if ALL_ABSOLUTE
#		define spawn_RCS spawnv
#	else
#		define spawn_RCS spawnvp
#	endif
#else
#	if ALL_ABSOLUTE
#		define exec_RCS execv
#	else
#		define exec_RCS execvp
#	endif
#endif

/* utime.h */
#if !has_utimbuf
	struct utimbuf { time_t actime, modtime; };
#endif


/* Standard C library */

/* <stdio.h> */
#ifndef L_tmpnam
#define L_tmpnam 32 /* power of 2 > sizeof("/usr/tmp/xxxxxxxxxxxxxxx") */
#endif
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#if has_mktemp
	char *mktemp P((char*)); /* traditional */
#else
	char *tmpnam P((char*));
#endif

/* <stdlib.h> */
char *getenv P((char const*));
void _exit P((int)) exiting;
void exit P((int)) exiting;
malloc_type malloc P((size_t));
malloc_type realloc P((malloc_type,size_t));
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

/* <string.h> */
char *strcpy P((char*,char const*));
char *strchr P((char const*,int));
char *strrchr P((char const*,int));
void *memcpy P((void*,void const*,size_t));
#if has_memmove
	void *memmove P((void*,void const*,size_t));
#endif

/* <time.h> */
time_t time P((time_t*));
EOF

cat >a.c <<EOF
#include "$A_H"
#define a 0
#define b 1
#if H==a
#	include "a.ha"
#else
#	include "a.hb"
#endif
int main() { exitmain(0); }
EOF

# Comment out lines in a.ha that the compiler rejects.
# a.ha may not contain comments that cross line boundaries.
# Leave the result in a.h$H.
H=a L=1
U=`wc -l <a.ha | sed 's| ||g'`
commentOut='s|^[^#/][^/]*|/* & */|'

until
	test $U -lt $L  ||
	{ $PREPARE_CC || exit;  $CS -DH=$H a.c $LS >&2 && $CS_OK; }
do
	case $H in
	a) I=b;;
	*) I=a
	esac

	# The compiler rejects some line in L..U.
	# Use binary search to set L to be the index of the first bad line in L..U.
	u=$U
	while test $L -lt $u
	do
		M=`expr '(' $L + $u ')' / 2`
		M1=`expr $M + 1`
		sed "$M1,\$$commentOut" a.h$H >a.h$I || exit
		$PREPARE_CC || exit
		if $CS -DH=$I a.c $LS >&2 && $CS_OK
		then L=$M1
		else u=$M
		fi
	done

	# Comment out the bad line.
	badline=`sed -n "$L{p;q;}" a.h$H`
	echo >&3 "$n$0: commenting out \`$badline' $dots"
	sed "$L$commentOut" a.h$H >a.h$I || exit

	H=$I
	L=`expr $L + 1`
done

cat a.h$H

echo >&3 OK
