package MakeMaker::Test::Setup::SAS;

@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw(setup_recurs teardown_recurs);

use strict;
use File::Path;
use File::Basename;

our $dirname='Multiple-Authors';
my %Files = (
             $dirname.'/Makefile.PL'   => <<'END',
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME             => 'Multiple::Authors',
    AUTHOR           => ['John Doe <jd@example.com>', 'Jane Doe <jd@example.com>'],
    VERSION_FROM     => 'lib/Multiple/Authors.pm',
    PREREQ_PM        => { strict => 0 },
);
END

             $dirname.'/lib/Multiple/Authors.pm'    => <<'END',
package Multiple::Authors;

$VERSION = 0.05;

=head1 NAME

Multiple::Authors - several authors

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
