#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

sub BEGIN {
    unshift @INC, 't';
    unshift @INC, 't/compat' if $] < 5.006002;
    require Config; import Config;
    if ($ENV{PERL_CORE} and $Config{'extensions'} !~ /\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
    require 'st-dump.pl';
}


use Storable qw(store retrieve nstore);
use Test::More tests => 14;

$a = 'toto';
$b = \$a;
$c = bless {}, CLASS;
$c->{attribute} = 'attrval';
%a = ('key', 'value', 1, 0, $a, $b, 'cvar', \$c);
@a = ('first', '', undef, 3, -4, -3.14159, 456, 4.5,
	$b, \$a, $a, $c, \$c, \%a);

isnt(store(\@a, 'store'), undef);
is(Storable::last_op_in_netorder(), '');
isnt(nstore(\@a, 'nstore'), undef);
is(Storable::last_op_in_netorder(), 1);
is(Storable::last_op_in_netorder(), 1);

$root = retrieve('store');
isnt($root, undef);
is(Storable::last_op_in_netorder(), '');

$nroot = retrieve('nstore');
isnt($root, undef);
is(Storable::last_op_in_netorder(), 1);

$d1 = &dump($root);
isnt($d1, undef);
$d2 = &dump($nroot);
isnt($d2, undef);

is($d1, $d2);

# Make sure empty string is defined at retrieval time
isnt($root->[1], undef);
is(length $root->[1], 0);

END { 1 while unlink('store', 'nstore') }
