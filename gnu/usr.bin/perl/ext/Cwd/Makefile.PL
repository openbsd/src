# core-only Makefile.PL
use ExtUtils::MakeMaker;
WriteMakefile(
    NAME    => 'Cwd',
    VERSION_FROM => '../../lib/Cwd.pm',
    'DEFINE' => '-DNO_PPPORT_H',
);
