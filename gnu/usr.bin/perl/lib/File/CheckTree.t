#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Test;

BEGIN { plan tests => 6 }

use strict;

use File::CheckTree;
use File::Spec;          # used to get absolute paths

# We assume that we start from the perl "t" directory.
# Will move up one level to make it easier to generate
# reliable pathnames for testing File::CheckTree

chdir(File::Spec->updir) or die "cannot change to parent of t/ directory: $!";


#### TEST 1 -- No warnings ####
# usings both relative and full paths, indented comments

{
    my ($num_warnings, $path_to_README);
    $path_to_README = File::Spec->rel2abs('README');

    my @warnings;
    local $SIG{__WARN__} = sub { push @warnings, "@_" };

    eval {
        $num_warnings = validate qq{
            lib  -d
# comment, followed "blank" line (w/ whitespace):
           
            # indented comment, followed blank line (w/o whitespace):

            README -f
            $path_to_README -e || warn
        };
    };

    if ( !$@ && !@warnings && defined($num_warnings) && $num_warnings == 0 ) {
        ok(1);
    }
    else {
        ok(0);
    }
}


#### TEST 2 -- One warning ####

{
    my ($num_warnings, @warnings);

    local $SIG{__WARN__} = sub { push @warnings, "@_" };

    eval {
        $num_warnings = validate qq{
            lib    -f
            README -f
        };
    };

    if ( !$@ && @warnings == 1
             && $warnings[0] =~ /lib is not a plain file/
             && defined($num_warnings)
             && $num_warnings == 1 )
    {
        ok(1);
    }
    else {
        ok(0);
    }
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
            README -f || die
            README -d || warn
            lib    -f || warn "my warning: $file\n"
        };
    };

    if ( !$@ && @warnings == 3
             && $warnings[0] =~ /lib is not a plain file/
             && $warnings[1] =~ /README is not a directory/
             && $warnings[2] =~ /my warning: lib/
             && defined($num_warnings)
             && $num_warnings == 3 )
    {
        ok(1);
    }
    else {
        ok(0);
    }
}


#### TEST 4 -- cd directive ####
# cd directive followed by relative paths, followed by full paths
{
    my ($num_warnings, @warnings, $path_to_libFile, $path_to_dist);
    $path_to_libFile = File::Spec->rel2abs(File::Spec->catdir('lib','File'));
    $path_to_dist    = File::Spec->rel2abs(File::Spec->curdir);

    local $SIG{__WARN__} = sub { push @warnings, "@_" };

    eval {
        $num_warnings = validate qq{
            lib                -d || die
            $path_to_libFile   cd
            Spec               -e
            Spec               -f
            $path_to_dist      cd
            README             -ef
            INSTALL            -d || warn
            $path_to_libFile   -d || die
        };
    };

    if ( !$@ && @warnings == 2
             && $warnings[0] =~ /Spec is not a plain file/
             && $warnings[1] =~ /INSTALL is not a directory/
             && defined($num_warnings)
             && $num_warnings == 2 )
    {
        ok(1);
    }
    else {
        ok(0);
    }
}


#### TEST 5 -- Exception ####
# test with generic "|| die"
{
    my $num_warnings;

    eval {
        $num_warnings = validate q{
            lib       -ef || die
            README    -d
        };
    };

    if ( $@ && $@ =~ /lib is not a plain file/
            && not defined $num_warnings )
    {
        ok(1);
    }
    else {
        ok(0);
    }
}


#### TEST 6 -- Exception ####
# test with "|| die 'my error message'"
{
    my $num_warnings;

    eval {
        $num_warnings = validate q{
            lib       -ef || die "yadda $file yadda...\n"
            README    -d
        };
    };

    if ( $@ && $@ =~ /yadda lib yadda/
            && not defined $num_warnings )
    {
        ok(1);
    }
    else {
        ok(0);
    }
}
