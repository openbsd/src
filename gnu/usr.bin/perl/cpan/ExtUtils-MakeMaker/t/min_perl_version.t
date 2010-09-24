#!/usr/bin/perl -w

# This is a test checking various aspects of the optional argument
# MIN_PERL_VERSION to WriteMakefile.

BEGIN {
    unshift @INC, 't/lib';
}

use strict;
use Test::More tests => 33;

use TieOut;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::MPV;
use File::Path;

use ExtUtils::MakeMaker;

# avoid environment variables interfering with our make runs
delete @ENV{qw(LIB MAKEFLAGS)};

my $perl     = which_perl();
my $make     = make_run();
my $makefile = makefile_name();

chdir 't';

perl_lib();

ok( setup_recurs(), 'setup' );
END {
    ok( chdir(File::Spec->updir), 'leaving dir' );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir 'Min-PerlVers', 'entering dir Min-PerlVers' ) ||
    diag("chdir failed: $!");

{
    # ----- argument verification -----

    my $stdout = tie *STDOUT, 'TieOut';
    ok( $stdout, 'capturing stdout' );
    my $warnings = '';
    local $SIG{__WARN__} = sub {
        $warnings .= join '', @_;
    };

    eval {
        WriteMakefile(
            NAME             => 'Min::PerlVers',
            MIN_PERL_VERSION => '5',
        );
    };
    is( $warnings, '', 'MIN_PERL_VERSION=5 does not trigger a warning' );
    is( $@, '',        '  nor a hard failure' );


    $warnings = '';
    eval {
        WriteMakefile(
            NAME             => 'Min::PerlVers',
            MIN_PERL_VERSION => '5.4.4',
        );
    };
    is( $warnings, '', 'MIN_PERL_VERSION=X.Y.Z does not trigger a warning' );
    is( $@, '',        '  nor a hard failure' );


    $warnings = '';
    eval {
        WriteMakefile(
            NAME             => 'Min::PerlVers',
            MIN_PERL_VERSION => '999999',
        );
    };
    ok( '' ne $warnings, 'MIN_PERL_VERSION=999999 triggers a warning' );
    is( $warnings,
        "Warning: Perl version 999999 or higher required. We run $].\n",
                         '  with expected message text' );
    is( $@, '',          '  and without a hard failure' );

    $warnings = '';
    eval {
        WriteMakefile(
            NAME             => 'Min::PerlVers',
            MIN_PERL_VERSION => '999999',
            PREREQ_FATAL     => 1,
        );
    };
    is( $warnings, '', 'MIN_PERL_VERSION=999999 and PREREQ_FATAL: no warning' );
    is( $@, <<"END",   '  correct exception' );
MakeMaker FATAL: perl version too low for this distribution.
Required is 999999. We run $].
END

    $warnings = '';
    eval {
        WriteMakefile(
            NAME             => 'Min::PerlVers',
            MIN_PERL_VERSION => 'foobar',
        );
    };
    ok( '' ne $warnings,    'MIN_PERL_VERSION=foobar triggers a warning' );
    is( $warnings, <<'END', '  with expected message text' );
Warning: MIN_PERL_VERSION is not in a recognized format.
Recommended is a quoted numerical value like '5.005' or '5.008001'.
END

    is( $@, '',             '  and without a hard failure' );
}


# ----- PREREQ_PRINT output -----
{
    my $prereq_out = run(qq{$perl Makefile.PL "PREREQ_PRINT=1"});
    is( $?, 0,            'PREREQ_PRINT exiting normally' );
    my $prereq_out_sane = $prereq_out =~ /^\s*\$PREREQ_PM\s*=/;
    ok( $prereq_out_sane, '  and talking like we expect' ) ||
        diag($prereq_out);

    SKIP: {
        skip 'not going to evaluate rubbish', 3 if !$prereq_out_sane;

        package _Prereq::Print::WithMPV;          ## no critic
        our($PREREQ_PM, $BUILD_REQUIRES, $MIN_PERL_VERSION, $ERR);
        $ERR = '';
        eval {
            eval $prereq_out;                     ## no critic
            $ERR = $@;
        };
        ::is( $@ . $ERR, '',                      'prereqs evaluable' );
        ::is_deeply( $PREREQ_PM, { strict => 0 }, '  and looking correct' );
        ::is( $MIN_PERL_VERSION, '5.005',         'min version also correct' );
    }
}


# ----- PRINT_PREREQ output -----
{
    my $prereq_out = run(qq{$perl Makefile.PL "PRINT_PREREQ=1"});
    is( $?, 0,                      'PRINT_PREREQ exiting normally' );
    ok( $prereq_out !~ /^warning/i, '  and not complaining loudly' );
    like( $prereq_out,
        qr/^perl\(perl\) \s* >= 5\.005 \s+ perl\(strict\) \s* >= \s* 0 \s*$/x,
                                    'dump has prereqs and perl version' );
}


# ----- generated files verification -----
{
    unlink $makefile;
    my @mpl_out = run(qq{$perl Makefile.PL});
    END { unlink $makefile, makefile_backup() }

    cmp_ok( $?, '==', 0, 'Makefile.PL exiting normally' ) || diag(@mpl_out);
    ok( -e $makefile, 'Makefile present' );
}


# ----- ppd output -----
{
    my $ppd_file = 'Min-PerlVers.ppd';
    my @make_out = run(qq{$make ppd});
    END { unlink $ppd_file }

    cmp_ok( $?, '==', 0,    'Make ppd exiting normally' ) || diag(@make_out);

    my $ppd_html = slurp($ppd_file);
    ok( defined($ppd_html), '  .ppd file present' );

    like( $ppd_html, qr{^\s*<PERLCORE VERSION="5,005,0,0" />}m,
                            '  .ppd file content good' );
}


# ----- META.yml output -----
{
    my $distdir  = 'Min-PerlVers-0.05';
    $distdir =~ s{\.}{_}g if $Is_VMS;

    my $meta_yml = "$distdir/META.yml";
    my @make_out    = run(qq{$make metafile});
    END { rmtree $distdir }

    cmp_ok( $?, '==', 0, 'Make metafile exiting normally' ) || diag(@make_out);
    my $meta = slurp($meta_yml);
    ok( defined($meta),  '  META.yml present' );

    like( $meta, qr{\nrequires:[^\S\n]*\n\s+perl:\s+5\.005\n\s+strict:\s+0\n},
                         '  META.yml content good');
}

__END__
