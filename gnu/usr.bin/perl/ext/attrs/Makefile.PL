use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'attrs',
    VERSION_FROM => 'attrs.pm',
    XSPROTOARG => '-noprototypes'
);
