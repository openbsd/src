#
# OpenSSL/engines/Makefile
#

DIR=	engines
TOP=	..
CC=	cc
INCLUDES= -I../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r
ENGDIRS= ccgost

RECURSIVE_MAKE=	[ -z "$(ENGDIRS)" ] || for i in $(ENGDIRS) ; do \
		    (cd $$i && echo "making $$target in $(DIR)/$$i..." && \
		    $(MAKE) -e TOP=../.. DIR=$$i $$target ) || exit 1; \
		done;

PEX_LIBS=
EX_LIBS=

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile engines.com install.com engine_vector.mar
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBNAMES=

LIBSRC=
LIBOBJ=

SRC= $(LIBSRC)

EXHEADER= 
HEADER=	

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ..; $(MAKE) DIRS=$(DIR) all)

all:	lib subdirs

lib:	$(LIBOBJ)
	@if [ -n "$(SHARED_LIBS)" ]; then \
		set -e; \
		for l in $(LIBNAMES); do \
			$(MAKE) -f ../Makefile.shared -e \
				LIBNAME=$$l LIBEXTRAS=e_$$l.o \
				LIBDEPS='-L.. -lcrypto $(EX_LIBS)' \
				link_o.$(SHLIB_TARGET); \
		done; \
	else \
		$(AR) $(LIB) $(LIBOBJ); \
		$(RANLIB) $(LIB) || echo Never mind.; \
	fi; \
	touch lib

subdirs:
	echo $(EDIRS)
	@target=all; $(RECURSIVE_MAKE)

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO
	@target=files; $(RECURSIVE_MAKE)

links:
	@target=links; $(RECURSIVE_MAKE)

# XXXXX This currently only works on systems that use .so as suffix
# for shared libraries as well as for Cygwin which uses the
# dlfcn_name_converter and therefore stores the engines with .so suffix, too.
# XXXXX This was extended to HP-UX dl targets, which use .sl suffix.
# XXXXX This was extended to mingw targets, which use eay32.dll suffix without lib as prefix.
install:
	@[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	@if [ -n "$(SHARED_LIBS)" ]; then \
		set -e; \
		$(PERL) $(TOP)/util/mkdir-p.pl $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines; \
		for l in $(LIBNAMES); do \
			( echo installing $$l; \
			  pfx=lib; \
			  if [ "$(PLATFORM)" != "Cygwin" ]; then \
				case "$(CFLAGS)" in \
				*DSO_BEOS*)	sfx=".so";;	\
				*DSO_DLFCN*)	sfx=`expr "$(SHLIB_EXT)" : '.*\(\.[a-z][a-z]*\)' \| ".so"`;;	\
				*DSO_DL*)	sfx=".sl";;	\
				*DSO_WIN32*)	sfx="eay32.dll"; pfx=;;	\
				*)		sfx=".bad";;	\
				esac; \
				cp $$pfx$$l$$sfx $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$$pfx$$l$$sfx.new; \
			  else \
				sfx=".so"; \
				cp cyg$$l.dll $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$$pfx$$l$$sfx.new; \
			  fi; \
			  chmod 555 $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$$pfx$$l$$sfx.new; \
			  mv -f $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$$pfx$$l$$sfx.new $(INSTALL_PREFIX)$(INSTALLTOP)/$(LIBDIR)/engines/$$pfx$$l$$sfx ); \
		done; \
	fi
	@target=install; $(RECURSIVE_MAKE)

tags:
	ctags $(SRC)

errors:
	set -e; for l in $(LIBNAMES); do \
		$(PERL) ../util/mkerr.pl -conf e_$$l.ec \
			-nostatic -staticloader -write e_$$l.c; \
	done
	(cd ccgost; $(MAKE) PERL=$(PERL) errors)

tests:

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff
	@target=lint; $(RECURSIVE_MAKE)

depend:
	@if [ -z "$(THIS)" ]; then \
	    $(MAKE) -f $(TOP)/Makefile reflect THIS=$@; \
	fi
	@[ -z "$(THIS)" ] || $(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)
	@[ -z "$(THIS)" ] || (set -e; target=depend; $(RECURSIVE_MAKE) )

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)
	@target=dclean; $(RECURSIVE_MAKE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff
	@target=clean; $(RECURSIVE_MAKE)

# DO NOT DELETE THIS LINE -- make depend depends on it.
