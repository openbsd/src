#!./perl


BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

my %seen;

package Implement;

sub TIEARRAY
{
 $seen{'TIEARRAY'}++;
 my ($class,@val) = @_;
 return bless \@val,$class;
}

sub STORESIZE
{        
 $seen{'STORESIZE'}++;
 my ($ob,$sz) = @_; 
 return $#{$ob} = $sz-1;
}

sub EXTEND
{        
 $seen{'EXTEND'}++;
 my ($ob,$sz) = @_; 
 return @$ob = $sz;
}

sub FETCHSIZE
{        
 $seen{'FETCHSIZE'}++;
 return scalar(@{$_[0]});
}

sub FETCH
{
 $seen{'FETCH'}++;
 my ($ob,$id) = @_;
 return $ob->[$id]; 
}

sub STORE
{
 $seen{'STORE'}++;
 my ($ob,$id,$val) = @_;
 $ob->[$id] = $val; 
}                 

sub UNSHIFT
{
 $seen{'UNSHIFT'}++;
 my $ob = shift;
 unshift(@$ob,@_);
}                 

sub PUSH
{
 $seen{'PUSH'}++;
 my $ob = shift;;
 push(@$ob,@_);
}                 

sub CLEAR
{
 $seen{'CLEAR'}++;
 @{$_[0]} = ();
}

sub DESTROY
{
 $seen{'DESTROY'}++;
}

sub POP
{
 $seen{'POP'}++;
 my ($ob) = @_;
 return pop(@$ob);
}

sub SHIFT
{
 $seen{'SHIFT'}++;
 my ($ob) = @_;
 return shift(@$ob);
}

sub SPLICE
{
 $seen{'SPLICE'}++;
 my $ob  = shift;                    
 my $off = @_ ? shift : 0;
 my $len = @_ ? shift : @$ob-1;
 return splice(@$ob,$off,$len,@_);
}

package NegIndex;               # 20020220 MJD
@ISA = 'Implement';

# simulate indices -2 .. 2
my $offset = 2;
$NegIndex::NEGATIVE_INDICES = 1;

sub FETCH {
  my ($ob,$id) = @_;
#  print "# FETCH @_\n";
  $id += $offset;
  $ob->[$id];
}

sub STORE {
  my ($ob,$id,$value) = @_;
#  print "# STORE @_\n";
  $id += $offset;
  $ob->[$id] = $value;
}

sub DELETE {
  my ($ob,$id) = @_;
#  print "# DELETE @_\n";
  $id += $offset;
  delete $ob->[$id];
}

sub EXISTS {
  my ($ob,$id) = @_;
#  print "# EXISTS @_\n";
  $id += $offset;
  exists $ob->[$id];
}

#
# Returning -1 from FETCHSIZE used to get casted to U32 causing a
# segfault
#

package NegFetchsize;

sub TIEARRAY  { bless [] }
sub FETCH     { }
sub FETCHSIZE { -1 }

package main;
  
print "1..66\n";                   
my $test = 1;

