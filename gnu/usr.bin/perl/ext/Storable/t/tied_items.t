#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

#
# Tests ref to items in tied hash/array structures.
#

sub BEGIN {
    if ($ENV{PERL_CORE}){
	chdir('t') if -d 't';
	@INC = ('.', '../lib', '../ext/Storable/t');
    } else {
	unshift @INC, 't';
    }
    require Config; import Config;
    if ($ENV{PERL_CORE} and $Config{'extensions'} !~ /\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
    require 'st-dump.pl';
}

sub ok;
$^W = 0;

print "1..8\n";

use Storable qw(dclone);

$h_fetches = 0;

sub H::TIEHASH { bless \(my $x), "H" }
sub H::FETCH { $h_fetches++; $_[1] - 70 }

tie %h, "H";

$ref = \$h{77};
$ref2 = dclone $ref;

ok 1, $h_fetches == 0;
ok 2, $$ref2 eq $$ref;
ok 3, $$ref2 == 7;
ok 4, $h_fetches == 2;

$a_fetches = 0;

sub A::TIEARRAY { bless \(my $x), "A" }
sub A::FETCH { $a_fetches++; $_[1] - 70 }

tie @a, "A";

$ref = \$a[78];
$ref2 = dclone $ref;

ok 5, $a_fetches == 0;
ok 6, $$ref2 eq $$ref;
ok 7, $$ref2 == 8;
# I don't understand why it's 3 and not 2
ok 8, $a_fetches == 3;
