
PERL = ../miniperl

# Files to be built with variable substitution after miniperl is
# available.  Dependencies handled manually below (for now).

pl = c2ph.PL h2ph.PL h2xs.PL perlbug.PL perldoc.PL pl2pm.PL splain.PL
plextract  = c2ph h2ph h2xs perlbug perldoc pl2pm splain

all: $(plextract)

$(plextract):
	$(PERL) -I../lib $@.PL

c2ph:		c2ph.PL ../config.sh

h2ph:		h2ph.PL ../config.sh

h2xs:		h2xs.PL ../config.sh

perlbug:	perlbug.PL ../config.sh ../patchlevel.h

perldoc:	perldoc.PL ../config.sh

pl2pm:		pl2pm.PL ../config.sh

splain:		splain.PL ../config.sh ../lib/diagnostics.pm

clean:

realclean:
	rm -rf $(plextract) pstruct

clobber:	realclean

distclean:	clobber
