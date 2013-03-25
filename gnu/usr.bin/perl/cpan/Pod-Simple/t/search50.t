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
BEGIN { plan tests => 8 }

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
print "# (On Debian-based distributions Pod is stripped from\n",
      "# strict.pm, so skip these tests.)\n";
my $nopod = not exists ($name2where->{'strict'});
skip($nopod, ($name2where->{'strict'} || 'huh???'), '/strict\.(pod|pm)$/');

skip($nopod, grep( m/strict\.(pod|pm)/, keys %$where2name ));

my  $strictpath = $name2where->{'strict'};
if( $strictpath ) {
  my @x = ($x->find('strict')||'(nil)', $strictpath);
  print "# Comparing \"$x[0]\" to \"$x[1]\"\n";
  for(@x) { s{[/\\]}{/}g; }
  print "#        => \"$x[0]\" to \"$x[1]\"\n";
  ok $x[0], $x[1], " find('strict') should match survey's name2where{strict}";
} elsif ($nopod) {
  skip "skipping find() for strict.pm"; # skipping find() for 'thatpath/strict.pm
} else {
  ok 0;  # an entry without a defined path means can't test find()
}

print "# Test again on a module we know is present, in case the
strict.pm tests were skipped...\n";

# Grab the first item in $name2where, since it doesn't matter which we
# use.
my $testmod = (keys %$name2where)[0];
my  $testpath = $name2where->{$testmod};
if( $testmod ) {
  my @x = ($x->find($testmod)||'(nil)', $testpath);
  print "# Comparing \"$x[0]\" to \"$x[1]\"\n";
  for(@x) { s{[/\\]}{/}g; }
  # If it finds a .pod, it's probably correct, as that's where the docs are.
  # Change it to .pm so that it matches.
  $x[0] =~ s{[.]pod$}{.pm} if $x[1] =~ m{[.]pm$};
  print "#        => \"$x[0]\" to \"$x[1]\"\n";
  ok
       lc $x[0], 
       lc $x[1], 
       " find('$testmod') should match survey's name2where{$testmod}";
} else {
  ok 0;  # no 'thatpath/<name>.pm' means can't test find()
}

ok 1;
print "# Byebye from ", __FILE__, "\n";
print "# @INC\n";
__END__

