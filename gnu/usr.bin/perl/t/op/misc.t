#!./perl

chdir 't' if -d 't';
@INC = "../lib";
$ENV{PERL5LIB} = "../lib";

$|=1;

undef $/;
@prgs = split "\n########\n", <DATA>;
print "1..", scalar @prgs, "\n";

$tmpfile = "misctmp000";
1 while -f ++$tmpfile;
END { unlink $tmpfile if $tmpfile; }

for (@prgs){
    my $switch;
    if (s/^\s*-\w+//){
	$switch = $&;
    }
    my($prog,$expected) = split(/\nEXPECT\n/, $_);
    open TEST, "| sh -c './perl $switch' >$tmpfile 2>&1";
    print TEST $prog, "\n";
    close TEST;
    $status = $?;
    $results = `cat $tmpfile`;
    $results =~ s/\n+$//;
    $expected =~ s/\n+$//;
    if ( $results ne $expected){
	print STDERR "PROG: $switch\n$prog\n";
	print STDERR "EXPECTED:\n$expected\n";
	print STDERR "GOT:\n$results\n";
	print "not ";
    }
    print "ok ", ++$i, "\n";
}

__END__
$foo=undef; $foo->go;
EXPECT
Can't call method "go" without a package or object reference at - line 1.
########
BEGIN
        {
	    "foo";
        }
########
$array[128]=1
########
$x=0x0eabcd; print $x->ref;
EXPECT
Can't call method "ref" without a package or object reference at - line 1.
########
chop ($str .= <STDIN>);
########
close ($banana);
########
$x=2;$y=3;$x<$y ? $x : $y += 23;print $x;
EXPECT
25
########
eval {sub bar {print "In bar";}}
########
system "./perl -ne 'print if eof' /dev/null"
########
chop($file = <>);
########
package N;
sub new {my ($obj,$n)=@_; bless \$n}  
$aa=new N 1;
$aa=12345;
print $aa;
EXPECT
12345
########
%@x=0;
EXPECT
Can't coerce HASH to string in repeat at - line 1.
########
$_="foo";
printf(STDOUT "%s\n", $_);
EXPECT
foo
########
push(@a, 1, 2, 3,)
########
quotemeta ""
########
for ("ABCDE") {
 &sub;
s/./&sub($&)/eg;
print;}
sub sub {local($_) = @_;
$_ x 4;}
EXPECT
Modification of a read-only value attempted at - line 3.
########
package FOO;sub new {bless {FOO => BAR}};
package main;
use strict vars;   
my $self = new FOO;
print $$self{FOO};
EXPECT
BAR
########
$_="foo";
s/.{1}//s;
print;
EXPECT
oo
########
print scalar ("foo","bar")
EXPECT
bar
########
sub by_number { $a <=> $b; };# inline function for sort below
$as_ary{0}="a0";
@ordered_array=sort by_number keys(%as_ary);
########
sub NewShell
{
  local($Host) = @_;
  my($m2) = $#Shells++;
  $Shells[$m2]{HOST} = $Host;
  return $m2;
}
 
sub ShowShell
{
  local($i) = @_;
}
 
&ShowShell(&NewShell(beach,Work,"+0+0"));
&ShowShell(&NewShell(beach,Work,"+0+0"));
&ShowShell(&NewShell(beach,Work,"+0+0"));
########
   {
       package FAKEARRAY;
   
       sub TIEARRAY
       { print "TIEARRAY @_\n"; 
         die "bomb out\n" unless $count ++ ;
         bless ['foo'] 
       }
       sub FETCH { print "fetch @_\n"; $_[0]->[$_[1]] }
       sub STORE { print "store @_\n"; $_[0]->[$_[1]] = $_[2] }
       sub DESTROY { print "DESTROY \n"; undef @{$_[0]}; }
   }
   
eval 'tie @h, FAKEARRAY, fred' ;
tie @h, FAKEARRAY, fred ;
EXPECT
TIEARRAY FAKEARRAY fred
TIEARRAY FAKEARRAY fred
DESTROY 
########
BEGIN { die "phooey\n" }
EXPECT
phooey
BEGIN failed--compilation aborted at - line 1.
########
BEGIN { 1/$zero }
EXPECT
Illegal division by zero at - line 1.
BEGIN failed--compilation aborted at - line 1.
########
BEGIN { undef = 0 }
EXPECT
Modification of a read-only value attempted at - line 1.
BEGIN failed--compilation aborted at - line 1.
