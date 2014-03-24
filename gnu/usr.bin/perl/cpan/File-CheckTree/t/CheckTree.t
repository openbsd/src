#!./perl -w

use Test::More tests => 23;

use strict;

require overload;

use File::CheckTree;
use File::Spec;          # used to get absolute paths

# We assume that we start from the dist/File-CheckTree in the perl repository,
# or the dist root directory for the CPAN version.


#### TEST 1 -- No warnings ####
# usings both relative and full paths, indented comments

{
    my ($num_warnings, $path_to_libFileCheckTree);
    $path_to_libFileCheckTree = File::Spec->rel2abs(
        File::Spec->catfile('lib', 'File', 'CheckTree.pm'),
    );

    my @warnings;
    local $SIG{__WARN__} = sub { push @warnings, "@_" };

    eval {
        $num_warnings = validate qq{
            lib  -d
# comment, followed "blank" line (w/ whitespace):

            # indented comment, followed blank line (w/o whitespace):

            lib/File/CheckTree.pm -f
            '$path_to_libFileCheckTree' -e || warn
        };
    };

    diag($_) for @warnings;
    is( $@, '' );
    is( scalar @warnings, 0 );
    is( $num_warnings, 0 );
}


#### TEST 2 -- One warning ####

{
    my ($num_warnings, @warnings);

    local $SIG{__WARN__} = sub { push @warnings, "@_" };

    eval {
        $num_warnings = validate qq{
            lib    -f
            lib/File/CheckTree.pm -f
        };
    };

    is( $@, '' );
    is( scalar @warnings, 1 );
    like( $warnings[0], qr/lib is not a plain file/);
    is( $num_warnings, 1 );
}


#### TEST 3 -- Multiple warnings ####
# including first warning only from a bundle of tests,
# generic "|| warn", default "|| warn" and "|| warn '...' "

{
    my ($num_warnings, @warnings);

    local $SIG{__WARN__} = sub { push @warnings, "@_" };

    eval {
        $num_warnings = validate q{
            lib     -effd
            lib/File/CheckTree.pm -f || die
            lib/File/CheckTree.pm -d || warn
            lib    -f || warn "my warning: $file\n"
        };
    };

    is( $@, '' );
    is( scalar @warnings, 3 );
    like( $warnings[0], qr/lib is not a plain file/);
    like( $warnings[1], qr{lib/File/CheckTree.pm is not a directory});
    like( $warnings[2], qr/my warning: lib/);
    is( $num_warnings, 3 );
}


#### TEST 4 -- cd directive ####
# cd directive followed by relative paths, followed by full paths
{
    my ($num_warnings, @warnings, $path_to_lib, $path_to_dist);
    $path_to_lib  = File::Spec->rel2abs(File::Spec->catdir('lib'));
    $path_to_dist = File::Spec->rel2abs(File::Spec->curdir);

    local $SIG{__WARN__} = sub { push @warnings, "@_" };

    eval {
        $num_warnings = validate qq{
            lib                   -d || die
            '$path_to_lib'        cd
            File                  -e
            File                  -f
            '$path_to_dist'       cd
            lib/File/CheckTree.pm -ef
            lib/File/CheckTree.pm -d || warn
            '$path_to_lib'        -d || die
        };
    };

    is( $@, '' );
    is( scalar @warnings, 2 );
    like( $warnings[0], qr/File is not a plain file/);
    like( $warnings[1], qr/CheckTree\.pm is not a directory/);
    is( $num_warnings, 2 );
}


#### TEST 5 -- Exception ####
# test with generic "|| die"
{
    my $num_warnings;

    eval {
        $num_warnings = validate q{
            lib                    -ef || die
            lib/File/CheckTree.pm  -d
        };
    };

    like($@, qr/lib is not a plain file/);
}


#### TEST 6 -- Exception ####
# test with "|| die 'my error message'"
{
    my $num_warnings;

    eval {
        $num_warnings = validate q{
            lib                    -ef || die "yadda $file yadda...\n"
            lib/File/CheckTree.pm  -d
        };
    };

    like($@, qr/yadda lib yadda/);
    is( $num_warnings, undef );
}

#### TEST 7 -- Quoted file names ####
{
    my $num_warnings;
    eval {
        $num_warnings = validate q{
            "a file with whitespace" !-ef
            'a file with whitespace' !-ef
        };
    };

    is ( $@, '', 'No errors mean we compile correctly');
}

#### TEST 8 -- Malformed query ####
{
    my $num_warnings;
    eval {
        $num_warnings = validate q{
            a file with whitespace !-ef
        };
    };

    like( $@, qr/syntax error/,
          'We got a syntax error for a malformed file query' );
}
