use ExtUtils::MakeMaker;

unless($ENV{PERL_CORE}) {
    $ENV{PERL_CORE} = 1 if grep { $_ eq 'PERL_CORE=1' } @ARGV;
}
if ($ENV{PERL_CORE}) {
    # Pods will be built by installman.
    @coreopts = ( MAN3PODS => {} );
}
else {
    @coreopts = ();
}

WriteMakefile(
    NAME	=> "Devel::PPPort",
    DISTNAME	=> "Devel-PPPort",
    VERSION_FROM=> 'PPPort.pm',

    PL_FILES	=> { 'ppport_h.PL' => 'ppport.h' },
    'depend'	=> { '$(OBJECT)' => '$(H_FILES)' },
    C		=> [qw(module2.c module3.c)],
    H		=> [qw(ppport.h)],
    OBJECT	=> '$(BASEEXT)$(OBJ_EXT) $(O_FILES)',
    XSPROTOARG	=> '-noprototypes',
    'dist'	=> { COMPRESS=>"gzip", SUFFIX=>"gz" },
    'clean'	=> { FILES => qw($(H_FILES))},
    @coreopts,
);
