#!/usr/bin/perl

use lib '..';
use Memoize;

print "1..7\n";


sub n_null { '' }

{ my $I = 0;
  sub n_diff { $I++ }
}

{ my $I = 0;
  sub a1 { $I++; "$_[0]-$I"  }
  my $J = 0;
  sub a2 { $J++; "$_[0]-$J"  }
  my $K = 0;
  sub a3 { $K++; "$_[0]-$K"  }
}

my $a_normal =  memoize('a1', INSTALL => undef);
my $a_nomemo =  memoize('a2', INSTALL => undef, NORMALIZER => 'n_diff');
my $a_allmemo = memoize('a3', INSTALL => undef, NORMALIZER => 'n_null');

@ARGS = (1, 2, 3, 2, 1);

@res  = map { &$a_normal($_) } @ARGS;
print ((("@res" eq "1-1 2-2 3-3 2-2 1-1") ? '' : 'not '), "ok 1\n");

@res  = map { &$a_nomemo($_) } @ARGS;
print ((("@res" eq "1-1 2-2 3-3 2-4 1-5") ? '' : 'not '), "ok 2\n");

@res = map { &$a_allmemo($_) } @ARGS;
print ((("@res" eq "1-1 1-1 1-1 1-1 1-1") ? '' : 'not '), "ok 3\n");

		
       
# Test fully-qualified name and installation
$COUNT = 0;
sub parity { $COUNT++; $_[0] % 2 }
sub parnorm { $_[0] % 2 }
memoize('parity', NORMALIZER =>  'main::parnorm');
@res = map { &parity($_) } @ARGS;
print ((("@res" eq "1 0 1 0 1") ? '' : 'not '), "ok 4\n");
print (( ($COUNT == 2) ? '' : 'not '), "ok 5\n");

# Test normalization with reference to normalizer function
$COUNT = 0;
sub par2 { $COUNT++; $_[0] % 2 }
memoize('par2', NORMALIZER =>  \&parnorm);
@res = map { &par2($_) } @ARGS;
print ((("@res" eq "1 0 1 0 1") ? '' : 'not '), "ok 6\n");
print (( ($COUNT == 2) ? '' : 'not '), "ok 7\n");


