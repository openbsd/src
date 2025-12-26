use ExtUtils::MakeMaker;

my $core = grep { $_ eq 'PERL_CORE=1' } @ARGV;

WriteMakefile(
    NAME => 'Safe',
    VERSION_FROM => 'Safe.pm',
    INSTALLDIRS => ($] < 5.011 ? 'perl' : 'site'),
    ($core || $] >= 5.011) ? () : (INST_LIB => '$(INST_ARCHLIB)'),
    AUTHOR => [
        'Malcolm Beattie',
        'Perl 5 Porters <perl5-porters@perl.org>'
    ],
    ABSTRACT_FROM => 'Safe.pm',
    LICENSE => 'perl_5',
    META_ADD => {
        'meta-spec' => {
            version => '2',
            url     => 'http://search.cpan.org/perldoc?CPAN::Meta::Spec',
        },
        'resources' => {
            'bugtracker' => {
                web => 'https://github.com/Perl/perl5/issues',
            },
            'repository' => {
                url  => 'git://github.com/Perl/perl5.git',
                web  => 'https://github.com/Perl/perl5',
                type => 'git'
            }
        }
    }
);
