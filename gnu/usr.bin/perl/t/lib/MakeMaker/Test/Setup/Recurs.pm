package MakeMaker::Test::Setup::Recurs;

@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw(setup_recurs teardown_recurs);

use strict;
use File::Path;
use File::Basename;

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
