#!/usr/bin/perl -w

use strict;

die "$0: EPOCROOT unset\n" unless exists $ENV{EPOCROOT};
die "$0: EPOCROOT directory does exists\n" unless -d $ENV{EPOCROOT};

my $EPOC32 = "$ENV{EPOCROOT}epoc32";
my $EXE = "$EPOC32\\release\\thumb\\urel\\perlapp.app";
my $RSC = "$EPOC32\\data\\z\\system\\apps\\perlapp\\perlapp.rsc";

use Fcntl qw(O_RDONLY);

my %new = ($EXE => 'perlappmin.hex',
	   $RSC => 'perlrscmin.hex');

for my $fn ($EXE, $RSC) {
    if (sysopen(my $fh, $fn, O_RDONLY)) {
	my $buffer;
	my $size = -s $fn;
	my $read;
	my $newfn = $new{$fn};
	unlink($newfn);
	if (($read = sysread($fh, $buffer, $size)) == $size) {
	    if (open(my $newfh, ">$newfn")) {
		binmode($newfh);
		print $newfh unpack("H*", $buffer);
		close($newfh);
		print "Created $newfn\n";
	    } else {
		die qq[$0: open ">$newfn" failed: $!\n];
	    }
	} else {
	    die qq[$0: sysread $size returned $read\n];
	}
	close($fh);
    } else {
	die qq[$0: sysopen "$fn": $!\n];
    }
}

