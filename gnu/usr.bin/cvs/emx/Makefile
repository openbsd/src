# Generated automatically from Makefile.in by configure.
# Makefile for OS/2.  Generated from Makefile.in when CVS is
# configured for a dist-build.  Configure never gets run on OS/2, so
# we must include the generated Makefile in the distribution.  See the
# `dist-dir' rule.

project: cvs.exe
# .SUFFIXES .c .o .exe

# Directory in which to install executables.
install_dir = s:/gnu/util

# srcdir is usually "."
srcdir     = .

# top_srcdir is usually ".."
top_srcdir = ..

lib_dir     = ${top_srcdir}/lib
cvs_srcdir = ${top_srcdir}/src
diff_srcdir = ${top_srcdir}/diff
zlib_dir = ${top_srcdir}/zlib

# Do we need these?
# prefix = /usr/local
# exec_prefix = ${prefix}

# Used to say -lufc, but that would only be needed for crypt().
LIB = -lsocket

CINC =
# This says we are building an object file, not a full executable.
OBJ_CFLAGS = -c
CFLAGS = -pedantic -Wall -fno-builtin ${CINC} \
  -I${srcdir} -I${lib_dir} -I${cvs_srcdir} -I${zlib_dir} \
  -DUNIX -DHAVE_CONFIG_H


# headers specific to OS/2

# We list OS2_HEADERS so we know what to include when we make dist-dir
# here.
OS2_HEADERS = \
       ${srcdir}/config.h \
       ${srcdir}/options.h \
       ${srcdir}/rcmd.h

# headers we use from the common src dir, ../src
COMMON_HEADERS = \
       ${cvs_srcdir}/client.h \
       ${cvs_srcdir}/cvs.h \
       ${cvs_srcdir}/rcs.h \
       ${cvs_srcdir}/hash.h \
       ${cvs_srcdir}/myndbm.h \
       ${cvs_srcdir}/patchlevel.h \
       ${cvs_srcdir}/update.h \
       ${cvs_srcdir}/server.h \
       ${cvs_srcdir}/error.h

# headers in ../diff
DIFF_HEADERS = \
       ${diff_srcdir}/cmpbuf.h \
	   ${diff_srcdir}/diff.h \
	   ${diff_srcdir}/system.h 

# sources specific to OS/2
OS2_SOURCES = \
       ${srcdir}/filesubr.c \
       ${srcdir}/startserver.c \
       ${srcdir}/stripslash.c \
       ${srcdir}/savecwd.c \
       ${srcdir}/system.c

# sources we use from the common src dir, ../src
# FIXME: Is this used anywhere?  I don't think it is.
COMMON_SOURCES = \
       ${cvs_srcdir}/add.c \
       ${cvs_srcdir}/admin.c \
       ${cvs_srcdir}/buffer.c \
       ${cvs_srcdir}/checkin.c \
       ${cvs_srcdir}/checkout.c \
       ${cvs_srcdir}/classify.c \
       ${cvs_srcdir}/client.c \
       ${cvs_srcdir}/commit.c \
       ${cvs_srcdir}/create_adm.c \
       ${cvs_srcdir}/cvsrc.c \
       ${cvs_srcdir}/diff.c \
       ${cvs_srcdir}/edit.c \
       ${cvs_srcdir}/entries.c \
       ${cvs_srcdir}/error.c \
       ${cvs_srcdir}/expand_path.c \
       ${cvs_srcdir}/fileattr.c \
       ${cvs_srcdir}/find_names.c \
       ${cvs_srcdir}/hash.c \
       ${cvs_srcdir}/history.c \
       ${cvs_srcdir}/ignore.c \
       ${cvs_srcdir}/import.c \
       ${cvs_srcdir}/lock.c \
       ${cvs_srcdir}/log.c \
       ${cvs_srcdir}/login.c \
       ${cvs_srcdir}/logmsg.c \
       ${cvs_srcdir}/main.c \
       ${cvs_srcdir}/mkmodules.c \
       ${cvs_srcdir}/modules.c \
       ${cvs_srcdir}/myndbm.c \
       ${cvs_srcdir}/no_diff.c \
       ${cvs_srcdir}/parseinfo.c \
       ${cvs_srcdir}/patch.c \
       ${cvs_srcdir}/rcs.c \
       ${cvs_srcdir}/rcscmds.c \
       ${cvs_srcdir}/recurse.c \
       ${cvs_srcdir}/release.c \
       ${cvs_srcdir}/remove.c \
       ${cvs_srcdir}/repos.c \
       ${cvs_srcdir}/root.c \
       ${cvs_srcdir}/rtag.c \
       ${cvs_srcdir}/scramble.c \
       ${cvs_srcdir}/server.c \
       ${cvs_srcdir}/status.c \
       ${cvs_srcdir}/subr.c \
       ${cvs_srcdir}/run.c \
       ${cvs_srcdir}/tag.c \
       ${cvs_srcdir}/update.c \
       ${cvs_srcdir}/watch.c \
       ${cvs_srcdir}/wrapper.c \
       ${cvs_srcdir}/vers_ts.c \
       ${cvs_srcdir}/version.c \
       ${cvs_srcdir}/zlib.c
