use ExtUtils::MakeMaker;

WriteMakefile(
    NAME	=> 'DynaLoader',
    LINKTYPE	=> 'static',
    DEFINE	=> '-DLIBC="$(LIBC)"',
    MAN3PODS 	=> ' ', 	# Pods will be built by installman.
    SKIP	=> [qw(dynamic dynamic_lib dynamic_bs)],
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'DynaLoader.pm',
    clean	=> {FILES => 'DynaLoader.c'},
);


sub MY::postamble {
	'
DynaLoader.c: $(DLSRC)
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $(DLSRC) >tmp && mv tmp $@

# Perform very simple tests just to check for major gaffs.
# We can\'t do much more for platforms we are not executing on.
test-xs:
	for i in dl_*xs; \
	    do $(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) $(XSUBPPARGS) $$i > /dev/null; \
	done
';
}

