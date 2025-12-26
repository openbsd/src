#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  Copyright (c) 2017, Reini Urban
#
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

use strict;
use warnings;
use ExtUtils::MakeMaker 6.31;
use Config;

WriteMakefile(
    NAME                => 'Storable',
    AUTHOR              => 'Perl 5 Porters',
    LICENSE             => 'perl',
    PREREQ_PM           => {
        XSLoader => 0,
    },
    INSTALLDIRS         => ($] >= 5.007 && $] < 5.012) ? 'perl' : 'site',
    VERSION_FROM        => 'lib/Storable.pm',
    ABSTRACT_FROM       => 'lib/Storable.pm',
    CONFIGURE_REQUIRES  => {
        'ExtUtils::MakeMaker' => '6.31',
    },
    BUILD_REQUIRES      => {
        'ExtUtils::MakeMaker' => '6.31',
    },
    TEST_REQUIRES       => {
        'Test::More' => '0.82',
        'File::Temp' => '0',
    },
    META_MERGE          => {
        "meta-spec" => { version => 2 },
        resources   => {
            bugtracker => 'https://github.com/Perl/perl5/issues',
        },
        provides    => {
            'Storable'  => {
                file        => 'lib/Storable.pm',
                version     => MM->parse_version('lib/Storable.pm'),
            },
        },
    },
    clean               => { FILES => 'Storable-*' },
);
