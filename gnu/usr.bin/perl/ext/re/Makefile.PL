use ExtUtils::MakeMaker;
use File::Spec;
use Config;

my $object = 're_exec$(OBJ_EXT) re_comp$(OBJ_EXT) re$(OBJ_EXT)';

my $defines = '-DPERL_EXT_RE_BUILD -DPERL_EXT_RE_DEBUG -DPERL_EXT';

WriteMakefile(
    NAME		=> 're',
    VERSION_FROM	=> 're.pm',
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
    my $dquote_static_c = upupfile('dquote_static.c');
    my $inline_invlist_c = upupfile('inline_invlist.c');

    <<EOF;
re_comp.c : $regcomp_c
	- \$(RM_F) re_comp.c
	\$(CP) $regcomp_c re_comp.c

re_comp\$(OBJ_EXT) : re_comp.c dquote_static.c inline_invlist.c

re_exec.c : $regexec_c
	- \$(RM_F) re_exec.c
	\$(CP) $regexec_c re_exec.c

re_exec\$(OBJ_EXT) : re_exec.c inline_invlist.c

dquote_static.c : $dquote_static_c
	- \$(RM_F) dquote_static.c
	\$(CP) $dquote_static_c dquote_static.c

inline_invlist.c : $inline_invlist_c
	- \$(RM_F) inline_invlist.c
	\$(CP) $inline_invlist_c inline_invlist.c

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
