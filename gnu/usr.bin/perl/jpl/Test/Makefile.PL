#!/usr/bin/perl

$JPL_HOME = $ENV{JPL_HOME}
    or die "You have not run setvars to set your environment variables.\n" .
           "See the JPL README file for more information.\n";

use Config;

eval `$JPL_HOME/setvars -perl`;

chop($WHAT = `pwd`);
$WHAT =~ s#.*/##;

if ($^O eq 'linux') {
    $flags = "-Dbool=char";	# avoid builtin bool altogether
    $libs = "-lc -lm -ldl";
}
else {
    $flags = "";
    $libs = "-lc -lm -ldl";
}
chop($cwd = `pwd`);
($jpldir = $cwd) =~ s#/[^/]*$##;

open(MAKEFILE, ">Makefile");

print MAKEFILE <<"SUBS";
CC = $Config{cc}
WHAT = $WHAT
PERL = perl$]
ARCHNAME = $Config{archname}
JAVA_HOME = $ENV{JAVA_HOME}
JPL_HOME = $ENV{JPL_HOME}
PERLARCHDIR = $Config{archlib}
FLAGS = $Config{cccdlflags} $Config{ccdlflags} $Config{lddlflags} $flags
INCL  = -I\$(PERLARCHDIR)/CORE \\
	-I\$(JAVA_HOME)/include \\
	-I\$(JAVA_HOME)/include/$^O \\
	-I\$(JAVA_HOME)/include/genunix
LIBS = $libs

SUBS

print MAKEFILE <<'NOSUBS';
.SUFFIXES: .jpl .class

.jpl.class:
	$(PERL) -MJPL::Compile -e "file('$*.jpl')"

all: $(WHAT).class lib$(WHAT).so

debug: $(WHAT)_g.class lib$(WHAT)_g.so

lib$(WHAT).so: $(WHAT).c $(JPL_HOME)/lib/$(ARCHNAME)/libPerlInterpreter.so
	$(CC) $(FLAGS) $(INCL) $(WHAT).c \
	$(PERLARCHDIR)/CORE/libperl.so \
	$(JPL_HOME)/lib/$(ARCHNAME)/libPerlInterpreter.so \
	$(LIBS) \
	-o lib$(WHAT).so

lib$(WHAT)_g.so: $(WHAT).c $(JPL_HOME)/lib/$(ARCHNAME)/libPerlInterpreter_g.so
	$(CC) -g $(FLAGS) $(INCL) $(WHAT).c \
	$(PERLARCHDIR)/CORE/libperl.so \
	$(JPL_HOME)/lib/$(ARCHNAME)/libPerlInterpreter_g.so \
	$(LIBS) \
	-o lib$(WHAT)_g.so

test:

install: all
	cp $(WHAT).class	$(JPL_HOME)/lib
	cp lib$(WHAT).so	$(JPL_HOME)/lib/$(ARCHNAME)
	cp $(WHAT).pl		$(JPL_HOME)/perl

clean:
	rm -f $(WHAT).c $(WHAT).h \
	$(WHAT)*.class $(WHAT)*.pl lib$(WHAT).so $(WHAT)*.java

distclean: clean
	rm -f Makefile

NOSUBS

close MAKEFILE;
