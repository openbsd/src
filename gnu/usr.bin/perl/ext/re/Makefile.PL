use ExtUtils::MakeMaker;
use File::Spec;
use Config;

my $object = 're_exec$(OBJ_EXT) re_comp$(OBJ_EXT) re$(OBJ_EXT)';

my $defines = '-DPERL_EXT_RE_BUILD -DPERL_EXT_RE_DEBUG -DPERL_EXT';

WriteMakefile(
    NAME		=> 're',
    VERSION_FROM	=> 're.pm',
    MAN3PODS		=> {}, 	# Pods will be built by installman.
    XSPROTOARG		=> '-noprototypes',
    OBJECT		=> $object,
    DEFINE             => $defines,
    clean		=> { FILES => '*$(OBJ_EXT) *.c ../../lib/re.pm' },
);

package MY;

sub upupfile {
    File::Spec->catfile(File::Spec->updir, File::Spec->updir, $_[0]);
}

sub postamble {
    my $regcomp_c = upupfile('regcomp.c');
    my $regexec_c = upupfile('regexec.c');

    <<EOF;
re_comp.c : $regcomp_c
	- \$(RM_F) re_comp.c
	\$(CP) $regcomp_c re_comp.c

re_comp\$(OBJ_EXT) : re_comp.c

re_exec.c : $regexec_c
	- \$(RM_F) re_exec.c
	\$(CP) $regexec_c re_exec.c

re_exec\$(OBJ_EXT) : re_exec.c

EOF
}

sub MY::c_o {
    my($self) = @_;
    package MY; # so that "SUPER" works right
    my $inh = $self->SUPER::c_o(@_);
    use Config;
    if ($Config{osname} eq 'aix' && $Config{ccversion} eq '5.0.1.0') {
	# Known buggy optimizer.
	my $cccmd = $self->const_cccmd;
	$cccmd =~ s/^CCCMD\s*=\s*//;
	$cccmd =~ s/\s\$\(OPTIMIZE\)\s/ /;
	$inh .= qq{

re_comp\$\(OBJ_EXT\): re_comp.c
\t$cccmd \$(CCCDLFLAGS) -I\$(PERL_INC) \$(DEFINE) \$*.c
};
    }
    $inh;
}
