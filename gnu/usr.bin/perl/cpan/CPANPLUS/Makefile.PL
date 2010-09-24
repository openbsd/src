use strict;
use ExtUtils::MakeMaker;

WriteMakefile (
    NAME            => 'CPANPLUS',
    VERSION_FROM    => 'lib/CPANPLUS/Internals.pm', # finds $VERSION
    EXE_FILES       => ['bin/cpan2dist','bin/cpanp','bin/cpanp-run-perl'],
    INSTALLDIRS     => ( $] >= 5.009005 ? 'perl' : 'site' ),
    AUTHOR          => 'Jos Boumans <kane[at]cpan.org>',
    ABSTRACT        => 'Ameliorated interface to the CPAN'
);
