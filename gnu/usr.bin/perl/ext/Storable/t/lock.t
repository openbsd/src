#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
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

use Storable qw(lock_store lock_retrieve);

unless (&Storable::CAN_FLOCK) {
    print "1..0 # Skip: fcntl/flock emulation broken on this platform\n";
	exit 0;
}

print "1..5\n";

@a = ('first', undef, 3, -4, -3.14159, 456, 4.5);

#
# We're just ensuring things work, we're not validating locking.
#

ok 1, defined lock_store(\@a, 'store');
ok 2, $dumped = &dump(\@a);

$root = lock_retrieve('store');
ok 3, ref $root eq 'ARRAY';
ok 4, @a == @$root;
ok 5, &dump($root) eq $dumped; 

unlink 't/store';

