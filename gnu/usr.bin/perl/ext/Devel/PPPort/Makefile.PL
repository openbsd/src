use ExtUtils::MakeMaker;

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
    'clean'	=> { FILES => qw($(H_FILES))}
);
