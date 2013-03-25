#!./perl

print "1..7\n";

# Tests bug #22977.  Test case from Dave Mitchell.
sub f ($);
sub f ($) {
my $test = $_[0];
write;
format STDOUT =
ok @<<<<<<<
$test
.
}

f(1);
f(2);

# A bug caused by the fix for #22977/50528
sub foo {
  sub bar {
    # Fill the pad with alphabet soup, to give the closed-over variable a
    # high padoffset (more likely to trigger the bug and crash).
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    my $x;
    format STDOUT2 =
@<<<<<<
"ok 3".$x # $x is not available, but this should not crash
.
  }
}
*STDOUT = *STDOUT2{FORMAT};
undef *bar;
write;

# A regression introduced in 5.10; format cloning would close over the
# variables in the currently-running sub (the main CV in this test) if the
# outer sub were an inactive closure.
sub baz {
  my $a;
  sub {
    $a;
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t)}
    my $x;
    format STDOUT3 =
@<<<<<<<<<<<<<<<<<<<<<<<<<
defined $x ? "not ok 4 - $x" : "ok 4"
.
  }
}
*STDOUT = *STDOUT3{FORMAT};
{
  local $^W = 1;
  my $w;
  local $SIG{__WARN__} = sub { $w = shift };
  write;
  print "not " unless $w =~ /^Variable "\$x" is not available at/;
  print "ok 5 - closure var not available when outer sub is inactive\n";
}

# Cloning a format whose outside has been undefined
sub x {
    {my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$t,$u)}
    my $z;
    format STDOUT6 =
@<<<<<<<<<<<<<<<<<<<<<<<<<
defined $z ? "not ok 6 - $z" : "ok 6"
.
}
undef &x;
*STDOUT = *STDOUT6{FORMAT};
{
  local $^W = 1;
  my $w;
  local $SIG{__WARN__} = sub { $w = shift };
  write;
  print "not " unless $w =~ /^Variable "\$z" is not available at/;
  print "ok 7 - closure var not available when outer sub is undefined\n";
}
