package STTestLib;
use strict;
use warnings;

use Exporter;
*import = \&Exporter::import;

our @EXPORT_OK = qw(slurp write_and_retrieve tempfilename);

use Storable qw(retrieve);
use File::Temp qw(tempfile);

sub slurp {
    my $file = shift;
    open my $fh, "<", $file or die "Can't open '$file': $!";
    binmode $fh;
    my $contents = do { local $/; <$fh> };
    die "Can't read $file: $!" unless defined $contents;
    return $contents;
}

sub write_and_retrieve {
    my $data = shift;

    my ($fh, $filename) = tempfile('storable-testfile-XXXXX', TMPDIR => 1, UNLINK => 1);
    binmode $fh;
    print $fh $data or die "Can't print to '$filename': $!";
    close $fh or die "Can't close '$filename': $!";

    return eval { retrieve $filename };
}

sub tempfilename {
    local $^W;
    my (undef, $file) = tempfile('storable-testfile-XXXXX', TMPDIR => 1, UNLINK => 1);
    return $file;
}

1;
