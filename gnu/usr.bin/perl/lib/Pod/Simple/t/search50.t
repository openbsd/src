BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        use File::Spec;
        @INC = (File::Spec->rel2abs('../lib') );
    }
}
use strict;

#sub Pod::Simple::Search::DEBUG () {5};

use Pod::Simple::Search;
use Test;
BEGIN { plan tests => 7 }

print "#  Test the scanning of the whole of \@INC ...\n";

my $x = Pod::Simple::Search->new;
die "Couldn't make an object!?" unless ok defined $x;
ok $x->inc; # make sure inc=1 is the default
print $x->_state_as_string;
#$x->verbose(12);

use Pod::Simple;
*pretty = \&Pod::Simple::BlackBox::pretty;

my $found = 0;
$x->callback(sub {
  print "#  ", join("  ", map "{$_}", @_), "\n";
  ++$found;
  return;
});

print "# \@INC == @INC\n";

my $t = time();   my($name2where, $where2name) = $x->survey();
$t = time() - $t;
ok $found;

print "# Found $found items in $t seconds!\n# See...\n";

my $p = pretty( $where2name, $name2where )."\n";
$p =~ s/, +/,\n/g;
$p =~ s/^/#  /mg;
print $p;

print "# OK, making sure strict and strict.pm were in there...\n";
ok( ($name2where->{'strict'} || 'huh???'), '/strict\.(pod|pm)$/');

ok grep( m/strict\.(pod|pm)/, keys %$where2name );

my  $strictpath = $name2where->{'strict'};
if( $strictpath ) {
  my @x = ($x->find('strict')||'(nil)', $strictpath);
  print "# Comparing \"$x[0]\" to \"$x[1]\"\n";
  for(@x) { s{[/\\]}{/}g; }
  print "#        => \"$x[0]\" to \"$x[1]\"\n";
  ok $x[0], $x[1], " find('strict') should match survey's name2where{strict}";
} else {
  ok 0;  # no 'thatpath/strict.pm' means can't test find()
}

ok 1;
print "# Byebye from ", __FILE__, "\n";
print "# @INC\n";
__END__

