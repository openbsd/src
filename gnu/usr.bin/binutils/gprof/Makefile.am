## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

SUFFIXES = .m

SUBDIRS = po

BASEDIR = $(srcdir)/..
BFDDIR = $(BASEDIR)/bfd
INCDIR	= $(BASEDIR)/include

WARN_CFLAGS = @WARN_CFLAGS@
AM_CFLAGS = $(WARN_CFLAGS)

MKDEP = gcc -MM

INCLUDES = -D_GNU_SOURCE -DDEBUG -I../bfd -I$(srcdir)/../include -I$(srcdir)/../bfd -I$(srcdir)/../intl -I../intl -I. -DLOCALEDIR="\"$(prefix)/share/locale\""

bin_PROGRAMS = gprof

## Convenience var listing pure sources.
sources = basic_blocks.c call_graph.c cg_arcs.c cg_dfn.c \
	cg_print.c corefile.c gmon_io.c gprof.c hertz.c hist.c source.c \
	search_list.c symtab.c sym_ids.c utils.c \
	i386.c alpha.c vax.c tahoe.c sparc.c
gprof_SOURCES = $(sources) flat_bl.c bsd_callg_bl.c fsf_callg_bl.c
gprof_DEPENDENCIES = ../bfd/libbfd.la ../libiberty/libiberty.a $(INTLDEPS)
gprof_LDADD = ../bfd/libbfd.la ../libiberty/libiberty.a $(INTLLIBS)

noinst_HEADERS = \
	basic_blocks.h call_graph.h cg_arcs.h cg_dfn.h cg_print.h \
	corefile.h gmon.h gmon_io.h gmon_out.h gprof.h hertz.h hist.h \
	search_list.h source.h sym_ids.h symtab.h utils.h

EXTRA_DIST = flat_bl.c bsd_callg_bl.c fsf_callg_bl.c bbconv.pl

BUILT_SOURCES = flat_bl.c bsd_callg_bl.c fsf_callg_bl.c

diststuff: $(BUILT_SOURCES) info

.m.c:
	awk -f $(srcdir)/gen-c-prog.awk > ./$*.c \
	    FUNCTION=`(echo $*|sed -e 's,.*/,,g' -e 's/_bl//')`_blurb \
	    FILE=$*.m $(srcdir)/$*.m

POTFILES = $(sources) $(noinst_HEADERS)
po/POTFILES.in: @MAINT@ Makefile
	for file in $(POTFILES); do echo $$file; done | sort > tmp \
	  && mv tmp $(srcdir)/po/POTFILES.in

info_TEXINFOS = gprof.texi
man_MANS = gprof.1

# Targets to rebuild dependencies in this Makefile.
# Have to get rid of DEP1 here so that "$?" later includes all sources.
DEP: dep.sed $(gprof_SOURCES) $(noinst_HEADERS) gconfig.h
	rm -f DEP1
	$(MAKE) MKDEP="$(MKDEP)" DEP1
	sed -f dep.sed < DEP1 > DEPA
	echo '# IF YOU PUT ANYTHING HERE IT WILL GO AWAY' >> DEPA
	if grep ' /' DEPA > /dev/null 2> /dev/null; then \
	  echo 'make DEP failed!'; exit 1; \
	else \
	  mv -f DEPA $@; \
	fi

DEP1: $(gprof_SOURCES)
	echo '# DO NOT DELETE THIS LINE -- mkdep uses it.' > DEP2
	echo '# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.' >> DEP2
	$(MKDEP) $(INCLUDES) $(CFLAGS) $? >> DEP2
	mv -f DEP2 $@

dep.sed: dep-in.sed config.status
	objdir=`pwd`; \
	sed <$(srcdir)/dep-in.sed >dep.sed	\
		-e 's!@INCDIR@!$(INCDIR)!'	\
		-e 's!@BFDDIR@!$(BFDDIR)!'	\
		-e 's!@SRCDIR@!$(srcdir)!'	\
		-e "s!@OBJDIR@!$${objdir}!"

dep: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < Makefile > tmp-Makefile
	cat DEP >> tmp-Makefile
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile Makefile

dep-in: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < $(srcdir)/Makefile.in > tmp-Makefile.in
	cat DEP >> tmp-Makefile.in
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile.in $(srcdir)/Makefile.in

dep-am: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < $(srcdir)/Makefile.am > tmp-Makefile.am
	cat DEP >> tmp-Makefile.am
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile.am $(srcdir)/Makefile.am

.PHONY: dep dep-in dep-am

CLEANFILES = dep.sed DEP DEPA DEP1 DEP2

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
basic_blocks.o: basic_blocks.c basic_blocks.h gprof.h \
  $(INCDIR)/ansidecl.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h gconfig.h \
  $(INCDIR)/bin-bugs.h source.h search_list.h symtab.h \
  ../bfd/bfd.h corefile.h gmon_io.h gmon.h gmon_out.h \
  $(INCDIR)/libiberty.h sym_ids.h
call_graph.o: call_graph.c cg_arcs.h gprof.h $(INCDIR)/ansidecl.h \
  $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h gconfig.h $(INCDIR)/bin-bugs.h \
  symtab.h ../bfd/bfd.h source.h search_list.h call_graph.h \
  corefile.h gmon_io.h gmon.h gmon_out.h sym_ids.h
