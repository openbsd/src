#!./perl

print "1..12\n";
sub context {
  my ( $cona, $testnum ) = @_;
  my $conb = (defined wantarray) ? ( wantarray ? 'A' : 'S' ) : 'V';
  unless ( $cona eq $conb ) {
	print "# Context $conb should be $cona\nnot ";
  }
  print "ok $testnum\n";
}

context('V',1);
$a = context('S',2);
@a = context('A',3);
scalar context('S',4);
$a = scalar context('S',5);
($a) = context('A',6);
($a) = scalar context('S',7);

{
  # [ID 20020626.011] incorrect wantarray optimisation
  sub simple { wantarray ? 1 : 2 }
  sub inline {
    my $a = wantarray ? simple() : simple();
    $a;
  }
  my @b = inline();
  my $c = inline();
  print +(@b == 1 && "@b" eq "2") ? "ok 8\n" : "not ok 8\t# <@b>\n";
  print +($c == 2) ? "ok 9\n" : "not ok 9\t# <$c>\n";
}

my $qcontext = q{
  $q = (defined wantarray) ? ( wantarray ? 'A' : 'S' ) : 'V';
};
eval $qcontext;
print $q eq 'V' ? "ok 10\n" : "not ok 10\n";
$a = eval $qcontext;
print $q eq 'S' ? "ok 11\n" : "not ok 11\n";
@a = eval $qcontext;
print $q eq 'A' ? "ok 12\n" : "not ok 12\n";

1;
