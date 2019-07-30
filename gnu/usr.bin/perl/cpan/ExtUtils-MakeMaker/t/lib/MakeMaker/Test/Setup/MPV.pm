package MakeMaker::Test::Setup::MPV;

@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw(setup_recurs teardown_recurs);

use strict;
use File::Path;
use File::Basename;

my %Files = (
             'Min-PerlVers/Makefile.PL'   => <<'END',
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME             => 'Min::PerlVers',
    AUTHOR           => 'John Doe <jd@example.com>',
    VERSION_FROM     => 'lib/Min/PerlVers.pm',
    PREREQ_PM        => { strict => 0 },
    MIN_PERL_VERSION => '5.005',
);
END

             'Min-PerlVers/lib/Min/PerlVers.pm'    => <<'END',
package Min::PerlVers;

$VERSION = 0.05;

=head1 NAME

Min::PerlVers - being picky about perl versions

=cut

1;
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


1;