# end of $COMMON_SOURCES

# sources in ../diff
DIFF_SOURCES = \
	   ${diff_srcdir}/analyze.c \
	   ${diff_srcdir}/cmpbuf.c \
	   ${diff_srcdir}/context.c \
	   ${diff_srcdir}/diff.c \
	   ${diff_srcdir}/diff3.c \
	   ${diff_srcdir}/dir.c \
	   ${diff_srcdir}/ed.c \
	   ${diff_srcdir}/ifdef.c \
	   ${diff_srcdir}/io.c \
	   ${diff_srcdir}/normal.c \
	   ${diff_srcdir}/side.c \
	   ${diff_srcdir}/util.c \
	   ${diff_srcdir}/version.c 

# sources from ../lib
# FIXME: Is this used anywhere?  I don't think it is.
LIB_SOURCES = \
       ${lib_dir}/vasprintf.c \
       ${lib_dir}/argmatch.c \
       ${lib_dir}/getline.c \
       ${lib_dir}/getopt.c \
       ${lib_dir}/getopt1.c \
       ${lib_dir}/md5.c \
       ${lib_dir}/regex.c \
       ${lib_dir}/sighandle.c \
       ${lib_dir}/valloc.c \
       ${lib_dir}/yesno.c \
       ${lib_dir}/getdate.c

# object files from OS/2 sources
# was       ${srcdir}/rcmd.o
OS2_OBJECTS = \
       ${srcdir}/filesubr.o \
       ${srcdir}/startserver.o \
       ${srcdir}/stripslash.o \
       ${top_srcdir}/windows-NT/rcmd.o \
       ${srcdir}/savecwd.o \
       ${srcdir}/system.o

