## Makefile.am for texinfo/emacs.
## $Id: Makefile.am,v 1.3 2000/02/09 02:18:37 espie Exp $
## Run automake in .. to produce Makefile.in from this.

# Put texinfo.txi first because that's the best candidate UPDATED in
# version.texi.
info_TEXINFOS = texinfo.txi info-stnd.texi info.texi

man_MANS = info.1 install-info.1 makeinfo.1 texindex.1 texi2dvi.1 \
           info.5 texinfo.5
HELP2MAN = help2man
noinst_SCRIPTS = $(HELP2MAN)

# Use the programs built in our distribution.
MAKEINFO = ../makeinfo/makeinfo
INSTALL_INFO = ../util/install-info

# Include our texinfo.tex, not Automake's.
EXTRA_DIST = macro.texi userdoc.texi epsf.tex texinfo.tex \
             txi-cs.tex txi-de.tex txi-en.tex txi-es.tex \
	     txi-nl.tex txi-no.tex txi-pt.tex \
	     $(man_MANS) $(noinst_SCRIPTS)

install-data-local:
	@echo "WARNING: You must install texinfo.tex, txi-??.tex, and epsf.tex manually,"
	@echo "WARNING: perhaps in TEXMF/tex/texinfo/ and"
	@echo "WARNING: TEXMF/tex/generic/dvips/ respectively,"
	@echo "WARNING: where TEXMF is a root of your TeX tree."
	@echo "WARNING: See doc/README for some considerations."
	@echo "WARNING: You can run make TEXMF=/your/texmf install-tex to do this."

texmf_texinfo = $(TEXMF)/tex/texinfo
texmf_dvips = $(TEXMF)/tex/generic/dvips
install-tex:
	test -n "$(TEXMF)" || (echo "TEXMF must be set." >&2; exit 1)
	$(mkinstalldirs) $(texmf_texinfo) $(texmf_dvips)
	$(INSTALL_DATA) texinfo.tex $(texmf_texinfo)/texinfo.tex
	$(INSTALL_DATA) txi-cs.tex $(texmf_texinfo)/txi-cs.tex
	$(INSTALL_DATA) txi-de.tex $(texmf_texinfo)/txi-de.tex
	$(INSTALL_DATA) txi-no.tex $(texmf_texinfo)/txi-no.tex
	$(INSTALL_DATA) epsf.tex $(texmf_dvips)/epsf.tex

# Don't want to run help2man at every installation.
if TEXINFO_MAINT
info.1: ../info/ginfo $(HELP2MAN)
	$(HELP2MAN) --name="read Info documents" $< >$@
install-info.1: $(INSTALL_INFO) $(HELP2MAN)
	$(HELP2MAN) --name="update info/dir entries" $< >$@
makeinfo.1: $(MAKEINFO) $(HELP2MAN)
	$(HELP2MAN) --name="translate Texinfo documents" $< >$@
texi2dvi.1: ../util/texi2dvi $(HELP2MAN)
	$(HELP2MAN) --name="print Texinfo documents" $< >$@
texindex.1: ../util/texindex $(HELP2MAN)
	$(HELP2MAN) --name="sort Texinfo index files" $< >$@
endif

# Do not create info files for distribution, but do create man pages.
dist-info:

# Do not try to build the info files in $(srcdir),
# since we don't distribute them.
.texi.info:
	$(MAKEINFO) -I$(srcdir) `echo $< | sed 's,.*/,,'`
texinfo: $(srcdir)/texinfo.txi
	$(MAKEINFO) -I$(srcdir) texinfo.txi

# Similarly, Do not try to install the info files from $(srcdir).
install-info-am: $(INFO_DEPS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(infodir)
	@for file in $(INFO_DEPS); do \
	  d=.; \
	  for ifile in `cd $$d && echo $$file $$file-[0-9] $$file-[0-9][0-9]`; do \
	    if test -f $$d/$$ifile; then \
	      echo " $(INSTALL_DATA) $$d/$$ifile $(infodir)/$$ifile"; \
	      $(INSTALL_DATA) $$d/$$ifile $(infodir)/$$ifile; \
	    else : ; fi; \
	  done; \
	done
	@$(POST_INSTALL)
	@if $(SHELL) -c '$(INSTALL_INFO) --version | sed 1q | fgrep -s -v -i debian' >/dev/null 2>&1; then \
	  for file in $(INFO_DEPS); do \
	    echo " $(INSTALL_INFO) --info-dir=$(infodir) $(infodir)/$$file";\
	    $(INSTALL_INFO) --info-dir=$(infodir) $(infodir)/$$file || :;\
	  done; \
	else : ; fi


# Remove the info files at make distclean.
distclean-aminfo:
	rm -f texinfo texinfo-* info*.info*
