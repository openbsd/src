
PERL = ../miniperl
REALPERL = ../perl
RUN =   # Used mainly cross-compilation setups.


# Files to be built with variable substitution after miniperl is
# available.  Dependencies handled manually below (for now).

pl = c2ph.PL config_data.PL corelist.PL cpan.PL h2ph.PL h2xs.PL instmodsh.PL perlbug.PL perldoc.PL perlivp.PL pl2pm.PL prove.PL cpanp-run-perl.PL cpanp.PL cpan2dist.PL splain.PL dprofpp.PL libnetcfg.PL piconv.PL enc2xs.PL xsubpp.PL
plextract = c2ph config_data corelist cpan h2ph h2xs instmodsh perlbug perldoc perlivp pl2pm prove cpanp-run-perl cpanp cpan2dist splain dprofpp libnetcfg piconv enc2xs xsubpp
plextractexe = ./c2ph ./config_data ./corelist ./cpan ./h2ph ./h2xs ./instmodsh ./perlbug ./perldoc ./perlivp ./pl2pm ./prove ./cpanp-run-perl ./cpanp ./cpan2dist ./splain ./dprofpp ./libnetcfg ./piconv ./enc2xs ./xsubpp

all: $(plextract) 

$(plextract):
	$(RUN) $(PERL) -I../lib $@.PL

c2ph:		c2ph.PL ../config.sh

cpan:		cpan.PL ../config.sh

config_data:	config_data.PL ../config.sh

corelist:	corelist.PL ../config.sh

h2ph:		h2ph.PL ../config.sh

h2xs:		h2xs.PL ../config.sh

instmodsh:	instmodsh.PL ../config.sh

perlbug:	perlbug.PL ../config.sh ../patchlevel.h

perldoc:	perldoc.PL ../config.sh

perlivp:	perlivp.PL ../config.sh

prove:		prove.PL ../config.sh

cpanp-run-perl:	cpanp-run-perl.PL ../config.sh

cpanp:	cpanp.PL ../config.sh

cpan2dist:	cpan2dist.PL ../config.sh

pl2pm:		pl2pm.PL ../config.sh

splain:		splain.PL ../config.sh ../lib/diagnostics.pm

dprofpp:	dprofpp.PL ../config.sh

libnetcfg:	libnetcfg.PL ../config.sh

piconv:		piconv.PL ../config.sh

enc2xs:		enc2xs.PL ../config.sh

xsubpp:		xsubpp.PL ../config.sh

clean:

realclean:
	rm -rf $(plextract) pstruct $(plextractexe)
	rm -f ../t/_h2ph_pre.ph

clobber:	realclean

distclean:	clobber

veryclean:	distclean
	-rm -f *~ *.org