# object files from ../src
COMMON_OBJECTS = \
       ${cvs_srcdir}/add.o \
       ${cvs_srcdir}/admin.o \
       ${cvs_srcdir}/buffer.o \
       ${cvs_srcdir}/checkin.o \
       ${cvs_srcdir}/checkout.o \
       ${cvs_srcdir}/classify.o \
       ${cvs_srcdir}/client.o \
       ${cvs_srcdir}/commit.o \
       ${cvs_srcdir}/create_adm.o \
       ${cvs_srcdir}/cvsrc.o \
       ${cvs_srcdir}/diff.o \
       ${cvs_srcdir}/edit.o \
       ${cvs_srcdir}/entries.o \
       ${cvs_srcdir}/expand_path.o \
       ${cvs_srcdir}/fileattr.o \
       ${cvs_srcdir}/find_names.o \
       ${cvs_srcdir}/hash.o \
       ${cvs_srcdir}/history.o \
       ${cvs_srcdir}/ignore.o \
       ${cvs_srcdir}/import.o \
       ${cvs_srcdir}/lock.o \
       ${cvs_srcdir}/log.o \
       ${cvs_srcdir}/login.o \
       ${cvs_srcdir}/logmsg.o \
       ${cvs_srcdir}/main.o \
       ${cvs_srcdir}/mkmodules.o \
       ${cvs_srcdir}/modules.o \
       ${cvs_srcdir}/myndbm.o \
       ${cvs_srcdir}/no_diff.o \
       ${cvs_srcdir}/parseinfo.o \
       ${cvs_srcdir}/patch.o \
       ${cvs_srcdir}/rcs.o \
       ${cvs_srcdir}/rcscmds.o \
       ${cvs_srcdir}/recurse.o \
       ${cvs_srcdir}/release.o \
       ${cvs_srcdir}/remove.o \
       ${cvs_srcdir}/repos.o \
       ${cvs_srcdir}/root.o \
       ${cvs_srcdir}/rtag.o \
       ${cvs_srcdir}/scramble.o \
       ${cvs_srcdir}/server.o \
       ${cvs_srcdir}/status.o \
       ${cvs_srcdir}/tag.o \
       ${cvs_srcdir}/update.o \
       ${cvs_srcdir}/watch.o \
       ${cvs_srcdir}/wrapper.o \
       ${cvs_srcdir}/vers_ts.o \
       ${cvs_srcdir}/subr.o \
       ${cvs_srcdir}/run.o \
       ${cvs_srcdir}/version.o \
       ${cvs_srcdir}/error.o \
       ${cvs_srcdir}/zlib.o
# end of $COMMON_OBJECTS

# object files in ../diff
DIFF_OBJECTS = \
       ${diff_srcdir}/analyze.o \
	   ${diff_srcdir}/cmpbuf.o \
	   ${diff_srcdir}/context.o \
	   ${diff_srcdir}/diff.o \
	   ${diff_srcdir}/diff3.o \
	   ${diff_srcdir}/dir.o \
	   ${diff_srcdir}/ed.o \
	   ${diff_srcdir}/ifdef.o \
	   ${diff_srcdir}/io.o \
	   ${diff_srcdir}/normal.o \
	   ${diff_srcdir}/side.o \
	   ${diff_srcdir}/util.o \
	   ${diff_srcdir}/version.o 

# objects from ../lib
LIB_OBJECTS = \
       ${lib_dir}/vasprintf.o \
       ${lib_dir}/argmatch.o \
       ${lib_dir}/getline.o \
       ${lib_dir}/getopt.o \
       ${lib_dir}/getopt1.o \
       ${lib_dir}/md5.o \
       ${lib_dir}/regex.o \
       ${lib_dir}/sighandle.o \
       ${lib_dir}/valloc.o \
       ${lib_dir}/yesno.o \
       ${lib_dir}/getdate.o

ZLIB_OBJECTS = ${zlib_dir}/adler32.o \
       ${zlib_dir}/compress.o \
       ${zlib_dir}/crc32.o \
       ${zlib_dir}/gzio.o \
       ${zlib_dir}/uncompr.o \
       ${zlib_dir}/deflate.o \
       ${zlib_dir}/trees.o \
       ${zlib_dir}/zutil.o \
       ${zlib_dir}/inflate.o \
       ${zlib_dir}/infblock.o \
       ${zlib_dir}/inftrees.o \
       ${zlib_dir}/infcodes.o \
       ${zlib_dir}/infutil.o \
       ${zlib_dir}/inffast.o

