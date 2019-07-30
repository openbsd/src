#!perl -w
use strict;

# prints libraries for static linking and exits

use Config;

my @statics = split /\s+/, $Config{static_ext};

my %extralibs;
for (@statics) {
    my $file = "..\\lib\\auto\\$_\\extralibs.ld";
    open my $fh, '<', $file or die "can't open $file for reading: $!";
    $extralibs{$_}++ for grep {/\S/} split /\s+/, join '', <$fh>;
}
print map {s|/|\\|g;m|([^\\]+)$|;"..\\lib\\auto\\$_\\$1$Config{_a} "} @statics;
print map {"$_ "} sort keys %extralibs;
