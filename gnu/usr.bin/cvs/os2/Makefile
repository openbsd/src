# Generated automatically from Makefile.in by configure.
# Makefile for OS/2.  Generated from Makefile.in when CVS is
# configured for a dist-build.  Configure never gets run on OS/2, so
# we must include the generated Makefile in the distribution.  See the
# `dist-dir' rule.

project: cvs.exe
# .SUFFIXES .c .obj .exe

# path slash.  This was introduced so that it can be set to \
# instead of / for linking, but now that the cvs.exe rule uses
# subst to produce \, it should be possible to nuke SL.
SL = /

# Directory in which to install executables.
install_dir = s:${SL}gnu${SL}util

# srcdir is usually "."
srcdir     = .

# top_srcdir is usually ".."
top_srcdir = ..

lib_dir     = ..${SL}lib
cvs_srcdir = ..${SL}src

zlib_dir = ${top_srcdir}/zlib

# Do we need these?
# prefix = /usr/local
# exec_prefix = ${prefix}

# taken straight from the example project
LIB = s:${SL}ibmcpp${SL}lib;s:${SL}toolkt21${SL}os2lib;
CINC = -Is:${SL}ibmcpp${SL}include -Is:${SL}toolkt21${SL}c${SL}os2h
# This says we are building an object file, not a full executable.
OBJ_CFLAGS = -C+
CFLAGS = -W3 -Wrea+tru+ -Ti+ -Ss+ -Gd- -Gm+ -G4 -Q+ -Sm ${CINC} \
  -I${srcdir} -I${lib_dir} -I${cvs_srcdir} -I${zlib_dir} \
  -DIBM_CPP -DHAVE_CONFIG_H \
  -DTCPIP_IBM -Is:${SL}ibmtcpip${SL}include

# needed to make command-line wildcard expansion work right.  That's
# right, DOS-descendants handle this from the program, not the shell.
ARGVLIB = s:${SL}ibmcpp${SL}lib${SL}setargv.obj

# TCP/IP stuff
TCPIPLIB = s:${SL}ibmtcpip${SL}lib${SL}tcp32dll.lib	s:${SL}ibmtcpip${SL}lib${SL}so32dll.lib


# headers specific to OS/2

# We list OS2_HEADERS so we know what to include when we make dist-dir
# here.
OS2_HEADERS = \
       ${srcdir}${SL}tcpip.h \
       ${srcdir}${SL}config.h \
       ${srcdir}${SL}options.h \
       ${srcdir}${SL}pwd.h \
       ${srcdir}${SL}dirent.h \
       ${srcdir}${SL}rcmd.h \
       ${srcdir}${SL}popen.h

# headers we use from the common src dir, ..${SL}src
COMMON_HEADERS = \
       ${cvs_srcdir}${SL}client.h \
       ${cvs_srcdir}${SL}cvs.h \
       ${cvs_srcdir}${SL}rcs.h \
       ${cvs_srcdir}${SL}hash.h \
       ${cvs_srcdir}${SL}myndbm.h \
       ${cvs_srcdir}${SL}patchlevel.h \
       ${cvs_srcdir}${SL}update.h \
       ${cvs_srcdir}${SL}server.h \
       ${cvs_srcdir}${SL}error.h

# sources specific to OS/2
OS2_SOURCES = \
       ${srcdir}${SL}mkdir.c \
       ${srcdir}${SL}pwd.c \
       ${srcdir}${SL}filesubr.c \
       ${srcdir}${SL}dirent.c \
       ${srcdir}${SL}run.c \
       ${srcdir}${SL}stripslash.c \
       ${srcdir}${SL}rcmd.c \
       ${srcdir}${SL}waitpid.c \
       ${srcdir}${SL}popen.c \
       ${srcdir}${SL}porttcp.c \
       ${srcdir}${SL}getdate.c \
       ${srcdir}${SL}getpass.c

