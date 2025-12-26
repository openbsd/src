#!perl -w

use Config;
use Test::More
    $Config{'extensions'} =~ /\bOpcode\b/
	|| $Config{'extensions'} =~ /\bPOSIX\b/
	|| $Config{'osname'} eq 'VMS'
    ? (tests => 2)
    : (skip_all => "no Opcode and POSIX extensions and we're not on VMS");

use strict;
use warnings;
use POSIX qw(ceil);
use Safe;

my $safe = Safe->new;
$safe->deny('add');

my $masksize = ceil( Opcode::opcodes / 8 );
# Attempt to change the opmask from within the safe compartment
$safe->reval( qq{\$_[1] = qq/\0/ x } . $masksize );

# Check that it didn't work
$safe->reval( q{$x + $y} );
# Written this way to keep the Test::More that comes with perl 5.6.2 happy
ok( $@ =~ /^'?addition \(\+\)'? trapped by operation mask/,
	    'opmask still in place with reval' );

my $safe2 = Safe->new;
$safe2->deny('add');

open my $fh, '>nasty.pl' or die "Can't write nasty.pl: $!\n";
print $fh <<EOF;
\$_[1] = "\0" x $masksize;
EOF
close $fh;
$safe2->rdo('./nasty.pl');
$safe2->reval( q{$x + $y} );
# Written this way to keep the Test::More that comes with perl 5.6.2 happy
ok( $@ =~ /^'?addition \(\+\)'? trapped by operation mask/,
	    'opmask still in place with rdo' );
END { unlink 'nasty.pl' }