SOURCES = ${COMMON_SOURCES} ${LIB_SOURCES} ${OS2_SOURCES} ${DIFF_SOURCES}
HEADERS = ${COMMON_HEADERS} ${OS2_HEADERS} ${DIFF_HEADERS}
OBJECTS = ${COMMON_OBJECTS} ${LIB_OBJECTS} ${OS2_OBJECTS} ${DIFF_OBJECTS} \
  ${ZLIB_OBJECTS}

DISTFILES = ${OS2_HEADERS} ${OS2_SOURCES} \
  ${srcdir}/README ${srcdir}/ChangeLog \
  ${srcdir}/Makefile.in ${srcdir}/.cvsignore \
  Makefile

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
	rm -f cvs.exe cvs.out
	rm -f ${srcdir}/*.o
	rm -f ${diff_srcdir}/*.o
	rm -f ${lib_dir}/*.o ${lib_dir}/*.a
	rm -f ${zlib_dir}/*.o ${zlib_dir}/*.a
	rm -f ${cvs_srcdir}/*.o

clean-cvs:
	del cvs.exe cvs.out
	del $(subst /,\,${srcdir}/*.o)
	del $(subst /,\,${diff_srcdir}/*.o)
	del $(subst /,\,${lib_dir}/*.o ${lib_dir}/*.a)
	del $(subst /,\,${zlib_dir}/*.o ${zlib_dir}/*.a)
	del $(subst /,\,${cvs_srcdir}/*.o)

install-cvs: cvs.exe
	copy $(subst /,\,${srcdir}/cvs.exe ${install_dir}/cvs.exe)

%.o: %.c
	gcc ${OBJ_CFLAGS} ${CFLAGS} -o $@ -c $*.c

${lib_dir}/libcvs.a: $(LIB_OBJECTS)
	$(AR) cr $@ $(LIB_OBJECTS)

${zlib_dir}/libz.a: $(ZLIB_OBJECTS)
	$(AR) cr $@ $(ZLIB_OBJECTS)

# Eventually probably static linking (cvs-static.exe) will be the
# default, but even so we'll want to preserve this rule so we can
# create a dynamically linked executable when we want coredumps.
cvs.exe: ${OBJECTS} ${lib_dir}/libcvs.a ${zlib_dir}/libz.a
	gcc $(COMMON_OBJECTS) $(OS2_OBJECTS) ${DIFF_OBJECTS} ${lib_dir}/libcvs.a ${zlib_dir}/libz.a $(LIB) -o cvs.out
	emxbind -w -o $@.exe cvs.out

${lib_dir}/libcvs.lib: ${lib_dir}/libcvs.a
	emxomf ${lib_dir}/libcvs.a

${zlib_dir}/libz.lib: ${zlib_dir}/libz.a
	emxomf ${zlib_dir}/libz.a

# Where do -Zomf -Zsys go?  When we compile the .o's?
# When I tried this rule, I had problems with (1) could not find
# socket.lib.  Hmm.  I thought I converted it from socket.a
# in the EMX distribution.  Probably my EMX installation at fault.
# (2) could not find miscellaneous system routines like strerror.
# This might be a matter of learning to live without them (EMX
# doc is supposed to describe which are present and which are not).
# (3) some linker errors like no stack and the like.  Maybe I am
# missing some options?
cvs-static.exe: ${OBJECTS} ${lib_dir}/libcvs.lib ${zlib_dir}/libz.lib
	emxomf ${OBJECTS}
	emxomfld -o $@ $(subst .o,.obj,${OBJECTS}) \
	  ${lib_dir}/libcvs.lib ${zlib_dir}/libz.lib ${LIB}

# cvs.o: ${OBJECTS} ${SOURCES} ${HEADERS}

subdir = emx
# Hmm.  This rule wants to exist on unix, so that "make dist" works.
# And it doesn't want to exist on OS/2, because configure doesn't
# run on OS/2 and so ../config.status won't exist.  For now we just
# say, in emx/README, that people will need to comment it out.
Makefile: ../config.status $(srcdir)/Makefile.in
	cd .. && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= ./config.status

