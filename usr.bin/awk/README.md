# OpenBSD Awk

This is a fork of The One True Awk, as shipped with OpenBSD.  It
includes changes not present in the upstream version because they
are OpenBSD-specific, are still open PRs, or were rejected by the
upstream maintainer.  This version of `awk` relies on APIs that are
not present in some other systems, such as `asprintf`, `pledge`,
`reallocarray`, `srandom_deterministic` and `strlcpy`.

## What is upstream? ##

Upstream is the bsd-features branch of https://github.com/onetrueawk/awk.

This is the version of `awk` described in _The AWK Programming Language_,
Second Edition, by Al Aho, Brian Kernighan, and Peter Weinberger
(Addison-Wesley, 2024, ISBN-13 978-0138269722, ISBN-10 0138269726).

## What's New? ##

This version of Awk handles UTF-8 and comma-separated values (CSV) input.

### Strings ###

Functions that process strings now count Unicode code points, not bytes;
this affects `length`, `substr`, `index`, `match`, `split`,
`sub`, `gsub`, and others.  Note that code
points are not necessarily characters.

UTF-8 sequences may appear in literal strings and regular expressions.
Aribtrary characters may be included with `\u` followed by 1 to 8 hexadecimal digits.

### Regular expressions ###

Regular expressions may include UTF-8 code points, including `\u`.
Character classes are likely to be limited to about 256 characters
when expanded.

### CSV ###

The option `--csv` turns on CSV processing of input:
fields are separated by commas, fields may be quoted with
double-quote (`"`) characters, quoted fields may contain embedded newlines.
In CSV mode, `FS` is ignored.

If no explicit separator argument is provided,
field-splitting in `split` is determined by CSV mode.

## Copyright

Copyright (C) Lucent Technologies 1997<br/>
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name Lucent Technologies or any of
its entities not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

## Distribution and Reporting Problems

Changes, mostly bug fixes and occasional enhancements, are listed
in `FIXES`.  If you distribute this code further, please please please
distribute `FIXES` with it.

If you find errors, please report them to bugs@openbsd.org rather
than the upstream maintainer unless you can also reproduce the
problem with an unmodified version of the upstream awk.

## Submitting Patches

Patches may be submitted to the tech@openbsd.org mailing list, or
bugs@openbsd.org if you are fixing a bug.

## Building

The program itself is created by

	make

which should produce a sequence of messages roughly like this:

	bison -d  awkgram.y
	awkgram.y: warning: 44 shift/reduce conflicts [-Wconflicts-sr]
	awkgram.y: warning: 85 reduce/reduce conflicts [-Wconflicts-rr]
	awkgram.y: note: rerun with option '-Wcounterexamples' to generate conflict counterexamples
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o awkgram.tab.o awkgram.tab.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o b.o b.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o main.o main.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o parse.o parse.c
	gcc -g -Wall -pedantic -Wcast-qual -O2 maketab.c -o maketab
	./maketab awkgram.tab.h >proctab.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o proctab.o proctab.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o tran.o tran.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o lib.o lib.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o run.o run.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2   -c -o lex.o lex.c
	gcc -g -Wall -pedantic -Wcast-qual   -O2 awkgram.tab.o b.o main.o parse.o proctab.o tran.o lib.o run.o lex.o   -lm

This produces an executable `a.out`; you will eventually want to
move this to some place like `/usr/bin/awk`.

If your system does not have `yacc` or `bison` (the GNU
equivalent), you need to install one of them first.

NOTE: This version uses ISO/IEC C99, as you should also.  We have
compiled this without any changes using `gcc -Wall` and/or local C
compilers on a variety of systems, but new systems or compilers
may raise some new complaint; reports of difficulties are
welcome.

This compiles without change on Macintosh OS X using `gcc` and
the standard developer tools.

You can also use `make CC=g++` to build with the GNU C++ compiler,
should you choose to do so.

## A Note About Releases

We don't usually do releases.

#### Last Updated

Mon 30 Oct 2023 12:53:07 MDT
