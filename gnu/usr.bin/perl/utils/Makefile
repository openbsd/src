
PERL = ../miniperl
REALPERL = ../perl

# Files to be built with variable substitution after miniperl is
# available.  Dependencies handled manually below (for now).

pl = c2ph.PL h2ph.PL h2xs.PL perlbug.PL perldoc.PL perlivp.PL pl2pm.PL splain.PL perlcc.PL dprofpp.PL libnetcfg.PL piconv.PL enc2xs.PL
plextract  = c2ph h2ph h2xs perlbug perldoc perlivp pl2pm splain perlcc dprofpp libnetcfg piconv enc2xs
plextractexe  = ./c2ph ./h2ph ./h2xs ./perlbug ./perldoc ./perlivp ./pl2pm ./splain ./perlcc ./dprofpp ./libnetcfg ./piconv ./enc2xs

all: $(plextract) 

compile: all $(plextract)
	$(REALPERL) -I../lib perlcc -I .. -L .. c2ph -o c2ph.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. h2ph -o h2ph.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. h2xs -o h2xs.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. perlbug -o perlbug.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. perldoc -o perldoc.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. perlivp -o perlivp.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. pl2pm -o pl2pm.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. splain -o splain.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. perlcc -I .. -L .. -o perlcc.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. dprofpp -o dprofpp.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc -I .. -L .. libnetcfg -o libnetcfg.exe -v 10 -log ../compilelog;

$(plextract):
	$(PERL) -I../lib $@.PL

c2ph:		c2ph.PL ../config.sh

h2ph:		h2ph.PL ../config.sh

h2xs:		h2xs.PL ../config.sh

perlbug:	perlbug.PL ../config.sh ../patchlevel.h

perldoc:	perldoc.PL ../config.sh

perlivp:	perlivp.PL ../config.sh

pl2pm:		pl2pm.PL ../config.sh

splain:		splain.PL ../config.sh ../lib/diagnostics.pm

perlcc:		perlcc.PL ../config.sh

dprofpp:	dprofpp.PL ../config.sh

libnetcfg:	libnetcfg.PL ../config.sh

piconv:		piconv.PL ../config.sh

enc2xs:		enc2xs.PL ../config.sh

clean:

realclean:
	rm -rf $(plextract) pstruct $(plextractexe)
	rm -f ../t/_h2ph_pre.ph

clobber:	realclean

distclean:	clobber

veryclean:	distclean
	-rm -f *~ *.org
