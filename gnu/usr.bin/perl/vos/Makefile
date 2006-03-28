#
# This Makefile can be used to update the config.alpha.h and
# config.ga.h files *in UNIX* (in VOS the configure_perl.cm
# is used).  Update the config.*.def files appropriately when
# you add new symbols to Configure.  If you don't know VOS,
# most of the time a safe guess for a symbol is 'undef'.
#

all:	config.alpha.h config.ga.h

config.alpha.h:	config.alpha.def ../config_h.SH mv-if-diff
	cp config.alpha.def config.def
	perl config.pl
	sh mv-if-diff config.h.new config.alpha.h
	rm -f config.def config.h.new

config.ga.h:	config.ga.def ../config_h.SH mv-if-diff
	cp config.ga.def config.def
	perl config.pl
	sh mv-if-diff config.h.new config.ga.h
	rm -f config.def config.h.new
