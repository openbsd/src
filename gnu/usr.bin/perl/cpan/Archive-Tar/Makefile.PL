use strict;
use ExtUtils::MakeMaker;

WriteMakefile (
    NAME            => 'Archive::Tar',
    VERSION_FROM    => 'lib/Archive/Tar.pm', # finds $VERSION
    EXE_FILES       => ['bin/ptar', 'bin/ptardiff'],
    INSTALLDIRS     => ( $] >= 5.009003 ? 'perl' : 'site' ),
    AUTHOR          => 'Jos Boumans <kane[at]cpan.org>',
    ABSTRACT        => 'Manipulates TAR archives'
);
