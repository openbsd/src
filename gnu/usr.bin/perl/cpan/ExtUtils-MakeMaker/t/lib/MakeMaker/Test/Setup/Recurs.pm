package MakeMaker::Test::Setup::Recurs;

@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw(setup_recurs teardown_recurs);

use strict;
use File::Path;
use File::Basename;
use MakeMaker::Test::Utils;

my %Files = (
             'Recurs/Makefile.PL'          => <<'END',
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME          => 'Recurs',
    VERSION       => 1.00,
);
END

             'Recurs/prj2/Makefile.PL'     => <<'END',
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME => 'Recurs::prj2',
    VERSION => 1.00,
);
END

             # Check if a test failure in a subdir causes make test to fail
             'Recurs/prj2/t/fail.t'         => <<'END',
#!/usr/bin/perl -w

print "1..1\n";
print "not ok 1\n";
END
            );

sub setup_recurs {

    while(my($file, $text) = each %Files) {
        # Convert to a relative, native file path.
        $file = File::Spec->catfile(File::Spec->curdir, split m{\/}, $file);

        my $dir = dirname($file);
        mkpath $dir;
        open(FILE, ">$file") || die "Can't create $file: $!";
        print FILE $text;
        close FILE;

        # ensure file at least 1 second old for makes that assume
        # files with the same time are out of date.
        my $time = calibrate_mtime();
        utime $time, $time - 1, $file;
    }

    return 1;
}

sub teardown_recurs {
    foreach my $file (keys %Files) {
        my $dir = dirname($file);
        if( -e $dir ) {
            rmtree($dir) || return;
        }
    }
    return 1;
}


1;
