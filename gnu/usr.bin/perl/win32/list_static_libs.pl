#!perl -w
use strict;

# prints libraries for static linking and exits

use Config;

my $out;
if (@ARGV > 1 && $ARGV[0] eq "-o") {
    shift;
    $out = shift;
}

my @statics = split /\s+/, $Config{static_ext};

my (@extralibs, %extralibs); # collect extralibs, preserving their order
for (@statics) {
    my $file = "..\\lib\\auto\\$_\\extralibs.ld";
    open my $fh, '<', $file or die "can't open $file for reading: $!";
    push @extralibs, grep {!$extralibs{$_}++} grep {/\S/} split /\s+/, join '', <$fh>;
}

my @libnames = join " ",
    map {s|/|\\|g;m|([^\\]+)$|;"..\\lib\\auto\\$_\\$1$Config{_a}"} @statics,
    @extralibs;
my $result = join " ", @libnames;

if ($out) {
    my $do_update = 0;
    # only write if:
    #  - output doesn't exist
    #  - there's a change in the content of the file
    #  - one of the generated static libs is newer than the file
    my $out_mtime;
    if (open my $fh, "<", $out) {
        $out_mtime = (stat $fh)[9];
        my $current = do { local $/; <$fh> };
        if ($current ne $result) {
            ++$do_update;
        }
        close $fh;
    }
    else {
        ++$do_update;
    }
    if (!$do_update && $out_mtime) {
        for my $lib (@libnames) {
            if ((stat $lib)[9] > $out_mtime) {
                ++$do_update;
                last;
            }
        }
    }

    unless ($do_update) {
        print "$0: No changes, no libraries changed, nothing to do\n";
        exit;
    }

    open my $fh, ">", $out
        or die "Cannot create $out: $!";
    print $fh $result;
    close $fh or die "Failed to close $out: $!\n";
}
else {
    print $result;
}