cg_arcs.o: cg_arcs.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h gconfig.h $(INCDIR)/bin-bugs.h \
  call_graph.h symtab.h ../bfd/bfd.h source.h search_list.h \
  cg_arcs.h cg_dfn.h cg_print.h utils.h sym_ids.h
cg_dfn.o: cg_dfn.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h gconfig.h $(INCDIR)/bin-bugs.h \
  cg_arcs.h symtab.h ../bfd/bfd.h source.h search_list.h \
  cg_dfn.h utils.h
cg_print.o: cg_print.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  cg_arcs.h gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h gconfig.h \
  $(INCDIR)/bin-bugs.h symtab.h ../bfd/bfd.h source.h \
  search_list.h cg_print.h hist.h utils.h
corefile.o: corefile.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h gconfig.h $(INCDIR)/bin-bugs.h \
  corefile.h ../bfd/bfd.h symtab.h source.h search_list.h
gmon_io.o: gmon_io.c cg_arcs.h gprof.h $(INCDIR)/ansidecl.h \
  $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h gconfig.h $(INCDIR)/bin-bugs.h \
  symtab.h ../bfd/bfd.h source.h search_list.h basic_blocks.h \
  corefile.h call_graph.h gmon_io.h gmon.h gmon_out.h \
  hertz.h hist.h $(INCDIR)/libiberty.h
gprof.o: gprof.c $(INCDIR)/getopt.h $(INCDIR)/libiberty.h \
  $(INCDIR)/ansidecl.h gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h gconfig.h \
  $(INCDIR)/bin-bugs.h basic_blocks.h source.h search_list.h \
  symtab.h ../bfd/bfd.h call_graph.h cg_arcs.h cg_print.h \
  corefile.h gmon_io.h gmon.h hertz.h hist.h sym_ids.h \
  $(INCDIR)/demangle.h
hertz.o: hertz.c hertz.h gprof.h $(INCDIR)/ansidecl.h \
  $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h gconfig.h $(INCDIR)/bin-bugs.h
hist.o: hist.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h gconfig.h $(INCDIR)/bin-bugs.h \
  corefile.h ../bfd/bfd.h gmon_io.h gmon.h gmon_out.h \
  hist.h symtab.h source.h search_list.h sym_ids.h utils.h
source.o: source.c gprof.h $(INCDIR)/ansidecl.h $(BFDDIR)/sysdep.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  gconfig.h $(INCDIR)/bin-bugs.h $(INCDIR)/libiberty.h \
  search_list.h source.h
search_list.o: search_list.c $(INCDIR)/libiberty.h \
  $(INCDIR)/ansidecl.h gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h gconfig.h \
  $(INCDIR)/bin-bugs.h search_list.h
symtab.o: symtab.c gprof.h $(INCDIR)/ansidecl.h $(BFDDIR)/sysdep.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  gconfig.h $(INCDIR)/bin-bugs.h cg_arcs.h symtab.h ../bfd/bfd.h \
  source.h search_list.h corefile.h
sym_ids.o: sym_ids.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  cg_arcs.h gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h gconfig.h \
  $(INCDIR)/bin-bugs.h symtab.h ../bfd/bfd.h source.h \
  search_list.h sym_ids.h
utils.o: utils.c gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h gconfig.h \
  $(INCDIR)/bin-bugs.h cg_arcs.h symtab.h ../bfd/bfd.h \
  source.h search_list.h
i386.o: i386.c gprof.h $(INCDIR)/ansidecl.h $(BFDDIR)/sysdep.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  gconfig.h $(INCDIR)/bin-bugs.h cg_arcs.h symtab.h ../bfd/bfd.h \
  source.h search_list.h corefile.h hist.h
alpha.o: alpha.c gprof.h $(INCDIR)/ansidecl.h $(BFDDIR)/sysdep.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  gconfig.h $(INCDIR)/bin-bugs.h cg_arcs.h symtab.h ../bfd/bfd.h \
  source.h search_list.h corefile.h hist.h
vax.o: vax.c gprof.h $(INCDIR)/ansidecl.h $(BFDDIR)/sysdep.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  gconfig.h $(INCDIR)/bin-bugs.h cg_arcs.h symtab.h ../bfd/bfd.h \
  source.h search_list.h corefile.h hist.h
tahoe.o: tahoe.c gprof.h $(INCDIR)/ansidecl.h $(BFDDIR)/sysdep.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  gconfig.h $(INCDIR)/bin-bugs.h cg_arcs.h symtab.h ../bfd/bfd.h \
  source.h search_list.h corefile.h hist.h
sparc.o: sparc.c gprof.h $(INCDIR)/ansidecl.h $(BFDDIR)/sysdep.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  gconfig.h $(INCDIR)/bin-bugs.h cg_arcs.h symtab.h ../bfd/bfd.h \
  source.h search_list.h corefile.h hist.h
flat_bl.o: flat_bl.c
bsd_callg_bl.o: bsd_callg_bl.c
fsf_callg_bl.o: fsf_callg_bl.c
# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