# sources we use from the common src dir, ..${SL}src
# FIXME: Is this used anywhere?  I don't think it is.
COMMON_SOURCES = \
       ${cvs_srcdir}${SL}add.c \
       ${cvs_srcdir}${SL}admin.c \
       ${cvs_srcdir}${SL}buffer.c \
       ${cvs_srcdir}${SL}checkin.c \
       ${cvs_srcdir}${SL}checkout.c \
       ${cvs_srcdir}${SL}classify.c \
       ${cvs_srcdir}${SL}client.c \
       ${cvs_srcdir}${SL}commit.c \
       ${cvs_srcdir}${SL}create_adm.c \
       ${cvs_srcdir}${SL}cvsrc.c \
       ${cvs_srcdir}${SL}diff.c \
       ${cvs_srcdir}${SL}edit.c \
       ${cvs_srcdir}${SL}entries.c \
       ${cvs_srcdir}${SL}error.c \
       ${cvs_srcdir}${SL}expand_path.c \
       ${cvs_srcdir}${SL}fileattr.c \
       ${cvs_srcdir}${SL}find_names.c \
       ${cvs_srcdir}${SL}hash.c \
       ${cvs_srcdir}${SL}history.c \
       ${cvs_srcdir}${SL}ignore.c \
       ${cvs_srcdir}${SL}import.c \
       ${cvs_srcdir}${SL}lock.c \
       ${cvs_srcdir}${SL}log.c \
       ${cvs_srcdir}${SL}login.c \
       ${cvs_srcdir}${SL}logmsg.c \
       ${cvs_srcdir}${SL}main.c \
       ${cvs_srcdir}${SL}mkmodules.c \
       ${cvs_srcdir}${SL}modules.c \
       ${cvs_srcdir}${SL}myndbm.c \
       ${cvs_srcdir}${SL}no_diff.c \
       ${cvs_srcdir}${SL}parseinfo.c \
       ${cvs_srcdir}${SL}patch.c \
       ${cvs_srcdir}${SL}rcs.c \
       ${cvs_srcdir}${SL}rcscmds.c \
       ${cvs_srcdir}${SL}recurse.c \
       ${cvs_srcdir}${SL}release.c \
       ${cvs_srcdir}${SL}remove.c \
       ${cvs_srcdir}${SL}repos.c \
       ${cvs_srcdir}${SL}root.c \
       ${cvs_srcdir}${SL}rtag.c \
       ${cvs_srcdir}${SL}scramble.c \
       ${cvs_srcdir}${SL}server.c \
       ${cvs_srcdir}${SL}status.c \
       ${cvs_srcdir}${SL}subr.c \
       ${cvs_srcdir}${SL}tag.c \
       ${cvs_srcdir}${SL}update.c \
       ${cvs_srcdir}${SL}watch.c \
       ${cvs_srcdir}${SL}wrapper.c \
       ${cvs_srcdir}${SL}vers_ts.c \
       ${cvs_srcdir}${SL}version.c
# end of $COMMON_SOURCES

# sources from ..${SL}lib
# FIXME: Is this used anywhere?  I don't think it is.
LIB_SOURCES = \
       ${lib_dir}${SL}getopt.c \
       ${lib_dir}${SL}getopt1.c \
       ${lib_dir}${SL}getline.c \
       ${lib_dir}${SL}getwd.c \
       ${lib_dir}${SL}savecwd.c \
       ${lib_dir}${SL}sighandle.c \
       ${lib_dir}${SL}yesno.c \
       ${lib_dir}${SL}vasprintf.c \
       ${lib_dir}${SL}xgetwd.c \
       ${lib_dir}${SL}md5.c \
       ${lib_dir}${SL}fnmatch.c \
       ${lib_dir}${SL}regex.c

# object files from OS/2 sources
OS2_OBJECTS = \
       ${srcdir}${SL}mkdir.obj \
       ${srcdir}${SL}pwd.obj \
       ${srcdir}${SL}filesubr.obj \
       ${srcdir}${SL}dirent.obj \
       ${srcdir}${SL}run.obj \
       ${srcdir}${SL}stripslash.obj \
       ${srcdir}${SL}rcmd.obj \
       ${srcdir}${SL}waitpid.obj \
       ${srcdir}${SL}popen.obj \
       ${srcdir}${SL}porttcp.obj \
       ${srcdir}${SL}getdate.obj \
       ${srcdir}${SL}getpass.obj