{my @ary;

{ my $ob = tie @ary,'Implement',3,2,1;
  print "not " unless $ob;
  print "ok ", $test++,"\n";
  print "not " unless tied(@ary) == $ob;
  print "ok ", $test++,"\n";
}


print "not " unless @ary == 3;
print "ok ", $test++,"\n";

print "not " unless $#ary == 2;
print "ok ", $test++,"\n";

print "not " unless join(':',@ary) eq '3:2:1';
print "ok ", $test++,"\n";         

print "not " unless $seen{'FETCH'} >= 3;
print "ok ", $test++,"\n";

@ary = (1,2,3);

print "not " unless $seen{'STORE'} >= 3;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '1:2:3';
print "ok ", $test++,"\n";         

{my @thing = @ary;
print "not " unless join(':',@thing) eq '1:2:3';
print "ok ", $test++,"\n";         

tie @thing,'Implement';
@thing = @ary;
print "not " unless join(':',@thing) eq '1:2:3';
print "ok ", $test++,"\n";
} 

print "not " unless pop(@ary) == 3;
print "ok ", $test++,"\n";
print "not " unless $seen{'POP'} == 1;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '1:2';
print "ok ", $test++,"\n";

push(@ary,4);
print "not " unless $seen{'PUSH'} == 1;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '1:2:4';
print "ok ", $test++,"\n";

my @x = splice(@ary,1,1,7);


print "not " unless $seen{'SPLICE'} == 1;
print "ok ", $test++,"\n";

print "not " unless @x == 1;
print "ok ", $test++,"\n";
print "not " unless $x[0] == 2;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '1:7:4';
print "ok ", $test++,"\n";             

print "not " unless shift(@ary) == 1;
print "ok ", $test++,"\n";
print "not " unless $seen{'SHIFT'} == 1;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '7:4';
print "ok ", $test++,"\n";             

my $n = unshift(@ary,5,6);
print "not " unless $seen{'UNSHIFT'} == 1;
print "ok ", $test++,"\n";
print "not " unless $n == 4;
print "ok ", $test++,"\n";
print "not " unless join(':',@ary) eq '5:6:7:4';
print "ok ", $test++,"\n";

@ary = split(/:/,'1:2:3');
print "not " unless join(':',@ary) eq '1:2:3';
print "ok ", $test++,"\n";         

my $t = 0;
foreach $n (@ary)
 {
  print "not " unless $n == ++$t;
  print "ok ", $test++,"\n";         
 }

# (30-33) 20020303 mjd-perl-patch+@plover.com
@ary = ();
$seen{POP} = 0;
pop @ary;                       # this didn't used to call POP at all
print "not " unless $seen{POP} == 1;
print "ok ", $test++,"\n";         
$seen{SHIFT} = 0;
shift @ary;                     # this didn't used to call SHIFT at  all
print "not " unless $seen{SHIFT} == 1;
print "ok ", $test++,"\n";         
$seen{PUSH} = 0;
push @ary;                       # this didn't used to call PUSH at all
print "not " unless $seen{PUSH} == 1;
print "ok ", $test++,"\n";         
$seen{UNSHIFT} = 0;
unshift @ary;                   # this didn't used to call UNSHIFT at all
print "not " unless $seen{UNSHIFT} == 1;
print "ok ", $test++,"\n";         

@ary = qw(3 2 1);
print "not " unless join(':',@ary) eq '3:2:1';
print "ok ", $test++,"\n";         

$#ary = 1;
print "not " unless $seen{'STORESIZE'} == 1;
print "ok ", $test++," -- seen STORESIZE\n";
print "not " unless join(':',@ary) eq '3:2';
print "ok ", $test++,"\n";

sub arysize :lvalue { $#ary }
arysize()--;
print "not " unless $seen{'STORESIZE'} == 2;
print "ok ", $test++," -- seen STORESIZE\n";
print "not " unless join(':',@ary) eq '3';
print "ok ", $test++,"\n";

untie @ary;   

}

# 20020401 mjd-perl-patch+@plover.com
# Thanks to Dave Mitchell for the small test case and the fix
{
  my @a;
  
  sub X::TIEARRAY { bless {}, 'X' }

  sub X::SPLICE {
    do '/dev/null';
    die;
  }

  tie @a, 'X';
  eval { splice(@a) };
  # If we survived this far.
  print "ok ", $test++, "\n";
}


{ # 20020220 mjd-perl-patch+@plover.com
  my @n;
  tie @n => 'NegIndex', ('A' .. 'E');

  # FETCH
  print "not " unless $n[0] eq 'C';
  print "ok ", $test++,"\n";
  print "not " unless $n[1] eq 'D';
  print "ok ", $test++,"\n";
  print "not " unless $n[2] eq 'E';
  print "ok ", $test++,"\n";
  print "not " unless $n[-1] eq 'B';
  print "ok ", $test++,"\n";
  print "not " unless $n[-2] eq 'A';
  print "ok ", $test++,"\n";

  # STORE
  $n[-2] = 'a';
  print "not " unless $n[-2] eq 'a';
  print "ok ", $test++,"\n";
  $n[-1] = 'b';
  print "not " unless $n[-1] eq 'b';
  print "ok ", $test++,"\n";
  $n[0] = 'c';
  print "not " unless $n[0] eq 'c';
  print "ok ", $test++,"\n";
  $n[1] = 'd';
  print "not " unless $n[1] eq 'd';
  print "ok ", $test++,"\n";
  $n[2] = 'e';
  print "not " unless $n[2] eq 'e';
  print "ok ", $test++,"\n";

  # DELETE and EXISTS
  for (-2 .. 2) {
    print exists($n[$_]) ? "ok $test\n" : "not ok $test\n";
    $test++;
    delete $n[$_];
    print defined($n[$_]) ? "not ok $test\n" : "ok $test\n";
    $test++;
    print exists($n[$_]) ? "not ok $test\n" : "ok $test\n";
    $test++;
  }
}
                           

                           
{
    tie my @dummy, "NegFetchsize";
    eval { "@dummy"; };
    print "# $@" if $@;
    print "not " unless $@ =~ /^FETCHSIZE returned a negative value/;
    print "ok ", $test++, " - croak on negative FETCHSIZE\n";
}

print "not " unless $seen{'DESTROY'} == 3;
print "ok ", $test++,"\n";         

