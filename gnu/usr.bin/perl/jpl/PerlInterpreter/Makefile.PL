#!/usr/bin/perl

$JPL_SRC = "..";

use Config;

eval `$JPL_SRC/setvars -perl`;

if ($^O eq 'linux') {
    $flags = "-Dbool=char";	# avoid builtin bool altogether
    $libs = "-lc -lm -ldl";
} else {
    $flags = "";
    $libs = "-lc -lm -ldl";
}

# Needed for JNI.
if ($^O eq 'solaris') {
    $libs .= " -lthread";
}

open(MAKEFILE, ">Makefile");

print MAKEFILE <<"SUBS";
WHAT = PerlInterpreter
JAVA_HOME = $ENV{JAVA_HOME}
JPL_HOME = $ENV{JPL_HOME}
ARCHNAME = $Config{archname}
PERLARCHDIR = $Config{archlib}
CC = $Config{cc}
FLAGS = $Config{cccdlflags} $Config{ccdlflags} $Config{lddlflags} $flags
INCL  = -I\$(PERLARCHDIR)/CORE \\
	-I\$(JAVA_HOME)/include \\
	-I\$(JAVA_HOME)/include/$^O \\
	-I\$(JAVA_HOME)/include/genunix
LIBS = $libs

SUBS


print MAKEFILE <<'NOSUBS';
.SUFFIXES: .java .class

.java.class:
	javac $*.java

.class.h:
	javah -jni $*

all: PerlInterpreter.class libPerlInterpreter.so

PerlInterpreter.class: PerlInterpreter.java

PerlInterpreter.h: PerlInterpreter.class

libPerlInterpreter.so: PerlInterpreter.c PerlInterpreter.h
	$(CC) $(FLAGS) $(INCL) PerlInterpreter.c \
	$(PERLARCHDIR)/auto/DynaLoader/DynaLoader.a \
	$(LIBS) \
	-o libPerlInterpreter.so

test:

install: all
	mkdir -p $(JPL_HOME)/lib/$(ARCHNAME)
	cp libPerlInterpreter.so $(JPL_HOME)/lib/$(ARCHNAME)
	cp $(WHAT).class $(JPL_HOME)/lib

clean:
	rm -f libPerlInterpreter.so
	rm -f PerlInterpreter.class
NOSUBS

close MAKEFILE;
