BEGIN {
    eval "use Test::More";
    if ($@) {
	print "1..0 # SKIPPED: Test::More not installed.\n";
	exit;
    }
}

use File::Spec;
use File::Find;
use strict;

eval "use Test::Pod 0.95";

if ($@) {
    plan skip_all => "Test::Pod v0.95 required for testing POD";
} else {
    my @files;
    my $blib = File::Spec->catfile(qw(blib lib));
    find( sub {push @files, $File::Find::name if /\.p(l|m|od)$/}, $blib);
    plan tests => scalar @files;
    Test::Pod::pod_file_ok($_) foreach @files;
}