# object files from ..${SL}src
COMMON_OBJECTS = \
       ${cvs_srcdir}${SL}add.obj \
       ${cvs_srcdir}${SL}admin.obj \
       ${cvs_srcdir}${SL}buffer.obj \
       ${cvs_srcdir}${SL}checkin.obj \
       ${cvs_srcdir}${SL}checkout.obj \
       ${cvs_srcdir}${SL}classify.obj \
       ${cvs_srcdir}${SL}client.obj \
       ${cvs_srcdir}${SL}commit.obj \
       ${cvs_srcdir}${SL}create_adm.obj \
       ${cvs_srcdir}${SL}cvsrc.obj \
       ${cvs_srcdir}${SL}diff.obj \
       ${cvs_srcdir}${SL}edit.obj \
       ${cvs_srcdir}${SL}entries.obj \
       ${cvs_srcdir}${SL}error.obj \
       ${cvs_srcdir}${SL}expand_path.obj \
       ${cvs_srcdir}${SL}fileattr.obj \
       ${cvs_srcdir}${SL}find_names.obj \
       ${cvs_srcdir}${SL}hash.obj \
       ${cvs_srcdir}${SL}history.obj \
       ${cvs_srcdir}${SL}ignore.obj \
       ${cvs_srcdir}${SL}import.obj \
       ${cvs_srcdir}${SL}lock.obj \
       ${cvs_srcdir}${SL}log.obj \
       ${cvs_srcdir}${SL}login.obj \
       ${cvs_srcdir}${SL}logmsg.obj \
       ${cvs_srcdir}${SL}main.obj \
       ${cvs_srcdir}${SL}mkmodules.obj \
       ${cvs_srcdir}${SL}modules.obj \
       ${cvs_srcdir}${SL}myndbm.obj \
       ${cvs_srcdir}${SL}no_diff.obj \
       ${cvs_srcdir}${SL}parseinfo.obj \
       ${cvs_srcdir}${SL}patch.obj \
       ${cvs_srcdir}${SL}rcs.obj \
       ${cvs_srcdir}${SL}rcscmds.obj \
       ${cvs_srcdir}${SL}recurse.obj \
       ${cvs_srcdir}${SL}release.obj \
       ${cvs_srcdir}${SL}remove.obj \
       ${cvs_srcdir}${SL}repos.obj \
       ${cvs_srcdir}${SL}root.obj \
       ${cvs_srcdir}${SL}rtag.obj \
       ${cvs_srcdir}${SL}scramble.obj \
       ${cvs_srcdir}${SL}server.obj \
       ${cvs_srcdir}${SL}status.obj \
       ${cvs_srcdir}${SL}subr.obj \
       ${cvs_srcdir}${SL}tag.obj \
       ${cvs_srcdir}${SL}update.obj \
       ${cvs_srcdir}${SL}watch.obj \
       ${cvs_srcdir}${SL}wrapper.obj \
       ${cvs_srcdir}${SL}vers_ts.obj \
       ${cvs_srcdir}${SL}version.obj \
       ${cvs_srcdir}/zlib.obj
# end of $COMMON_OBJECTS

# objects from ..${SL}lib
LIB_OBJECTS = \
       ${lib_dir}${SL}getopt.obj \
       ${lib_dir}${SL}getopt1.obj \
       ${lib_dir}${SL}getline.obj \
       ${lib_dir}${SL}getwd.obj \
       ${lib_dir}${SL}savecwd.obj \
       ${lib_dir}${SL}sighandle.obj \
       ${lib_dir}${SL}yesno.obj \
       ${lib_dir}${SL}vasprintf.obj \
       ${lib_dir}${SL}xgetwd.obj \
       ${lib_dir}${SL}md5.obj \
       ${lib_dir}${SL}fnmatch.obj \
       ${lib_dir}${SL}regex.obj \
       ${lib_dir}/valloc.obj

