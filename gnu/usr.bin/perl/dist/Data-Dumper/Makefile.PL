use strict;
use warnings;
use ExtUtils::MakeMaker;

my @extra;
push @extra, 'LICENSE' => 'perl_5'
    unless $ExtUtils::MakeMaker::VERSION < 6.31;
push @extra, 'META_MERGE' => {
        resources => {
            repository  => 'git://perl5.git.perl.org/perl.git',
            bugtracker => 'https://github.com/Perl/perl5/issues',
            homepage    => "http://dev.perl.org/",
        },
    } unless $ExtUtils::MakeMaker::VERSION < 6.46;

WriteMakefile(
    NAME          => 'Data::Dumper',
    VERSION_FROM  => 'Dumper.pm',
    ABSTRACT_FROM => 'Dumper.pm',
    $] <= 5.011000 ? ( INSTALLDIRS => 'perl' ) : (),
    ((grep { $_ eq 'PERL_CORE=1' } @ARGV) ? () : ('DEFINE' => '-DUSE_PPPORT_H')),
    @extra,
);
