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


use Scalar::Util qw(reftype);
use vars qw($t $y $x *F);
use Symbol qw(gensym);

# Ensure we do not trigger and tied methods
tie *F, 'MyTie';

@test = (
 [ undef, 1],
 [ undef, 'A'],
 [ HASH => {} ],
 [ ARRAY => [] ],
 [ SCALAR => \$t ],
 [ REF    => \(\$t) ],
 [ GLOB   => \*F ],
 [ GLOB   => gensym ],
 [ CODE   => sub {} ],
# [ IO => *STDIN{IO} ] the internal sv_reftype returns UNKNOWN
);

print "1..", @test*4, "\n";

my $i = 1;
foreach $test (@test) {
  my($type,$what) = @$test;
  my $pack;
  foreach $pack (undef,"ABC","0",undef) {
    print "# $what\n";
    my $res = reftype($what);
    printf "# %s - %s\n", map { defined($_) ? $_ : 'undef' } $type,$res;
    print "not " if $type ? $res ne $type : defined($res);
    bless $what, $pack if $type && defined $pack;
    print "ok ",$i++,"\n";
  }
}

package MyTie;

sub TIEHANDLE { bless {} }
sub DESTROY {}

sub AUTOLOAD {
  warn "$AUTOLOAD called";
  exit 1; # May be in an eval
}