ZLIB_OBJECTS = ${zlib_dir}/adler32.obj \
       ${zlib_dir}/compress.obj \
       ${zlib_dir}/crc32.obj \
       ${zlib_dir}/uncompr.obj \
       ${zlib_dir}/deflate.obj \
       ${zlib_dir}/trees.obj \
       ${zlib_dir}/zutil.obj \
       ${zlib_dir}/inflate.obj \
       ${zlib_dir}/infblock.obj \
       ${zlib_dir}/inftrees.obj \
       ${zlib_dir}/infcodes.obj \
       ${zlib_dir}/infutil.obj \
       ${zlib_dir}/inffast.obj

SOURCES = ${COMMON_SOURCES} ${LIB_SOURCES} ${OS2_SOURCES}
HEADERS = ${COMMON_HEADERS} ${OS2_HEADERS}
OBJECTS = ${COMMON_OBJECTS} ${LIB_OBJECTS} ${OS2_OBJECTS} ${ZLIB_OBJECTS}

DISTFILES = ${OS2_HEADERS} ${OS2_SOURCES} \
  ${srcdir}${SL}README ${srcdir}${SL}ChangeLog \
  ${srcdir}${SL}Makefile.in ${srcdir}${SL}.cvsignore \
  ${srcdir}${SL}test-makefile Makefile ${srcdir}/os2inc.h \
  ${srcdir}/watcom.mak

all:

.PHONY: all install uninstall
all install uninstall:

installdirs:
.PHONY: installdirs

.PHONY: tags TAGS
tags TAGS:

.PHONY: ls
ls:
	@echo ${DISTFILES}

.PHONY: clean distclean realclean mostlyclean
clean distclean realclean mostlyclean:

.PHONY: lint
lint:

.PHONY: dist-dir
dist-dir:
	mkdir ${DISTDIR}
	for i in ${DISTFILES}; do \
	  ln $${i} ${DISTDIR}; \
	done

# We don't have a real distclean or anything like that, but at least
# we can get rid of the obj files and cvs.exe. 
clean:
	rm -f cvs.exe
	rm -f ${srcdir}${SL}*.obj 
	rm -f ${lib_dir}${SL}*.obj 
	rm -f ${cvs_srcdir}${SL}*.obj 

install-cvs: cvs.exe
	cp ${srcdir}${SL}cvs.exe ${install_dir}${SL}cvs.exe

%.obj: %.c
	icc ${OBJ_CFLAGS} ${CFLAGS} /Fo$@ $*.c

# There seems to be no ICC option for specifying library locations, so
# we must `set' the path in the compilation environment.  Urgkle. 
#
#         I'm having some weird problem with "\" vs. "/".  If I build
# with "\" as the path separator in the makefile, the compiler dumps
# core.  Go figure.  If I build with "/" as the path separator, the
# object files compile fine but the linker thinks the "/" is indicating
# options and gets all confused (though at least it doesn't dump core).
# So, use subst to compile with "/" and link with "\".
cvs.exe: ${OBJECTS}
	echo Creating icc.in...
	echo -Q+ -Ti+ -Fe$@ -B"/batch" -B"/NOE" > icc.in
	echo $(subst /,\,$(OS2_OBJECTS))       >> icc.in
	echo $(subst /,\,$(LIB_OBJECTS))       >> icc.in
	echo $(subst /,\,$(COMMON_OBJECTS))    >> icc.in
	echo $(subst /,\,$(ZLIB_OBJECTS))      >> icc.in
	echo $(subst /,\,$(TCPIPLIB))          >> icc.in
	echo $(subst /,\,$(ARGVLIB))           >> icc.in
	echo Creating icc.in... done.
	set LIB=$(subst /,\,$(LIB)) & icc @icc.in

# cvs.obj: ${OBJECTS} ${SOURCES} ${HEADERS}

subdir = os2
# Hmm.  This rule wants to exist on unix, so that "make dist" works.
# And it doesn't want to exist on OS/2, because configure doesn't
# run on OS/2 and so ../config.status won't exist.  For now we just
# say, in emx/README, that people will need to comment it out.
Makefile: ../config.status $(srcdir)/Makefile.in
	cd .. && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= ./config.status
