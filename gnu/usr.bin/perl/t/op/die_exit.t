#!./perl

#
# Verify that C<die> return the return code
#	-- Robin Barker <rmb@cise.npl.co.uk>
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

if ($^O eq 'mpeix') {
    print "1..0 # Skip: broken on MPE/iX\n";
    exit 0;
}

$| = 1;

use strict;

my %tests = (
	 1 => [   0,   0],
	 2 => [   0,   1], 
	 3 => [   0, 127], 
	 4 => [   0, 128], 
	 5 => [   0, 255], 
	 6 => [   0, 256], 
	 7 => [   0, 512], 
	 8 => [   1,   0],
	 9 => [   1,   1],
	10 => [   1, 256],
	11 => [ 128,   0],
	12 => [ 128,   1],
	13 => [ 128, 256],
	14 => [ 255,   0],
	15 => [ 255,   1],
	16 => [ 255, 256],
	# see if implicit close preserves $?
	17 => [  0,  512, '{ local *F; open F, q[TEST]; close F; $!=0 } die;'],
);

my $max = keys %tests;

print "1..$max\n";

# Dump any error messages from the dying processes off to a temp file.
open(STDERR, ">die_exit.err") or die "Can't open temp error file:  $!";

foreach my $test (1 .. $max) {
    my($bang, $query, $code) = @{$tests{$test}};
    $code ||= 'die;';
    if ($^O eq 'MSWin32' || $^O eq 'NetWare' || $^O eq 'VMS') {
        system(qq{$^X -e "\$! = $bang; \$? = $query; $code"});
    }
    else {
        system(qq{$^X -e '\$! = $bang; \$? = $query; $code'});
    }
    my $exit = $?;

    # VMS exit code 44 (SS$_ABORT) is returned if a program dies.  We only get
    # the severity bits, which boils down to 4.  See L<perlvms/$?>.
    $bang = 4 if $^O eq 'VMS';

    printf "# 0x%04x  0x%04x  0x%04x\n", $exit, $bang, $query;
    print "not " unless $exit == (($bang || ($query >> 8) || 255) << 8);
    print "ok $test\n";
}
    
close STDERR;
END { 1 while unlink 'die_exit.err' }

