package TestUtils;

use strict;
use warnings;

use Exporter   ();
use File::Spec ();
use File::Find ();

our @ISA    = qw{ Exporter };
our @EXPORT = qw{
    find_tml_files
    json_class
    slurp
    test_data_directory
    test_data_file
};

sub find_tml_files {
    my $dir = shift;
    my @files;
    File::Find::find(
        sub { push @files, $File::Find::name if -f and /\.tml$/ },
        $dir
    );
    return @files;
}

# Prefer JSON to JSON::PP; skip if we don't have at least one
sub json_class {
    for (qw/JSON JSON::PP/) {
        return $_ if eval "require $_; 1";
    }
    return;
}

sub test_data_directory {
    return File::Spec->catdir( 't', 'data' );
}

sub test_data_file {
    return File::Spec->catfile( test_data_directory(), shift );
}

sub slurp {
    my $file = shift;
    local $/ = undef;
    open( FILE, " $file" ) or die "open($file) failed: $!";
    binmode( FILE, $_[0] ) if @_ > 0;
    # binmode(FILE); # disable perl's BOM interpretation
    my $source = <FILE>;
    close( FILE ) or die "close($file) failed: $!";
    $source;
}

1;
