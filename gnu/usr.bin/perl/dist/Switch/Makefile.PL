use ExtUtils::MakeMaker;
WriteMakefile(
    NAME => q[Switch],
    VERSION_FROM => q[Switch.pm],
    PREREQ_PM => { 'Filter::Util::Call' => 0, 'Text::Balanced' => 0 },
    INSTALLDIRS => ($] >= 5.00703 && $] < 5.011) ? 'perl' : 'site',
);
