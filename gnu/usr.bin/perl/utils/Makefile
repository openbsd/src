
PERL = ../miniperl

# Files to be built with variable substitution after miniperl is
# available.  Dependencies handled manually below (for now).

pl = c2ph.PL h2ph.PL h2xs.PL perlbug.PL perldoc.PL

pl = c2ph.PL h2ph.PL h2xs.PL perlbug.PL perldoc.PL pl2pm.PL
plextract  = c2ph h2ph h2xs perlbug perldoc pl2pm

all: $(plextract)

$(plextract):
	$(PERL) -I../lib $@.PL

clean:

realclean:
	rm -rf $(plextract) pstruct

clobber:	realclean

distclean:	clobber
