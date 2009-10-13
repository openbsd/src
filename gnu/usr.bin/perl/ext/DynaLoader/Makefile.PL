use ExtUtils::MakeMaker;

WriteMakefile(
    NAME => 'DynaLoader',
    LINKTYPE	=> 'static',
    DEFINE	=> '-DLIBC="$(LIBC)"',
    SKIP	=> [qw(dynamic dynamic_lib dynamic_bs)],
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'DynaLoader_pm.PL',
    PL_FILES	=> {'DynaLoader_pm.PL'=>'DynaLoader.pm',
		    'XSLoader_pm.PL'=>'XSLoader.pm'},
    PM		=> {'DynaLoader.pm' => '$(INST_LIBDIR)/DynaLoader.pm',
		    'XSLoader.pm' => '$(INST_LIBDIR)/XSLoader.pm'},
    depend      => {'DynaLoader$(OBJ_EXT)' => 'dlutils.c'},
    clean	=> {FILES => 'DynaLoader.c DynaLoader.xs DynaLoader.pm ' .
			     'XSLoader.pm'},
);

sub MY::postamble {
	'
DynaLoader.xs: $(DLSRC)
	$(RM_F) $@
	$(CP) $? $@

# Perform very simple tests just to check for major gaffs.
# We can\'t do much more for platforms we are not executing on.
test-xs:
	for i in dl_*xs; \
	    do $(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) $(XSUBPPARGS) $$i > /dev/null; \
	done
';
}

sub MY::static {
	'
$(PERL_SRC)/$(OBJECT) : $(FIRST_MAKEFILE) $(OBJECT)
	#$(RM_RF) $(PERL_SRC)/$(OBJECT)
	#$(CP) $(OBJECT) $(PERL_SRC)/$(OBJECT)

static :: $(PERL_SRC)/$(OBJECT)
';
}
