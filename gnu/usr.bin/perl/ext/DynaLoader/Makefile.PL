use strict;
use ExtUtils::MakeMaker;

my $is_mswin    = $^O eq 'MSWin32';
my $is_netware  = $^O eq 'NetWare';
my $is_vms      = $^O eq 'VMS';

WriteMakefile(
    NAME => 'DynaLoader',
    LINKTYPE	=> 'static',
    DEFINE	=> '-DLIBC="$(LIBC)"',
    SKIP	=> [qw(dynamic dynamic_lib dynamic_bs)],
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'DynaLoader_pm.PL',
    PL_FILES	=> {'DynaLoader_pm.PL'=>'DynaLoader.pm'},
    PM		=> {'DynaLoader.pm' => '$(INST_LIBDIR)/DynaLoader.pm'},
    depend      => {'DynaLoader$(OBJ_EXT)' => 'dlutils.c'},
    clean	=> {FILES => 'DynaLoader.c DynaLoader.xs DynaLoader.pm'},
);

sub MY::postamble {
    my $test_xs;

    if ($is_mswin || $is_netware || $is_vms) {
	$test_xs = '';
    } else {
	$test_xs = <<'EOT';

# Perform very simple tests just to check for major gaffs.
# We can\'t do much more for platforms we are not executing on.
test-xs:
	for i in dl_*xs; \
	    do $(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) $(XSUBPPARGS) $$i > /dev/null; \
	done
EOT
    }

    return '
DynaLoader.xs: $(DLSRC)
	$(RM_F) $@
	$(CP) $? $@
' . $test_xs;
}

sub MY::static {
    my $object;
    if ($is_mswin || $is_netware) {
	$object = '$(PERL_SRC)\\$(OBJECT)';
    } elsif ($is_vms) {
	$object = '$(PERL_SRC)$(OBJECT)';
    } else {
	$object = '$(PERL_SRC)/$(OBJECT)';
    }


    return "
$object : \$(FIRST_MAKEFILE) \$(OBJECT)
	#\$(RM_RF) $object
	#\$(CP) \$(OBJECT) $object

static :: $object
";
}
