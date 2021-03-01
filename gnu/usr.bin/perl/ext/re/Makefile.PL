use ExtUtils::MakeMaker;
use File::Spec;
use Config;

my $object = 're_exec$(OBJ_EXT) re_comp$(OBJ_EXT) re$(OBJ_EXT)';

my $defines = '-DPERL_EXT_RE_BUILD -DPERL_EXT_RE_DEBUG -DPERL_EXT';

my @libs;
if ($^O eq 'cygwin' && $Config{usequadmath}) {
    push @libs, qw(quadmath);
}
WriteMakefile(
    NAME		=> 're',
    @libs ? ( 'LIBS' => [ join(" ", map { "-l$_" } @libs) ] ) : (),
    VERSION_FROM	=> 're.pm',
    XSPROTOARG		=> '-noprototypes',
    OBJECT		=> $object,
    DEFINE             => $defines,
    clean		=> { FILES => '*$(OBJ_EXT) invlist_inline.h *.c ../../lib/re.pm' },
);

package MY;

sub upupfile {
    File::Spec->catfile(File::Spec->updir, File::Spec->updir, $_[0]);
}

sub postamble {
    my $regcomp_c = upupfile('regcomp.c');
    my $regexec_c = upupfile('regexec.c');
    my $dquote_c = upupfile('dquote.c');
    my $invlist_inline_h = upupfile('invlist_inline.h');

    <<EOF;
re_comp.c : $regcomp_c
	- \$(RM_F) re_comp.c
	\$(CP) $regcomp_c re_comp.c

re_comp\$(OBJ_EXT) : re_comp.c dquote.c invlist_inline.h

re_exec.c : $regexec_c
	- \$(RM_F) re_exec.c
	\$(CP) $regexec_c re_exec.c

re_exec\$(OBJ_EXT) : re_exec.c invlist_inline.h

dquote.c : $dquote_c
	- \$(RM_F) dquote.c
	\$(CP) $dquote_c dquote.c

invlist_inline.h : $invlist_inline_h
	- \$(RM_F) invlist_inline.h
	\$(CP) $invlist_inline_h invlist_inline.h

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
