#!/usr/bin/perl -w

# This is a test of the verification of the arguments to
# WriteMakefile.

BEGIN {
    unshift @INC, 't/lib';
}

use strict;
use Test::More tests => 16;
use File::Temp qw[tempdir];

use TieOut;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::BFD;

use ExtUtils::MakeMaker;

my $tmpdir = tempdir( DIR => 't', CLEANUP => 1 );
chdir $tmpdir;

perl_lib();

ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir 'Big-Dummy', "chdir'd to Big-Dummy" ) ||
  diag("chdir failed: $!");

{
    ok( my $stdout = tie *STDOUT, 'TieOut' );
    my $warnings = '';
    local $SIG{__WARN__} = sub {
        $warnings .= join '', @_;
    };
    # prerequisite warnings are disabled while building the perl core:
    local $ENV{PERL_CORE} = 0;

    WriteMakefile(
        NAME            => 'Big::Dummy',
        PREREQ_PM       => {
            strict  => 0
        }
    );
    is $warnings, '';

    $warnings = '';
    WriteMakefile(
        NAME            => 'Big::Dummy',
        PREREQ_PM       => {
            strict  => 99999
        }
    );
    is $warnings,
    sprintf("Warning: prerequisite strict 99999 not found. We have %s.\n",
            $strict::VERSION);

    $warnings = '';
    WriteMakefile(
        NAME            => 'Big::Dummy',
        PREREQ_PM       => {
            "I::Do::Not::Exist" => 0,
        }
    );
    is $warnings,
    "Warning: prerequisite I::Do::Not::Exist 0 not found.\n";


    $warnings = '';
    WriteMakefile(
        NAME            => 'Big::Dummy',
        PREREQ_PM       => {
            "I::Do::Not::Exist" => "",
        }
    );
    my @warnings = split /\n/, $warnings;
    is @warnings, 2;
    like $warnings[0], qr{^Unparsable version '' for prerequisite I::Do::Not::Exist\b};
    is $warnings[1], "Warning: prerequisite I::Do::Not::Exist 0 not found.";


    $warnings = '';
    WriteMakefile(
        NAME            => 'Big::Dummy',
        PREREQ_PM       => {
            "I::Do::Not::Exist" => 0,
            "strict"            => 99999,
        }
    );
    is $warnings,
    "Warning: prerequisite I::Do::Not::Exist 0 not found.\n".
    sprintf("Warning: prerequisite strict 99999 not found. We have %s.\n",
            $strict::VERSION);

    $warnings = '';
    eval {
        WriteMakefile(
            NAME            => 'Big::Dummy',
            PREREQ_PM       => {
                "I::Do::Not::Exist" => 0,
                "Nor::Do::I"        => 0,
                "strict"            => 99999,
            },
            PREREQ_FATAL    => 1,
        );
    };

    is $warnings, '';
    is $@, <<'END', "PREREQ_FATAL";
MakeMaker FATAL: prerequisites not found.
    I::Do::Not::Exist not installed
    Nor::Do::I not installed
    strict 99999

Please install these modules first and rerun 'perl Makefile.PL'.
END


    $warnings = '';
    eval {
        WriteMakefile(
            NAME            => 'Big::Dummy',
            PREREQ_PM       => {
                "I::Do::Not::Exist" => 0,
            },
            CONFIGURE => sub {
                require I::Do::Not::Exist;
            },
            PREREQ_FATAL    => 1,
        );
    };

    is $warnings, '';
    is $@, <<'END', "PREREQ_FATAL happens before CONFIGURE";
MakeMaker FATAL: prerequisites not found.
    I::Do::Not::Exist not installed

Please install these modules first and rerun 'perl Makefile.PL'.
END

}
