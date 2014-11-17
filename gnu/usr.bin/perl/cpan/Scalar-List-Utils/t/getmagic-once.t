#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}
use strict;
use Scalar::Util qw(blessed reftype refaddr);
use Test::More tests => 6;

my $getmagic_count;

{
    package T;
    use Tie::Scalar;
    use base qw(Tie::StdScalar);

    sub FETCH {
        $getmagic_count++;
        my($self) = @_;
        return $self->SUPER::FETCH;
    }
}

tie my $var, 'T';

$var = bless {};

$getmagic_count = 0;
ok blessed($var);
is $getmagic_count, 1, 'blessed';

$getmagic_count = 0;
ok reftype($var);
is $getmagic_count, 1, 'reftype';

$getmagic_count = 0;
ok refaddr($var);
is $getmagic_count, 1, 'refaddr';
