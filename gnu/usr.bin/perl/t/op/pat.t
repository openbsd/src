#!./perl
#
# This is a home for regular expression tests that don't fit into
# the format supported by op/regexp.t.  If you want to add a test
# that does fit that format, add it to op/re_tests, not here.

$| = 1;

print "1..1055\n";

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

eval 'use Config';          #  Defaults assumed if this fails

$x = "abc\ndef\n";

if ($x =~ /^abc/) {print "ok 1\n";} else {print "not ok 1\n";}
if ($x !~ /^def/) {print "ok 2\n";} else {print "not ok 2\n";}

$* = 1;
if ($x =~ /^def/) {print "ok 3\n";} else {print "not ok 3\n";}
$* = 0;

$_ = '123';
if (/^([0-9][0-9]*)/) {print "ok 4\n";} else {print "not ok 4\n";}

if ($x =~ /^xxx/) {print "not ok 5\n";} else {print "ok 5\n";}
if ($x !~ /^abc/) {print "not ok 6\n";} else {print "ok 6\n";}

if ($x =~ /def/) {print "ok 7\n";} else {print "not ok 7\n";}
if ($x !~ /def/) {print "not ok 8\n";} else {print "ok 8\n";}

if ($x !~ /.def/) {print "ok 9\n";} else {print "not ok 9\n";}
if ($x =~ /.def/) {print "not ok 10\n";} else {print "ok 10\n";}

if ($x =~ /\ndef/) {print "ok 11\n";} else {print "not ok 11\n";}
if ($x !~ /\ndef/) {print "not ok 12\n";} else {print "ok 12\n";}

$_ = 'aaabbbccc';
if (/(a*b*)(c*)/ && $1 eq 'aaabbb' && $2 eq 'ccc') {
	print "ok 13\n";
} else {
	print "not ok 13\n";
}
if (/(a+b+c+)/ && $1 eq 'aaabbbccc') {
	print "ok 14\n";
} else {
	print "not ok 14\n";
}

if (/a+b?c+/) {print "not ok 15\n";} else {print "ok 15\n";}

$_ = 'aaabccc';
if (/a+b?c+/) {print "ok 16\n";} else {print "not ok 16\n";}
if (/a*b+c*/) {print "ok 17\n";} else {print "not ok 17\n";}

$_ = 'aaaccc';
if (/a*b?c*/) {print "ok 18\n";} else {print "not ok 18\n";}
if (/a*b+c*/) {print "not ok 19\n";} else {print "ok 19\n";}

$_ = 'abcdef';
if (/bcd|xyz/) {print "ok 20\n";} else {print "not ok 20\n";}
if (/xyz|bcd/) {print "ok 21\n";} else {print "not ok 21\n";}

if (m|bc/*d|) {print "ok 22\n";} else {print "not ok 22\n";}

if (/^$_$/) {print "ok 23\n";} else {print "not ok 23\n";}

$* = 1;		# test 3 only tested the optimized version--this one is for real
if ("ab\ncd\n" =~ /^cd/) {print "ok 24\n";} else {print "not ok 24\n";}
$* = 0;

$XXX{123} = 123;
$XXX{234} = 234;
$XXX{345} = 345;

@XXX = ('ok 25','not ok 25', 'ok 26','not ok 26','not ok 27');
while ($_ = shift(@XXX)) {
    ?(.*)? && (print $1,"\n");
    /not/ && reset;
    /not ok 26/ && reset 'X';
}

while (($key,$val) = each(%XXX)) {
    print "not ok 27\n";
    exit;
}

print "ok 27\n";

'cde' =~ /[^ab]*/;
'xyz' =~ //;
if ($& eq 'xyz') {print "ok 28\n";} else {print "not ok 28\n";}

$foo = '[^ab]*';
'cde' =~ /$foo/;
'xyz' =~ //;
if ($& eq 'xyz') {print "ok 29\n";} else {print "not ok 29\n";}

$foo = '[^ab]*';
'cde' =~ /$foo/;
'xyz' =~ /$null/;
if ($& eq 'xyz') {print "ok 30\n";} else {print "not ok 30\n";}

$_ = 'abcdefghi';
/def/;		# optimized up to cmd
if ("$`:$&:$'" eq 'abc:def:ghi') {print "ok 31\n";} else {print "not ok 31\n";}

/cde/ + 0;	# optimized only to spat
if ("$`:$&:$'" eq 'ab:cde:fghi') {print "ok 32\n";} else {print "not ok 32\n";}

/[d][e][f]/;	# not optimized
if ("$`:$&:$'" eq 'abc:def:ghi') {print "ok 33\n";} else {print "not ok 33\n";}

$_ = 'now is the {time for all} good men to come to.';
/ {([^}]*)}/;
if ($1 eq 'time for all') {print "ok 34\n";} else {print "not ok 34 $1\n";}

$_ = 'xxx {3,4}  yyy   zzz';
print /( {3,4})/ ? "ok 35\n" : "not ok 35\n";
print $1 eq '   ' ? "ok 36\n" : "not ok 36\n";
print /( {4,})/ ? "not ok 37\n" : "ok 37\n";
print /( {2,3}.)/ ? "ok 38\n" : "not ok 38\n";
print $1 eq '  y' ? "ok 39\n" : "not ok 39\n";
print /(y{2,3}.)/ ? "ok 40\n" : "not ok 40\n";
print $1 eq 'yyy ' ? "ok 41\n" : "not ok 41\n";
print /x {3,4}/ ? "not ok 42\n" : "ok 42\n";
print /^xxx {3,4}/ ? "not ok 43\n" : "ok 43\n";

$_ = "now is the time for all good men to come to.";
@words = /(\w+)/g;
print join(':',@words) eq "now:is:the:time:for:all:good:men:to:come:to"
    ? "ok 44\n"
    : "not ok 44\n";

@words = ();
while (/\w+/g) {
    push(@words, $&);
}
print join(':',@words) eq "now:is:the:time:for:all:good:men:to:come:to"
    ? "ok 45\n"
    : "not ok 45\n";

@words = ();
pos = 0;
while (/to/g) {
    push(@words, $&);
}
print join(':',@words) eq "to:to"
    ? "ok 46\n"
    : "not ok 46 `@words'\n";

pos $_ = 0;
@words = /to/g;
print join(':',@words) eq "to:to"
    ? "ok 47\n"
    : "not ok 47 `@words'\n";

$_ = "abcdefghi";

$pat1 = 'def';
$pat2 = '^def';
$pat3 = '.def.';
$pat4 = 'abc';
$pat5 = '^abc';
$pat6 = 'abc$';
$pat7 = 'ghi';
$pat8 = '\w*ghi';
$pat9 = 'ghi$';

$t1=$t2=$t3=$t4=$t5=$t6=$t7=$t8=$t9=0;

for $iter (1..5) {
    $t1++ if /$pat1/o;
    $t2++ if /$pat2/o;
    $t3++ if /$pat3/o;
    $t4++ if /$pat4/o;
    $t5++ if /$pat5/o;
    $t6++ if /$pat6/o;
    $t7++ if /$pat7/o;
    $t8++ if /$pat8/o;
    $t9++ if /$pat9/o;
}

$x = "$t1$t2$t3$t4$t5$t6$t7$t8$t9";
print $x eq '505550555' ? "ok 48\n" : "not ok 48 $x\n";

$xyz = 'xyz';
print "abc" =~ /^abc$|$xyz/ ? "ok 49\n" : "not ok 49\n";

# perl 4.009 says "unmatched ()"
eval '"abc" =~ /a(bc$)|$xyz/; $result = "$&:$1"';
print $@ eq "" ? "ok 50\n" : "not ok 50\n";
print $result eq "abc:bc" ? "ok 51\n" : "not ok 51\n";


$_="abcfooabcbar";
$x=/abc/g;
print $` eq "" ? "ok 52\n" : "not ok 52\n" if $x;
$x=/abc/g;
print $` eq "abcfoo" ? "ok 53\n" : "not ok 53\n" if $x;
$x=/abc/g;
print $x == 0 ? "ok 54\n" : "not ok 54\n";
pos = 0;
$x=/ABC/gi;
print $` eq "" ? "ok 55\n" : "not ok 55\n" if $x;
$x=/ABC/gi;
print $` eq "abcfoo" ? "ok 56\n" : "not ok 56\n" if $x;
$x=/ABC/gi;
print $x == 0 ? "ok 57\n" : "not ok 57\n";
pos = 0;
$x=/abc/g;
print $' eq "fooabcbar" ? "ok 58\n" : "not ok 58\n" if $x;
$x=/abc/g;
print $' eq "bar" ? "ok 59\n" : "not ok 59\n" if $x;
$_ .= '';
@x=/abc/g;
print scalar @x == 2 ? "ok 60\n" : "not ok 60\n";

$_ = "abdc";
pos $_ = 2;
/\Gc/gc;
print "not " if (pos $_) != 2;
print "ok 61\n";
/\Gc/g;
print "not " if defined pos $_;
print "ok 62\n";

$out = 1;
'abc' =~ m'a(?{ $out = 2 })b';
print "not " if $out != 2;
print "ok 63\n";

$out = 1;
'abc' =~ m'a(?{ $out = 3 })c';
print "not " if $out != 1;
print "ok 64\n";

$_ = 'foobar1 bar2 foobar3 barfoobar5 foobar6';
@out = /(?<!foo)bar./g;
print "not " if "@out" ne 'bar2 barf';
print "ok 65\n";

# Tests which depend on REG_INFTY
$reg_infty = defined $Config{reg_infty} ? $Config{reg_infty} : 32767;
$reg_infty_m = $reg_infty - 1; $reg_infty_p = $reg_infty + 1;

# As well as failing if the pattern matches do unexpected things, the
# next three tests will fail if you should have picked up a lower-than-
# default value for $reg_infty from Config.pm, but have not.

undef $@;
print "not " if eval q(('aaa' =~ /(a{1,$reg_infty_m})/)[0] ne 'aaa') || $@;
print "ok 66\n";

undef $@;
print "not " if eval q(('a' x $reg_infty_m) !~ /a{$reg_infty_m}/) || $@;
print "ok 67\n";

undef $@;
print "not " if eval q(('a' x ($reg_infty_m - 1)) =~ /a{$reg_infty_m}/) || $@;
print "ok 68\n";

undef $@;
eval "'aaa' =~ /a{1,$reg_infty}/";
print "not " if $@ !~ m%^\QQuantifier in {,} bigger than%;
print "ok 69\n";

eval "'aaa' =~ /a{1,$reg_infty_p}/";
print "not "
	if $@ !~ m%^\QQuantifier in {,} bigger than%;
print "ok 70\n";
undef $@;

# Poke a couple more parse failures

$context = 'x' x 256;
eval qq("${context}y" =~ /(?<=$context)y/);
print "not " if $@ !~ m%^\QLookbehind longer than 255 not%;
print "ok 71\n";

# removed test
print "ok 72\n";

# Long Monsters
$test = 73;
for $l (125, 140, 250, 270, 300000, 30) { # Ordered to free memory
  $a = 'a' x $l;
  print "# length=$l\nnot " unless "ba$a=" =~ /a$a=/;
  print "ok $test\n";
  $test++;

  print "not " if "b$a=" =~ /a$a=/;
  print "ok $test\n";
  $test++;
}

# 20000 nodes, each taking 3 words per string, and 1 per branch
$long_constant_len = join '|', 12120 .. 32645;
$long_var_len = join '|', 8120 .. 28645;
%ans = ( 'ax13876y25677lbc' => 1,
	 'ax13876y25677mcb' => 0, # not b.
	 'ax13876y35677nbc' => 0, # Num too big
	 'ax13876y25677y21378obc' => 1,
	 'ax13876y25677y21378zbc' => 0,	# Not followed by [k-o]
	 'ax13876y25677y21378y21378kbc' => 1,
	 'ax13876y25677y21378y21378kcb' => 0, # Not b.
	 'ax13876y25677y21378y21378y21378kbc' => 0, # 5 runs
       );

for ( keys %ans ) {
  print "# const-len `$_' not =>  $ans{$_}\nnot "
    if $ans{$_} xor /a(?=([yx]($long_constant_len)){2,4}[k-o]).*b./o;
  print "ok $test\n";
  $test++;
  print "# var-len   `$_' not =>  $ans{$_}\nnot "
    if $ans{$_} xor /a(?=([yx]($long_var_len)){2,4}[k-o]).*b./o;
  print "ok $test\n";
  $test++;
}

$_ = " a (bla()) and x(y b((l)u((e))) and b(l(e)e)e";
$expect = "(bla()) ((l)u((e))) (l(e)e)";

sub matchit {
  m/
     (
       \(
       (?{ $c = 1 })		# Initialize
       (?:
	 (?(?{ $c == 0 })       # PREVIOUS iteration was OK, stop the loop
	   (?!
	   )			# Fail: will unwind one iteration back
	 )	
	 (?:
	   [^()]+		# Match a big chunk
	   (?=
	     [()]
	   )			# Do not try to match subchunks
	 |
	   \(
	   (?{ ++$c })
	 |
	   \)
	   (?{ --$c })
	 )
       )+			# This may not match with different subblocks
     )
     (?(?{ $c != 0 })
       (?!
       )			# Fail
     )				# Otherwise the chunk 1 may succeed with $c>0
   /xg;
}

@ans = ();
push @ans, $res while $res = matchit;

print "# ans='@ans'\n# expect='$expect'\nnot " if "@ans" ne "1 1 1";
print "ok $test\n";
$test++;

@ans = matchit;

print "# ans='@ans'\n# expect='$expect'\nnot " if "@ans" ne $expect;
print "ok $test\n";
$test++;

print "not " unless "abc" =~ /^(??{"a"})b/;
print "ok $test\n";
$test++;

my $matched;
$matched = qr/\((?:(?>[^()]+)|(??{$matched}))*\)/;

@ans = @ans1 = ();
push(@ans, $res), push(@ans1, $&) while $res = m/$matched/g;

print "# ans='@ans'\n# expect='$expect'\nnot " if "@ans" ne "1 1 1";
print "ok $test\n";
$test++;

print "# ans1='@ans1'\n# expect='$expect'\nnot " if "@ans1" ne $expect;
print "ok $test\n";
$test++;

@ans = m/$matched/g;

print "# ans='@ans'\n# expect='$expect'\nnot " if "@ans" ne $expect;
print "ok $test\n";
$test++;

@ans = ('a/b' =~ m%(.*/)?(.*)%);	# Stack may be bad
print "not " if "@ans" ne 'a/ b';
print "ok $test\n";
$test++;

$code = '{$blah = 45}';
$blah = 12;
eval { /(?$code)/ };
print "not " unless $@ and $@ =~ /not allowed at runtime/ and $blah == 12;
print "ok $test\n";
$test++;

for $code ('{$blah = 45}','=xx') {
  $blah = 12;
  $res = eval { "xx" =~ /(?$code)/o };
  if ($code eq '=xx') {
    print "#'$@','$res','$blah'\nnot " unless not $@ and $res;
  } else {
    print "#'$@','$res','$blah'\nnot " unless $@ and $@ =~ /not allowed at runtime/ and $blah == 12;
  }
  print "ok $test\n";
  $test++;
}

$code = '{$blah = 45}';
$blah = 12;
eval "/(?$code)/";			
print "not " if $blah != 45;
print "ok $test\n";
$test++;

$blah = 12;
/(?{$blah = 45})/;			
print "not " if $blah != 45;
print "ok $test\n";
$test++;

$x = 'banana';
$x =~ /.a/g;
print "not " unless pos($x) == 2;
print "ok $test\n";
$test++;

$x =~ /.z/gc;
print "not " unless pos($x) == 2;
print "ok $test\n";
$test++;

sub f {
    my $p = $_[0];
    return $p;
}

$x =~ /.a/g;
print "not " unless f(pos($x)) == 4;
print "ok $test\n";
$test++;

$x = $^R = 67;
'foot' =~ /foo(?{$x = 12; 75})[t]/;
print "not " unless $^R eq '75';
print "ok $test\n";
$test++;

$x = $^R = 67;
'foot' =~ /foo(?{$x = 12; 75})[xy]/;
print "not " unless $^R eq '67' and $x eq '12';
print "ok $test\n";
$test++;

$x = $^R = 67;
'foot' =~ /foo(?{ $^R + 12 })((?{ $x = 12; $^R + 17 })[xy])?/;
print "not " unless $^R eq '79' and $x eq '12';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/i eq '(?i-xsm:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/s eq '(?s-xim:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/m eq '(?m-xis:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/x eq '(?x-ism:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/xism eq '(?msix:\bv$)';
print "ok $test\n";
$test++;

print "not " unless qr/\b\v$/ eq '(?-xism:\bv$)';
print "ok $test\n";
$test++;

$_ = 'xabcx';
foreach $ans ('', 'c') {
  /(?<=(?=a)..)((?=c)|.)/g;
  print "# \$1  ='$1'\n# \$ans='$ans'\nnot " unless $1 eq $ans;
  print "ok $test\n";
  $test++;
}

$_ = 'a';
foreach $ans ('', 'a', '') {
  /^|a|$/g;
  print "# \$&  ='$&'\n# \$ans='$ans'\nnot " unless $& eq $ans;
  print "ok $test\n";
  $test++;
}

sub prefixify {
  my($v,$a,$b,$res) = @_;
  $v =~ s/\Q$a\E/$b/;
  print "not " unless $res eq $v;
  print "ok $test\n";
  $test++;
}
prefixify('/a/b/lib/arch', "/a/b/lib", 'X/lib', 'X/lib/arch');
prefixify('/a/b/man/arch', "/a/b/man", 'X/man', 'X/man/arch');

$_ = 'var="foo"';
/(\")/;
print "not " unless $1 and /$1/;
print "ok $test\n";
$test++;

$a=qr/(?{++$b})/;
$b = 7;
/$a$a/;
print "not " unless $b eq '9';
print "ok $test\n";
$test++;

$c="$a";
/$a$a/;
print "not " unless $b eq '11';
print "ok $test\n";
$test++;

{
  use re "eval";
  /$a$c$a/;
  print "not " unless $b eq '14';
  print "ok $test\n";
  $test++;

  local $lex_a = 2;
  my $lex_a = 43;
  my $lex_b = 17;
  my $lex_c = 27;
  my $lex_res = ($lex_b =~ qr/$lex_b(?{ $lex_c = $lex_a++ })/);
  print "not " unless $lex_res eq '1';
  print "ok $test\n";
  $test++;
  print "not " unless $lex_a eq '44';
  print "ok $test\n";
  $test++;
  print "not " unless $lex_c eq '43';
  print "ok $test\n";
  $test++;


  no re "eval";
  $match = eval { /$a$c$a/ };
  print "not "
    unless $b eq '14' and $@ =~ /Eval-group not allowed/ and not $match;
  print "ok $test\n";
  $test++;
}

{
  local $lex_a = 2;
  my $lex_a = 43;
  my $lex_b = 17;
  my $lex_c = 27;
  my $lex_res = ($lex_b =~ qr/17(?{ $lex_c = $lex_a++ })/);
  print "not " unless $lex_res eq '1';
  print "ok $test\n";
  $test++;
  print "not " unless $lex_a eq '44';
  print "ok $test\n";
  $test++;
  print "not " unless $lex_c eq '43';
  print "ok $test\n";
  $test++;
}

{
  package aa;
  $c = 2;
  $::c = 3;
  '' =~ /(?{ $c = 4 })/;
  print "not " unless $c == 4;
}
print "ok $test\n";
$test++;
print "not " unless $c == 3;
print "ok $test\n";
$test++;

sub must_warn_pat {
    my $warn_pat = shift;
    return sub { print "not " unless $_[0] =~ /$warn_pat/ }
}

sub must_warn {
    my ($warn_pat, $code) = @_;
    local %SIG;
    eval 'BEGIN { use warnings; $SIG{__WARN__} = $warn_pat };' . $code;
    print "ok $test\n";
    $test++;
}


sub make_must_warn {
    my $warn_pat = shift;
    return sub { must_warn(must_warn_pat($warn_pat)) }
}

my $for_future = make_must_warn('reserved for future extensions');

&$for_future('q(a:[b]:) =~ /[x[:foo:]]/');

#&$for_future('q(a=[b]=) =~ /[x[=foo=]]/');
print "ok $test\n"; $test++; # now a fatal croak

#&$for_future('q(a.[b].) =~ /[x[.foo.]]/');
print "ok $test\n"; $test++; # now a fatal croak

# test if failure of patterns returns empty list
$_ = 'aaa';
@_ = /bbb/;
print "not " if @_;
print "ok $test\n";
$test++;

@_ = /bbb/g;
print "not " if @_;
print "ok $test\n";
$test++;

@_ = /(bbb)/;
print "not " if @_;
print "ok $test\n";
$test++;

@_ = /(bbb)/g;
print "not " if @_;
print "ok $test\n";
$test++;

/a(?=.$)/;
print "not " if $#+ != 0 or $#- != 0;
print "ok $test\n";
$test++;

print "not " if $+[0] != 2 or $-[0] != 1;
print "ok $test\n";
$test++;

print "not "
   if defined $+[1] or defined $-[1] or defined $+[2] or defined $-[2];
print "ok $test\n";
$test++;

/a(a)(a)/;
print "not " if $#+ != 2 or $#- != 2;
print "ok $test\n";
$test++;

print "not " if $+[0] != 3 or $-[0] != 0;
print "ok $test\n";
$test++;

print "not " if $+[1] != 2 or $-[1] != 1;
print "ok $test\n";
$test++;

print "not " if $+[2] != 3 or $-[2] != 2;
print "ok $test\n";
$test++;

print "not "
   if defined $+[3] or defined $-[3] or defined $+[4] or defined $-[4];
print "ok $test\n";
$test++;

/.(a)(b)?(a)/;
print "not " if $#+ != 3 or $#- != 3;
print "ok $test\n";
$test++;

print "not " if $+[0] != 3 or $-[0] != 0;
print "ok $test\n";
$test++;

print "not " if $+[1] != 2 or $-[1] != 1;
print "ok $test\n";
$test++;

print "not " if $+[3] != 3 or $-[3] != 2;
print "ok $test\n";
$test++;

print "not "
   if defined $+[2] or defined $-[2] or defined $+[4] or defined $-[4];
print "ok $test\n";
$test++;

/.(a)/;
print "not " if $#+ != 1 or $#- != 1;
print "ok $test\n";
$test++;

print "not " if $+[0] != 2 or $-[0] != 0;
print "ok $test\n";
$test++;

print "not " if $+[1] != 2 or $-[1] != 1;
print "ok $test\n";
$test++;

print "not "
   if defined $+[2] or defined $-[2] or defined $+[3] or defined $-[3];
print "ok $test\n";
$test++;

eval { $+[0] = 13; };
print "not "
   if $@ !~ /^Modification of a read-only value attempted/;
print "ok $test\n";
$test++;

eval { $-[0] = 13; };
print "not "
   if $@ !~ /^Modification of a read-only value attempted/;
print "ok $test\n";
$test++;

eval { @+ = (7, 6, 5); };
print "not "
   if $@ !~ /^Modification of a read-only value attempted/;
print "ok $test\n";
$test++;

eval { @- = qw(foo bar); };
print "not "
   if $@ !~ /^Modification of a read-only value attempted/;
print "ok $test\n";
$test++;

/.(a)(ba*)?/;
print "#$#-..$#+\nnot " if $#+ != 2 or $#- != 1;
print "ok $test\n";
$test++;

$_ = 'aaa';
pos = 1;
@a = /\Ga/g;
print "not " unless "@a" eq "a a";
print "ok $test\n";
$test++;

$str = 'abcde';
pos $str = 2;

print "not " if $str =~ /^\G/;
print "ok $test\n";
$test++;

print "not " if $str =~ /^.\G/;
print "ok $test\n";
$test++;

print "not " unless $str =~ /^..\G/;
print "ok $test\n";
$test++;

print "not " if $str =~ /^...\G/;
print "ok $test\n";
$test++;

print "not " unless $str =~ /.\G./ and $& eq 'bc';
print "ok $test\n";
$test++;

print "not " unless $str =~ /\G../ and $& eq 'cd';
print "ok $test\n";
$test++;

undef $foo; undef $bar;
print "#'$str','$foo','$bar'\nnot "
    unless $str =~ /b(?{$foo = $_; $bar = pos})c/
	and $foo eq 'abcde' and $bar eq 2;
print "ok $test\n";
$test++;

undef $foo; undef $bar;
pos $str = undef;
print "#'$str','$foo','$bar'\nnot "
    unless $str =~ /b(?{$foo = $_; $bar = pos})c/g
	and $foo eq 'abcde' and $bar eq 2 and pos $str eq 3;
print "ok $test\n";
$test++;

$_ = $str;

undef $foo; undef $bar;
print "#'$str','$foo','$bar'\nnot "
    unless /b(?{$foo = $_; $bar = pos})c/
	and $foo eq 'abcde' and $bar eq 2;
print "ok $test\n";
$test++;

undef $foo; undef $bar;
print "#'$str','$foo','$bar'\nnot "
    unless /b(?{$foo = $_; $bar = pos})c/g
	and $foo eq 'abcde' and $bar eq 2 and pos eq 3;
print "ok $test\n";
$test++;

undef $foo; undef $bar;
pos = undef;
1 while /b(?{$foo = $_; $bar = pos})c/g;
print "#'$str','$foo','$bar'\nnot "
    unless $foo eq 'abcde' and $bar eq 2 and not defined pos;
print "ok $test\n";
$test++;

undef $foo; undef $bar;
$_ = 'abcde|abcde';
print "#'$str','$foo','$bar','$_'\nnot "
    unless s/b(?{$foo = $_; $bar = pos})c/x/g and $foo eq 'abcde|abcde'
	and $bar eq 8 and $_ eq 'axde|axde';
print "ok $test\n";
$test++;

@res = ();
# List context:
$_ = 'abcde|abcde';
@dummy = /([ace]).(?{push @res, $1,$2})([ce])(?{push @res, $1,$2})/g;
@res = map {defined $_ ? "'$_'" : 'undef'} @res;
$res = "@res";
print "#'@res' '$_'\nnot "
    unless "@res" eq "'a' undef 'a' 'c' 'e' undef 'a' undef 'a' 'c'";
print "ok $test\n";
$test++;

@res = ();
@dummy = /([ace]).(?{push @res, $`,$&,$'})([ce])(?{push @res, $`,$&,$'})/g;
@res = map {defined $_ ? "'$_'" : 'undef'} @res;
$res = "@res";
print "#'@res' '$_'\nnot "
    unless "@res" eq
  "'' 'ab' 'cde|abcde' " .
  "'' 'abc' 'de|abcde' " .
  "'abcd' 'e|' 'abcde' " .
  "'abcde|' 'ab' 'cde' " .
  "'abcde|' 'abc' 'de'" ;
print "ok $test\n";
$test++;

#Some more \G anchor checks
$foo='aabbccddeeffgg';

pos($foo)=1;

$foo=~/.\G(..)/g;
print "not " unless($1 eq 'ab');
print "ok $test\n";
$test++;

pos($foo) += 1;
$foo=~/.\G(..)/g;
print "not " unless($1 eq 'cc');
print "ok $test\n";
$test++;

pos($foo) += 1;
$foo=~/.\G(..)/g;
print "not " unless($1 eq 'de');
print "ok $test\n";
$test++;

print "not " unless $foo =~ /\Gef/g;
print "ok $test\n";
$test++;

undef pos $foo;

$foo=~/\G(..)/g;
print "not " unless($1  eq 'aa');
print "ok $test\n";
$test++;

$foo=~/\G(..)/g;
print "not " unless($1  eq 'bb');
print "ok $test\n";
$test++;

pos($foo)=5;
$foo=~/\G(..)/g;
print "not " unless($1  eq 'cd');
print "ok $test\n";
$test++;

$_='123x123';
@res = /(\d*|x)/g;
print "not " unless('123||x|123|' eq join '|', @res);
print "ok $test\n";
$test++;

# see if matching against temporaries (created via pp_helem()) is safe
{ foo => "ok $test\n".$^X }->{foo} =~ /^(.*)\n/g;
print "$1\n";
$test++;

# See if $i work inside (?{}) in the presense of saved substrings and
# changing $_
@a = qw(foo bar);
@b = ();
s/(\w)(?{push @b, $1})/,$1,/g for @a;

print "# \@b='@b', expect 'f o o b a r'\nnot " unless("@b" eq "f o o b a r");
print "ok $test\n";
$test++;

print "not " unless("@a" eq ",f,,o,,o, ,b,,a,,r,");
print "ok $test\n";
$test++;

$brackets = qr{
	         {  (?> [^{}]+ | (??{ $brackets }) )* }
	      }x;

"{{}" =~ $brackets;
print "ok $test\n";		# Did we survive?
$test++;

"something { long { and } hairy" =~ $brackets;
print "ok $test\n";		# Did we survive?
$test++;

"something { long { and } hairy" =~ m/((??{ $brackets }))/;
print "not " unless $1 eq "{ and }";
print "ok $test\n";
$test++;

$_ = "a-a\nxbb";
pos=1;
m/^-.*bb/mg and print "not ";
print "ok $test\n";
$test++;

$text = "aaXbXcc";
pos($text)=0;
$text =~ /\GXb*X/g and print 'not ';
print "ok $test\n";
$test++;

$text = "xA\n" x 500;
$text =~ /^\s*A/m and print 'not ';
print "ok $test\n";
$test++;

$text = "abc dbf";
@res = ($text =~ /.*?(b).*?\b/g);
"@res" eq 'b b' or print 'not ';
print "ok $test\n";
$test++;

@a = map chr,0..255;

@b = grep(/\S/,@a);
@c = grep(/[^\s]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\S/,@a);
@c = grep(/[\S]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\s/,@a);
@c = grep(/[^\S]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\s/,@a);
@c = grep(/[\s]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\D/,@a);
@c = grep(/[^\d]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\D/,@a);
@c = grep(/[\D]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\d/,@a);
@c = grep(/[^\D]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\d/,@a);
@c = grep(/[\d]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\W/,@a);
@c = grep(/[^\w]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\W/,@a);
@c = grep(/[\W]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\w/,@a);
@c = grep(/[^\W]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

@b = grep(/\w/,@a);
@c = grep(/[\w]/,@a);
print "not " if "@b" ne "@c";
print "ok $test\n";
$test++;

# see if backtracking optimization works correctly
"\n\n" =~ /\n  $ \n/x or print "not ";
print "ok $test\n";
$test++;

"\n\n" =~ /\n* $ \n/x or print "not ";
print "ok $test\n";
$test++;

"\n\n" =~ /\n+ $ \n/x or print "not ";
print "ok $test\n";
$test++;

[] =~ /^ARRAY/ or print "# [] \nnot ";
print "ok $test\n";
$test++;

eval << 'EOE';
{
 package S;
 use overload '""' => sub { 'Object S' };
 sub new { bless [] }
}
$a = 'S'->new;
EOE

$a and $a =~ /^Object\sS/ or print "# '$a' \nnot ";
print "ok $test\n";
$test++;

# test result of match used as match (!)
'a1b' =~ ('xyz' =~ /y/) and $` eq 'a' or print "not ";
print "ok $test\n";
$test++;

'a1b' =~ ('xyz' =~ /t/) and $` eq 'a' or print "not ";
print "ok $test\n";
$test++;

$w = 0;
{
    local $SIG{__WARN__} = sub { $w = 1 };
    local $^W = 1;
	$w = 1 if ("1\n" x 102) =~ /^\s*\n/m;
}
print $w ? "not " : "", "ok $test\n";
$test++;

my %space = ( spc   => " ",
	      tab   => "\t",
	      cr    => "\r",
	      lf    => "\n",
	      ff    => "\f",
# There's no \v but the vertical tabulator seems miraculously
# be 11 both in ASCII and EBCDIC.
	      vt    => chr(11),
	      false => "space" );

my @space0 = sort grep { $space{$_} =~ /\s/ }          keys %space;
my @space1 = sort grep { $space{$_} =~ /[[:space:]]/ } keys %space;
my @space2 = sort grep { $space{$_} =~ /[[:blank:]]/ } keys %space;

print "not " unless "@space0" eq "cr ff lf spc tab";
print "ok $test # @space0\n";
$test++;

print "not " unless "@space1" eq "cr ff lf spc tab vt";
print "ok $test # @space1\n";
$test++;

print "not " unless "@space2" eq "spc tab";
print "ok $test # @space2\n";
$test++;

# bugid 20001021.005 - this caused a SEGV
print "not " unless undef =~ /^([^\/]*)(.*)$/;
print "ok $test\n";
$test++;

# bugid 20000731.001

print "not " unless "A \x{263a} B z C" =~ /A . B (??{ "z" }) C/;
print "ok $test\n";
$test++;

my $ordA = ord('A');

$_ = "a\x{100}b";
if (/(.)(\C)(\C)(.)/) {
  print "ok 232\n";
  if ($1 eq "a") {
    print "ok 233\n";
  } else {
    print "not ok 233\n";
  }
  if ($ordA == 65) { # ASCII (or equivalent), should be UTF-8
      if ($2 eq "\xC4") {
	  print "ok 234\n";
      } else {
	  print "not ok 234\n";
      }
      if ($3 eq "\x80") {
	  print "ok 235\n";
      } else {
	  print "not ok 235\n";
      }
  } elsif ($ordA == 193) { # EBCDIC (or equivalent), should be UTF-EBCDIC
      if ($2 eq "\x8C") {
	  print "ok 234\n";
      } else {
	  print "not ok 234\n";
      }
      if ($3 eq "\x41") {
	  print "ok 235\n";
      } else {
	  print "not ok 235\n";
      }
  } else {
      for (234..235) {
	  print "not ok $_ # ord('A') == $ordA\n";
      }
  }
  if ($4 eq "b") {
    print "ok 236\n";
  } else {
    print "not ok 236\n";
  }
} else {
  for (232..236) {
    print "not ok $_\n";
  }
}
$_ = "\x{100}";
if (/(\C)/g) {
  print "ok 237\n";
  # currently \C are still tagged as UTF-8
  if ($ordA == 65) {
      if ($1 eq "\xC4") {
	  print "ok 238\n";
      } else {
	  print "not ok 238\n";
      }
  } elsif ($ordA == 193) {
      if ($1 eq "\x8C") {
	  print "ok 238\n";
      } else {
	  print "not ok 238\n";
      }
  } else {
      print "not ok 238 # ord('A') == $ordA\n";
  }
} else {
  for (237..238) {
    print "not ok $_\n";
  }
}
if (/(\C)/g) {
  print "ok 239\n";
  # currently \C are still tagged as UTF-8
  if ($ordA == 65) {
      if ($1 eq "\x80") {
	  print "ok 240\n";
      } else {
	  print "not ok 240\n";
      }
  } elsif ($ordA == 193) {
      if ($1 eq "\x41") {
	  print "ok 240\n";
      } else {
	  print "not ok 240\n";
      }
  } else {
      print "not ok 240 # ord('A') == $ordA\n";
  }
} else {
  for (239..240) {
    print "not ok $_\n";
  }
}

{
  # japhy -- added 03/03/2001
  () = (my $str = "abc") =~ /(...)/;
  $str = "def";
  print "not " if $1 ne "abc";
  print "ok 241\n";
}

# The 242 and 243 go with the 244 and 245.
# The trick is that in EBCDIC the explicit numeric range should match
# (as also in non-EBCDIC) but the explicit alphabetic range should not match.

if ("\x8e" =~ /[\x89-\x91]/) {
  print "ok 242\n";
} else {
  print "not ok 242\n";
}

if ("\xce" =~ /[\xc9-\xd1]/) {
  print "ok 243\n";
} else {
  print "not ok 243\n";
}

# In most places these tests would succeed since \x8e does not
# in most character sets match 'i' or 'j' nor would \xce match
# 'I' or 'J', but strictly speaking these tests are here for
# the good of EBCDIC, so let's test these only there.
if (ord('i') == 0x89 && ord('J') == 0xd1) { # EBCDIC
  if ("\x8e" !~ /[i-j]/) {
    print "ok 244\n";
  } else {
    print "not ok 244\n";
  }
  if ("\xce" !~ /[I-J]/) {
    print "ok 245\n";
  } else {
    print "not ok 245\n";
  }
} else {
  for (244..245) {
    print "ok $_ # Skip: only in EBCDIC\n";
  }
}

print "not " unless "\x{ab}" =~ /\x{ab}/;
print "ok 246\n";

print "not " unless "\x{abcd}" =~ /\x{abcd}/;
print "ok 247\n";

{
    # bug id 20001008.001

    my $test = 248;
    my @x = ("stra\337e 138","stra\337e 138");
    for (@x) {
	s/(\d+)\s*([\w\-]+)/$1 . uc $2/e;
	my($latin) = /^(.+)(?:\s+\d)/;
	print $latin eq "stra\337e" ? "ok $test\n" :	# 248,249
	    "#latin[$latin]\nnot ok $test\n";
	$test++;
	$latin =~ s/stra\337e/straße/; # \303\237 after the 2nd a
	use utf8; # needed for the raw UTF-8
	$latin =~ s!(s)tr(?:aß|s+e)!$1tr.!; # \303\237 after the a
    }
}

{
    print "not " unless "ba\xd4c" =~ /([a\xd4]+)/ && $1 eq "a\xd4";
    print "ok 250\n";

    print "not " unless "ba\xd4c" =~ /([a\xd4]+)/ && $1 eq "a\x{d4}";
    print "ok 251\n";

    print "not " unless "ba\x{d4}c" =~ /([a\xd4]+)/ && $1 eq "a\x{d4}";
    print "ok 252\n";

    print "not " unless "ba\x{d4}c" =~ /([a\xd4]+)/ && $1 eq "a\xd4";
    print "ok 253\n";

    print "not " unless "ba\xd4c" =~ /([a\x{d4}]+)/ && $1 eq "a\xd4";
    print "ok 254\n";

    print "not " unless "ba\xd4c" =~ /([a\x{d4}]+)/ && $1 eq "a\x{d4}";
    print "ok 255\n";

    print "not " unless "ba\x{d4}c" =~ /([a\x{d4}]+)/ && $1 eq "a\x{d4}";
    print "ok 256\n";

    print "not " unless "ba\x{d4}c" =~ /([a\x{d4}]+)/ && $1 eq "a\xd4";
    print "ok 257\n";
}

{
    # the first half of 20001028.003

    my $X = chr(1448);
    my ($Y) = $X =~ /(.*)/;
    print "not " unless $Y eq v1448 && length($Y) == 1;
    print "ok 258\n";
}

{
    # 20001108.001

    my $X = "Szab\x{f3},Bal\x{e1}zs";
    my $Y = $X;
    $Y =~ s/(B)/$1/ for 0..3;
    print "not " unless $Y eq $X && $X eq "Szab\x{f3},Bal\x{e1}zs";
    print "ok 259\n";
}

{
    # the second half of 20001028.003

    my $X = '';
    $X =~ s/^/chr(1488)/e;
    print "not " unless length $X == 1 && ord($X) == 1488;
    print "ok 260\n";
}

{
    # 20000517.001

    my $x = "\x{100}A";

    $x =~ s/A/B/;

    print "not " unless $x eq "\x{100}B" && length($x) == 2;
    print "ok 261\n";
}

{
    # bug id 20001230.002

    print "not " unless "École" =~ /^\C\C(.)/ && $1 eq 'c';
    print "ok 262\n";

    print "not " unless "École" =~ /^\C\C(c)/;
    print "ok 263\n";
}

SKIP: {
    my $test = 264; # till 575

    use charnames ":full";

    # This is far from complete testing, there are dozens of character
    # classes in Unicode.  The mixing of literals and \N{...} is
    # intentional so that in non-Latin-1 places we test the native
    # characters, not the Unicode code points.

    my %s = (
	     "a" 				=> 'Ll',
	     "\N{CYRILLIC SMALL LETTER A}"	=> 'Ll',
	     "A" 				=> 'Lu',
	     "\N{GREEK CAPITAL LETTER ALPHA}"	=> 'Lu',
	     "\N{HIRAGANA LETTER SMALL A}"	=> 'Lo',
	     "\N{COMBINING GRAVE ACCENT}"	=> 'Mn',
	     "0"				=> 'Nd',
	     "\N{ARABIC-INDIC DIGIT ZERO}"	=> 'Nd',
	     "_"				=> 'N',
	     "!"				=> 'P',
	     " "				=> 'Zs',
	     "\0"				=> 'Cc',
	     );
	
    for my $char (map { s/^\S+ //; $_ }
                    sort map { sprintf("%06x", ord($_))." $_" } keys %s) {
	my $class = $s{$char};
	my $code  = sprintf("%06x", ord($char));
	printf "#\n# 0x$code\n#\n";
	print "# IsAlpha\n";
	if ($class =~ /^[LM]/) {
	    print "not " unless $char =~ /\p{IsAlpha}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsAlpha}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsAlpha}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsAlpha}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsAlnum\n";
	if ($class =~ /^[LMN]/ && $char ne "_") {
	    print "not " unless $char =~ /\p{IsAlnum}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsAlnum}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsAlnum}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsAlnum}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsASCII\n";
	if (ord("A") == 193) {
	    print "ok $test # Skip: in EBCDIC\n"; $test++;
	    print "ok $test # Skip: in EBCDIC\n"; $test++;
	} else {
	    if ($code le '00007f') {
		print "not " unless $char =~ /\p{IsASCII}/;
		print "ok $test\n"; $test++;
		print "not " if     $char =~ /\P{IsASCII}/;
		print "ok $test\n"; $test++;
	    } else {
		print "not " if     $char =~ /\p{IsASCII}/;
		print "ok $test\n"; $test++;
		print "not " unless $char =~ /\P{IsASCII}/;
		print "ok $test\n"; $test++;
	    }
	}
	print "# IsCntrl\n";
	if ($class =~ /^C/) {
	    print "not " unless $char =~ /\p{IsCntrl}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsCntrl}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsCntrl}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsCntrl}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsBlank\n";
	if ($class =~ /^Z[lp]/ || $char eq " ") {
	    print "not " unless $char =~ /\p{IsBlank}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsBlank}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsBlank}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsBlank}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsDigit\n";
	if ($class =~ /^Nd$/) {
	    print "not " unless $char =~ /\p{IsDigit}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsDigit}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsDigit}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsDigit}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsGraph\n";
	if ($class =~ /^([LMNPS])|Co/) {
	    print "not " unless $char =~ /\p{IsGraph}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsGraph}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsGraph}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsGraph}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsLower\n";
	if ($class =~ /^Ll$/) {
	    print "not " unless $char =~ /\p{IsLower}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsLower}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsLower}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsLower}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsPrint\n";
	if ($class =~ /^([LMNPS])|Co|Zs/) {
	    print "not " unless $char =~ /\p{IsPrint}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsPrint}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsPrint}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsPrint}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsPunct\n";
	if ($class =~ /^P/ || $char eq "_") {
	    print "not " unless $char =~ /\p{IsPunct}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsPunct}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsPunct}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsPunct}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsSpace\n";
	if ($class =~ /^Z/ || ($code =~ /^(0009|000A|000B|000C|000D)$/)) {
	    print "not " unless $char =~ /\p{IsSpace}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsSpace}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsSpace}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsSpace}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsUpper\n";
	if ($class =~ /^L[ut]/) {
	    print "not " unless $char =~ /\p{IsUpper}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsUpper}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsUpper}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsUpper}/;
	    print "ok $test\n"; $test++;
	}
	print "# IsWord\n";
	if ($class =~ /^[LMN]/ || $char eq "_") {
	    print "not " unless $char =~ /\p{IsWord}/;
	    print "ok $test\n"; $test++;
	    print "not " if     $char =~ /\P{IsWord}/;
	    print "ok $test\n"; $test++;
	} else {
	    print "not " if     $char =~ /\p{IsWord}/;
	    print "ok $test\n"; $test++;
	    print "not " unless $char =~ /\P{IsWord}/;
	    print "ok $test\n"; $test++;
	}
    }
}

{
    $_ = "abc\x{100}\x{200}\x{300}\x{380}\x{400}defg";

    if (/(.\x{300})./) {
	print "ok 576\n";

	print "not " unless $` eq "abc\x{100}" && length($`) == 4;
	print "ok 577\n";

	print "not " unless $& eq "\x{200}\x{300}\x{380}" && length($&) == 3;
	print "ok 578\n";

	print "not " unless $' eq "\x{400}defg" && length($') == 5;
	print "ok 579\n";

	print "not " unless $1 eq "\x{200}\x{300}" && length($1) == 2;
	print "ok 580\n";
    } else {
	for (576..580) { print "not ok $_\n" }
    }
}

{
    # bug id 20010306.008

    $a = "a\x{1234}";
    # The original bug report had 'no utf8' here but that was irrelevant.
    $a =~ m/\w/; # used to core dump

    print "ok 581\n";
}

{
    $test = 582;

    # bugid 20010410.006
    for my $rx (
		'/(.*?)\{(.*?)\}/csg',
		'/(.*?)\{(.*?)\}/cg',
		'/(.*?)\{(.*?)\}/sg',
		'/(.*?)\{(.*?)\}/g',
		'/(.+?)\{(.+?)\}/csg',
	       )
    {
	my($input, $i);

	$i = 0;
	$input = "a{b}c{d}";
        eval <<EOT;
	while (eval \$input =~ $rx) {
	    print "# \\\$1 = '\$1' \\\$2 = '\$2'\n";
	    ++\$i;
	}
EOT
	print "not " unless $i == 2;
	print "ok " . $test++ . "\n";
    }
}

{
    # from Robin Houston

    my $x = "\x{10FFFD}";
    $x =~ s/(.)/$1/g;
    print "not " unless ord($x) == 0x10FFFD && length($x) == 1;
    print "ok 587\n";
}

{
    my $x = "\x7f";

    print "not " if     $x =~ /[\x80-\xff]/;
    print "ok 588\n";

    print "not " if     $x =~ /[\x80-\x{100}]/;
    print "ok 589\n";

    print "not " if     $x =~ /[\x{100}]/;
    print "ok 590\n";

    print "not " if     $x =~ /\p{InLatin1Supplement}/;
    print "ok 591\n";

    print "not " unless $x =~ /\P{InLatin1Supplement}/;
    print "ok 592\n";

    print "not " if     $x =~ /\p{InLatinExtendedA}/;
    print "ok 593\n";

    print "not " unless $x =~ /\P{InLatinExtendedA}/;
    print "ok 594\n";
}

{
    my $x = "\x80";

    print "not " unless $x =~ /[\x80-\xff]/;
    print "ok 595\n";

    print "not " unless $x =~ /[\x80-\x{100}]/;
    print "ok 596\n";

    print "not " if     $x =~ /[\x{100}]/;
    print "ok 597\n";

    print "not " unless $x =~ /\p{InLatin1Supplement}/;
    print "ok 598\n";

    print "not " if    $x =~ /\P{InLatin1Supplement}/;
    print "ok 599\n";

    print "not " if     $x =~ /\p{InLatinExtendedA}/;
    print "ok 600\n";

    print "not " unless $x =~ /\P{InLatinExtendedA}/;
    print "ok 601\n";
}

{
    my $x = "\xff";

    print "not " unless $x =~ /[\x80-\xff]/;
    print "ok 602\n";

    print "not " unless $x =~ /[\x80-\x{100}]/;
    print "ok 603\n";

    print "not " if     $x =~ /[\x{100}]/;
    print "ok 604\n";

    # the next two tests must be ignored on EBCDIC
    print "not " unless $x =~ /\p{InLatin1Supplement}/ or ord("A") == 193;
    print "ok 605\n";

    print "not " if     $x =~ /\P{InLatin1Supplement}/ and ord("A") != 193;
    print "ok 606\n";

    print "not " if     $x =~ /\p{InLatinExtendedA}/;
    print "ok 607\n";

    print "not " unless $x =~ /\P{InLatinExtendedA}/;
    print "ok 608\n";
}

{
    my $x = "\x{100}";

    print "not " if     $x =~ /[\x80-\xff]/;
    print "ok 609\n";

    print "not " unless $x =~ /[\x80-\x{100}]/;
    print "ok 610\n";

    print "not " unless $x =~ /[\x{100}]/;
    print "ok 611\n";

    print "not " if     $x =~ /\p{InLatin1Supplement}/;
    print "ok 612\n";

    print "not " unless $x =~ /\P{InLatin1Supplement}/;
    print "ok 613\n";

    print "not " unless $x =~ /\p{InLatinExtendedA}/;
    print "ok 614\n";

    print "not " if     $x =~ /\P{InLatinExtendedA}/;
    print "ok 615\n";
}

{
    # from japhy
    my $w;
    use warnings;    
    local $SIG{__WARN__} = sub { $w .= shift };

    $w = "";
    eval 'qr/(?c)/';
    print "not " if $w !~ /^Useless \(\?c\)/;
    print "ok 616\n";

    $w = "";
    eval 'qr/(?-c)/';
    print "not " if $w !~ /^Useless \(\?-c\)/;
    print "ok 617\n";

    $w = "";
    eval 'qr/(?g)/';
    print "not " if $w !~ /^Useless \(\?g\)/;
    print "ok 618\n";

    $w = "";
    eval 'qr/(?-g)/';
    print "not " if $w !~ /^Useless \(\?-g\)/;
    print "ok 619\n";

    $w = "";
    eval 'qr/(?o)/';
    print "not " if $w !~ /^Useless \(\?o\)/;
    print "ok 620\n";

    $w = "";
    eval 'qr/(?-o)/';
    print "not " if $w !~ /^Useless \(\?-o\)/;
    print "ok 621\n";

    # now test multi-error regexes

    $w = "";
    eval 'qr/(?g-o)/';
    print "not " if $w !~ /^Useless \(\?g\).*\nUseless \(\?-o\)/;
    print "ok 622\n";

    $w = "";
    eval 'qr/(?g-c)/';
    print "not " if $w !~ /^Useless \(\?g\).*\nUseless \(\?-c\)/;
    print "ok 623\n";

    $w = "";
    eval 'qr/(?o-cg)/';  # (?c) means (?g) error won't be thrown
    print "not " if $w !~ /^Useless \(\?o\).*\nUseless \(\?-c\)/;
    print "ok 624\n";

    $w = "";
    eval 'qr/(?ogc)/';
    print "not " if $w !~ /^Useless \(\?o\).*\nUseless \(\?g\).*\nUseless \(\?c\)/;
    print "ok 625\n";
}

# More Unicode "class" tests

{
    use charnames ':full';

    print "not " unless "\N{LATIN CAPITAL LETTER A}" =~ /\p{InBasicLatin}/;
    print "ok 626\n";

    print "not " unless "\N{LATIN CAPITAL LETTER A WITH GRAVE}" =~ /\p{InLatin1Supplement}/;
    print "ok 627\n";

    print "not " unless "\N{LATIN CAPITAL LETTER A WITH MACRON}" =~ /\p{InLatinExtendedA}/;
    print "ok 628\n";

    print "not " unless "\N{LATIN SMALL LETTER B WITH STROKE}" =~ /\p{InLatinExtendedB}/;
    print "ok 629\n";

    print "not " unless "\N{KATAKANA LETTER SMALL A}" =~ /\p{InKatakana}/;
    print "ok 630\n";
}

$_ = "foo";

eval <<"EOT"; die if $@;
  /f
   o\r
   o
   \$
  /x && print "ok 631\n";
EOT

eval <<"EOT"; die if $@;
  /f
   o
   o
   \$\r
  /x && print "ok 632\n";
EOT

#test /o feature
sub test_o { $_[0] =~/$_[1]/o; return $1}
if(test_o('abc','(.)..') eq 'a') {
    print "ok 633\n";
} else {
    print "not ok 633\n";
}
if(test_o('abc','..(.)') eq 'a') {
    print "ok 634\n";
} else {
    print "not ok 634\n";
}

# 635..639: ID 20010619.003 (only the space character is
# supposed to be [:print:], not the whole isprint()).

print "not " if "\n"     =~ /[[:print:]]/;
print "ok 635\n";

print "not " if "\t"     =~ /[[:print:]]/;
print "ok 636\n";

# Amazingly vertical tabulator is the same in ASCII and EBCDIC.
print "not " if "\014"  =~ /[[:print:]]/;
print "ok 637\n";

print "not " if "\r"    =~ /[[:print:]]/;
print "ok 638\n";

print "not " unless " " =~ /[[:print:]]/;
print "ok 639\n";

##
## Test basic $^N usage outside of a regex
##
$x = "abcdef";
$T="ok 640\n";if ($x =~ /cde/ and not defined $^N)         {print $T} else {print "not $T"};
$T="ok 641\n";if ($x =~ /(cde)/          and $^N eq "cde") {print $T} else {print "not $T"};
$T="ok 642\n";if ($x =~ /(c)(d)(e)/      and $^N eq   "e") {print $T} else {print "not $T"};
$T="ok 643\n";if ($x =~ /(c(d)e)/        and $^N eq "cde") {print $T} else {print "not $T"};
$T="ok 644\n";if ($x =~ /(foo)|(c(d)e)/  and $^N eq "cde") {print $T} else {print "not $T"};
$T="ok 645\n";if ($x =~ /(c(d)e)|(foo)/  and $^N eq "cde") {print $T} else {print "not $T"};
$T="ok 646\n";if ($x =~ /(c(d)e)|(abc)/  and $^N eq "abc") {print $T} else {print "not $T"};
$T="ok 647\n";if ($x =~ /(c(d)e)|(abc)x/ and $^N eq "cde") {print $T} else {print "not $T"};
$T="ok 648\n";if ($x =~ /(c(d)e)(abc)?/  and $^N eq "cde") {print $T} else {print "not $T"};
$T="ok 649\n";if ($x =~ /(?:c(d)e)/      and $^N eq  "d" ) {print $T} else {print "not $T"};
$T="ok 650\n";if ($x =~ /(?:c(d)e)(?:f)/ and $^N eq  "d" ) {print $T} else {print "not $T"};
$T="ok 651\n";if ($x =~ /(?:([abc])|([def]))*/ and $^N eq  "f" ){print $T} else {print "not $T"};
$T="ok 652\n";if ($x =~ /(?:([ace])|([bdf]))*/ and $^N eq  "f" ){print $T} else {print "not $T"};
$T="ok 653\n";if ($x =~ /(([ace])|([bd]))*/    and $^N eq  "e" ){print $T} else {print "not $T"};
{
 $T="ok 654\n";if($x =~ /(([ace])|([bdf]))*/   and $^N eq  "f" ){print $T} else {print "not $T"};
}
## test to see if $^N is automatically localized -- it should now
## have the value set in test 653
$T="ok 655\n";if ($^N eq  "e" ){print $T} else {print "not $T"};

##
## Now test inside (?{...})
##
$T="ok 656\n";if ($x =~ /a([abc])(?{$y=$^N})c/      and $y eq "b" ){print $T} else {print "not $T"};
$T="ok 657\n";if ($x =~ /a([abc]+)(?{$y=$^N})d/     and $y eq "bc"){print $T} else {print "not $T"};
$T="ok 658\n";if ($x =~ /a([abcdefg]+)(?{$y=$^N})d/ and $y eq "bc"){print $T} else {print "not $T"};
$T="ok 659\n";if ($x =~ /(a([abcdefg]+)(?{$y=$^N})d)(?{$z=$^N})e/ and $y eq "bc" and $z eq "abcd")
              {print $T} else {print "not $T"};
$T="ok 660\n";if ($x =~ /(a([abcdefg]+)(?{$y=$^N})de)(?{$z=$^N})/ and $y eq "bc" and $z eq "abcde")
              {print $T} else {print "not $T"};

# Test the Unicode script classes

print "not " unless chr(0x100) =~ /\p{IsLatin}/; # outside Latin-1
print "ok 661\n";

print "not " unless chr(0x212b) =~ /\p{IsLatin}/; # Angstrom sign, very outside
print "ok 662\n";

print "not " unless chr(0x5d0) =~ /\p{IsHebrew}/; # inside InHebrew
print "ok 663\n";

print "not " unless chr(0xfb4f) =~ /\p{IsHebrew}/; # outside InHebrew
print "ok 664\n";

# singleton (not in a range, this test must be ignored on EBCDIC)
print "not " unless chr(0xb5) =~ /\p{IsGreek}/ or ord("A") == 193;
print "ok 665\n";

print "not " unless chr(0x37a) =~ /\p{IsGreek}/; # singleton
print "ok 666\n";

print "not " unless chr(0x386) =~ /\p{IsGreek}/; # singleton
print "ok 667\n";

print "not " unless chr(0x387) =~ /\P{IsGreek}/; # not there
print "ok 668\n";

print "not " unless chr(0x388) =~ /\p{IsGreek}/; # range
print "ok 669\n";

print "not " unless chr(0x38a) =~ /\p{IsGreek}/; # range
print "ok 670\n";

print "not " unless chr(0x38b) =~ /\P{IsGreek}/; # not there
print "ok 671\n";

print "not " unless chr(0x38c) =~ /\p{IsGreek}/; # singleton
print "ok 672\n";

if (ord("A") == 65) {
##
## Test [:cntrl:]...
##
## Should probably put in tests for all the POSIX stuff, but not sure how to
## guarantee a specific locale......
##
    $AllBytes = join('', map { chr($_) } 0..255);
    ($x = $AllBytes) =~ s/[[:cntrl:]]//g;
    if ($x ne join('', map { chr($_) } 0x20..0x7E, 0x80..0xFF)) {
	print "not ";
    }
    print "ok 673\n";

    ($x = $AllBytes) =~ s/[^[:cntrl:]]//g;
    if ($x ne join('', map { chr($_) } 0..0x1F, 0x7F)) { print "not " }
    print "ok 674\n";
} else {
    print "ok $_ # Skip: EBCDIC\n" for 673..674;
}

# With /s modifier UTF8 chars were interpreted as bytes
{
    my $a = "Hello \x{263A} World";
    
    my @a = ($a =~ /./gs);
    
    print "not " unless $#a == 12;
    print "ok 675\n";
}

@a = ("foo\nbar" =~ /./g);
print "ok 676\n" if @a == 6 && "@a" eq "f o o b a r";

@a = ("foo\nbar" =~ /./gs);
print "ok 677\n" if @a == 7 && "@a" eq "f o o \n b a r";

@a = ("foo\nbar" =~ /\C/g);
print "ok 678\n" if @a == 7 && "@a" eq "f o o \n b a r";

@a = ("foo\nbar" =~ /\C/gs);
print "ok 679\n" if @a == 7 && "@a" eq "f o o \n b a r";

@a = ("foo\n\x{100}bar" =~ /./g);
print "ok 680\n" if @a == 7 && "@a" eq "f o o \x{100} b a r";

@a = ("foo\n\x{100}bar" =~ /./gs);
print "ok 681\n" if @a == 8 && "@a" eq "f o o \n \x{100} b a r";

($a, $b) = map { chr } ord('A') == 65 ? (0xc4, 0x80) : (0x8c, 0x41);

@a = ("foo\n\x{100}bar" =~ /\C/g);
print "ok 682\n" if @a == 9 && "@a" eq "f o o \n $a $b b a r";

@a = ("foo\n\x{100}bar" =~ /\C/gs);
print "ok 683\n" if @a == 9 && "@a" eq "f o o \n $a $b b a r";

{
    # [ID 20010814.004] pos() doesn't work when using =~m// in list context
    $_ = "ababacadaea";
    $a = join ":", /b./gc;
    $b = join ":", /a./gc;
    $c = pos;
    print "$a $b $c" eq 'ba:ba ad:ae 10' ? "ok 684\n" : "not ok 684\t# $a $b $c\n";
}

{
    # [ID 20010407.006] matching utf8 return values from functions does not work

    package ID_20010407_006;

    sub x {
	"a\x{1234}";
    }

    my $x = x;
    my $y;

    $x =~ /(..)/; $y = $1;
    print "not " unless length($y) == 2 && $y eq $x;
    print "ok 685\n";

    x  =~ /(..)/; $y = $1;
    print "not " unless length($y) == 2 && $y eq $x;
    print "ok 686\n";
}


my $test = 687;

# Force scalar context on the patern match
sub ok ($$) {
    my($ok, $name) = @_;

    printf "%sok %d - %s\n", ($ok ? "" : "not "), $test, $name;

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    $test++;
    return $ok;
}

{
    # Check that \x## works. 5.6.1 and 5.005_03 fail some of these.
    $x = "\x4e" . "E";
    ok ($x =~ /^\x4EE$/, "Check only 2 bytes of hex are matched.");

    $x = "\x4e" . "i";
    ok ($x =~ /^\x4Ei$/, "Check that invalid hex digit stops it (2)");

    $x = "\x4" . "j";
    ok ($x =~ /^\x4j$/,  "Check that invalid hex digit stops it (1)");

    $x = "\x0" . "k";
    ok ($x =~ /^\xk$/,   "Check that invalid hex digit stops it (0)");

    $x = "\x0" . "x";
    ok ($x =~ /^\xx$/, "\\xx isn't to be treated as \\0");

    $x = "\x0" . "xa";
    ok ($x =~ /^\xxa$/, "\\xxa isn't to be treated as \\xa");

    $x = "\x9" . "_b";
    ok ($x =~ /^\x9_b$/, "\\x9_b isn't to be treated as \\x9b");

    print "# and now again in [] ranges\n";

    $x = "\x4e" . "E";
    ok ($x =~ /^[\x4EE]{2}$/, "Check only 2 bytes of hex are matched.");

    $x = "\x4e" . "i";
    ok ($x =~ /^[\x4Ei]{2}$/, "Check that invalid hex digit stops it (2)");

    $x = "\x4" . "j";
    ok ($x =~ /^[\x4j]{2}$/,  "Check that invalid hex digit stops it (1)");

    $x = "\x0" . "k";
    ok ($x =~ /^[\xk]{2}$/,   "Check that invalid hex digit stops it (0)");

    $x = "\x0" . "x";
    ok ($x =~ /^[\xx]{2}$/, "\\xx isn't to be treated as \\0");

    $x = "\x0" . "xa";
    ok ($x =~ /^[\xxa]{3}$/, "\\xxa isn't to be treated as \\xa");

    $x = "\x9" . "_b";
    ok ($x =~ /^[\x9_b]{3}$/, "\\x9_b isn't to be treated as \\x9b");

}

{
    # Check that \x{##} works. 5.6.1 fails quite a few of these.

    $x = "\x9b";
    ok ($x =~ /^\x{9_b}$/, "\\x{9_b} is to be treated as \\x9b");

    $x = "\x9b" . "y";
    ok ($x =~ /^\x{9_b}y$/, "\\x{9_b} is to be treated as \\x9b (again)");

    $x = "\x9b" . "y";
    ok ($x =~ /^\x{9b_}y$/, "\\x{9b_} is to be treated as \\x9b");

    $x = "\x9b" . "y";
    ok ($x =~ /^\x{9_bq}y$/, "\\x{9_bc} is to be treated as \\x9b");

    $x = "\x0" . "y";
    ok ($x =~ /^\x{x9b}y$/, "\\x{x9b} is to be treated as \\x0");

    $x = "\x0" . "y";
    ok ($x =~ /^\x{0x9b}y$/, "\\x{0x9b} is to be treated as \\x0");

    $x = "\x9b" . "y";
    ok ($x =~ /^\x{09b}y$/, "\\x{09b} is to be treated as \\x9b");

    print "# and now again in [] ranges\n";

    $x = "\x9b";
    ok ($x =~ /^[\x{9_b}]$/, "\\x{9_b} is to be treated as \\x9b");

    $x = "\x9b" . "y";
    ok ($x =~ /^[\x{9_b}y]{2}$/, "\\x{9_b} is to be treated as \\x9b (again)");

    $x = "\x9b" . "y";
    ok ($x =~ /^[\x{9b_}y]{2}$/, "\\x{9b_} is to be treated as \\x9b");

    $x = "\x9b" . "y";
    ok ($x =~ /^[\x{9_bq}y]{2}$/, "\\x{9_bc} is to be treated as \\x9b");

    $x = "\x0" . "y";
    ok ($x =~ /^[\x{x9b}y]{2}$/, "\\x{x9b} is to be treated as \\x0");

    $x = "\x0" . "y";
    ok ($x =~ /^[\x{0x9b}y]{2}$/, "\\x{0x9b} is to be treated as \\x0");

    $x = "\x9b" . "y";
    ok ($x =~ /^[\x{09b}y]{2}$/, "\\x{09b} is to be treated as \\x9b");
}

{
    # high bit bug -- japhy
    my $x = "ab\200d";
    $x =~ /.*?\200/ or print "not ";
    print "ok 715\n";
}

print "# some Unicode properties\n";

{
    # Dashes, underbars, case.
    print "not " unless "\x80" =~ /\p{in-latin1_SUPPLEMENT}/;
    print "ok 716\n";

    # Complement, leading and trailing whitespace.
    print "not " unless "\x80" =~ /\P{  ^  In Latin 1 Supplement  }/;
    print "ok 717\n";

    # No ^In, dashes, case, dash, any intervening (word-break) whitespace.
    # (well, newlines don't work...)
    print "not " unless "\x80" =~ /\p{latin-1   supplement}/;
    print "ok 718\n";
}

{
    print "not " unless "a" =~ /\pL/;
    print "ok 719\n";

    print "not " unless "a" =~ /\p{IsLl}/;
    print "ok 720\n";

    print "not " if     "a" =~ /\p{IsLu}/;
    print "ok 721\n";

    print "not " unless "a" =~ /\p{Ll}/;
    print "ok 722\n";

    print "not " if     "a" =~ /\p{Lu}/;
    print "ok 723\n";

    print "not " unless "A" =~ /\pL/;
    print "ok 724\n";

    print "not " unless "A" =~ /\p{IsLu}/;
    print "ok 725\n";

    print "not " if     "A" =~ /\p{IsLl}/;
    print "ok 726\n";

    print "not " unless "A" =~ /\p{Lu}/;
    print "ok 727\n";

    print "not " if     "A" =~ /\p{Ll}/;
    print "ok 728\n";

    print "not " if     "a" =~ /\PL/;
    print "ok 729\n";

    print "not " if     "a" =~ /\P{IsLl}/;
    print "ok 730\n";

    print "not " unless "a" =~ /\P{IsLu}/;
    print "ok 731\n";

    print "not " if     "a" =~ /\P{Ll}/;
    print "ok 732\n";

    print "not " unless "a" =~ /\P{Lu}/;
    print "ok 733\n";

    print "not " if     "A" =~ /\PL/;
    print "ok 734\n";

    print "not " if     "A" =~ /\P{IsLu}/;
    print "ok 735\n";

    print "not " unless "A" =~ /\P{IsLl}/;
    print "ok 736\n";

    print "not " if     "A" =~ /\P{Lu}/;
    print "ok 737\n";

    print "not " unless "A" =~ /\P{Ll}/;
    print "ok 738\n";

}

{
    print "not " if     "a" =~ /\p{Common}/;
    print "ok 739\n";

    print "not " unless "1" =~ /\p{Common}/;
    print "ok 740\n";
}

{
    print "not " if     "a"       =~ /\p{Inherited}/;
    print "ok 741\n";

    print "not " unless "\x{300}" =~ /\p{Inherited}/;
    print "ok 742\n";
}

{
    print "not " unless "a" =~ /\p{L&}/;
    print "ok 743\n";

    print "not " if     "1" =~ /\p{L&}/;
    print "ok 744\n";
}

{
    print "not " unless "a" =~ /\p{Lowercase Letter}/;
    print "ok 745\n";

    print "not " if     "A" =~ /\p{lowercaseletter}/;
    print "ok 746\n";
}

{
    print "not " unless "\x{AC00}" =~ /\p{HangulSyllables}/;
    print "ok 747\n";
}

{
    # Script=, Block=, Category=

    print "not " unless "\x{0100}" =~ /\p{Script=Latin}/;
    print "ok 748\n";

    print "not " unless "\x{0100}" =~ /\p{Block=LatinExtendedA}/;
    print "ok 749\n";

    print "not " unless "\x{0100}" =~ /\p{Category=UppercaseLetter}/;
    print "ok 750\n";
}

{
    print "# the basic character classes and Unicode \n";

    # 0100;LATIN CAPITAL LETTER A WITH MACRON;Lu;0;L;0041 0304;;;;N;LATIN CAPITAL LETTER A MACRON;;;0101;
    print "not " unless "\x{0100}" =~ /\w/;
    print "ok 751\n";

    # 0660;ARABIC-INDIC DIGIT ZERO;Nd;0;AN;;0;0;0;N;;;;;
    print "not " unless "\x{0660}" =~ /\d/;
    print "ok 752\n";

    # 1680;OGHAM SPACE MARK;Zs;0;WS;;;;;N;;;;;
    print "not " unless "\x{1680}" =~ /\s/;
    print "ok 753\n";
}

{
    print "# folding matches and Unicode\n";

    print "not " unless "a\x{100}" =~ /A/i;
    print "ok 754\n";

    print "not " unless "A\x{100}" =~ /a/i;
    print "ok 755\n";

    print "not " unless "a\x{100}" =~ /a/i;
    print "ok 756\n";

    print "not " unless "A\x{100}" =~ /A/i;
    print "ok 757\n";

    print "not " unless "\x{101}a" =~ /\x{100}/i;
    print "ok 758\n";

    print "not " unless "\x{100}a" =~ /\x{100}/i;
    print "ok 759\n";

    print "not " unless "\x{101}a" =~ /\x{101}/i;
    print "ok 760\n";

    print "not " unless "\x{100}a" =~ /\x{101}/i;
    print "ok 761\n";

    print "not " unless "a\x{100}" =~ /A\x{100}/i;
    print "ok 762\n";

    print "not " unless "A\x{100}" =~ /a\x{100}/i;
    print "ok 763\n";

    print "not " unless "a\x{100}" =~ /a\x{100}/i;
    print "ok 764\n";

    print "not " unless "A\x{100}" =~ /A\x{100}/i;
    print "ok 765\n";

    print "not " unless "a\x{100}" =~ /[A]/i;
    print "ok 766\n";

    print "not " unless "A\x{100}" =~ /[a]/i;
    print "ok 767\n";

    print "not " unless "a\x{100}" =~ /[a]/i;
    print "ok 768\n";

    print "not " unless "A\x{100}" =~ /[A]/i;
    print "ok 769\n";

    print "not " unless "\x{101}a" =~ /[\x{100}]/i;
    print "ok 770\n";

    print "not " unless "\x{100}a" =~ /[\x{100}]/i;
    print "ok 771\n";

    print "not " unless "\x{101}a" =~ /[\x{101}]/i;
    print "ok 772\n";

    print "not " unless "\x{100}a" =~ /[\x{101}]/i;
    print "ok 773\n";

}

{
    use charnames ':full';

    print "# LATIN LETTER A WITH GRAVE\n";
    my $lower = "\N{LATIN SMALL LETTER A WITH GRAVE}";
    my $UPPER = "\N{LATIN CAPITAL LETTER A WITH GRAVE}";

    print $lower =~ m/$UPPER/i   ? "ok 774\n" : "not ok 774\n";
    print $UPPER =~ m/$lower/i   ? "ok 775\n" : "not ok 775\n";
    print $lower =~ m/[$UPPER]/i ? "ok 776\n" : "not ok 776\n";
    print $UPPER =~ m/[$lower]/i ? "ok 777\n" : "not ok 777\n";

    print "# GREEK LETTER ALPHA WITH VRACHY\n";

    $lower = "\N{GREEK CAPITAL LETTER ALPHA WITH VRACHY}";
    $UPPER = "\N{GREEK SMALL LETTER ALPHA WITH VRACHY}";

    print $lower =~ m/$UPPER/i   ? "ok 778\n" : "not ok 778\n";
    print $UPPER =~ m/$lower/i   ? "ok 779\n" : "not ok 779\n";
    print $lower =~ m/[$UPPER]/i ? "ok 780\n" : "not ok 780\n";
    print $UPPER =~ m/[$lower]/i ? "ok 781\n" : "not ok 781\n";

    print "# LATIN LETTER Y WITH DIAERESIS\n";

    $lower = "\N{LATIN CAPITAL LETTER Y WITH DIAERESIS}";
    $UPPER = "\N{LATIN SMALL LETTER Y WITH DIAERESIS}";
    print $lower =~ m/$UPPER/i   ? "ok 782\n" : "not ok 782\n";
    print $UPPER =~ m/$lower/i   ? "ok 783\n" : "not ok 783\n";
    print $lower =~ m/[$UPPER]/i ? "ok 784\n" : "not ok 784\n";
    print $UPPER =~ m/[$lower]/i ? "ok 785\n" : "not ok 785\n";
}

{
    use warnings;
    use charnames ':full';
    
    print "# GREEK CAPITAL LETTER SIGMA vs COMBINING GREEK PERISPOMENI\n";

    my $SIGMA = "\N{GREEK CAPITAL LETTER SIGMA}";
    my $char  = "\N{COMBINING GREEK PERISPOMENI}";

    # Before #13843 this was failing by matching falsely.
    print "_:$char:_" =~ m/_:$SIGMA:_/i ? "not ok 786\n" : "ok 786\n";
}

{
    print "# \\X\n";

    use charnames ':full';

    print "a!"              =~ /^(\X)!/ && $1 eq "a" ?
	"ok 787\n" : "not ok 787 # $1\n";
    print "\xDF!"           =~ /^(\X)!/ && $1 eq "\xDF" ?
	"ok 788\n" : "not ok 788 # $1\n";
    print "\x{100}!"        =~ /^(\X)!/ && $1 eq "\x{100}" ?
	"ok 789\n" : "not ok 789 # $1\n";
    print "\x{100}\x{300}!" =~ /^(\X)!/ && $1 eq "\x{100}\x{300}" ?
	"ok 790\n" : "not ok 790 # $1\n";
    print "\N{LATIN CAPITAL LETTER E}!" =~ /^(\X)!/ &&
	$1 eq "\N{LATIN CAPITAL LETTER E}" ?
	"ok 791\n" : "not ok 791 # $1\n";
    print "\N{LATIN CAPITAL LETTER E}\N{COMBINING GRAVE ACCENT}!" =~
	/^(\X)!/ &&
	$1 eq "\N{LATIN CAPITAL LETTER E}\N{COMBINING GRAVE ACCENT}" ?
	"ok 792\n" : "not ok 792 # $1\n";
}

{
    print "#\\C and \\X\n";

    print "!abc!" =~ /a\Cc/ ? "ok 793\n" : "not ok 793\n";
    print "!abc!" =~ /a\Xc/ ? "ok 794\n" : "not ok 794\n";
}

{
    print "# FINAL SIGMA\n";

    my $SIGMA = "\x{03A3}"; # CAPITAL
    my $Sigma = "\x{03C2}"; # SMALL FINAL
    my $sigma = "\x{03C3}"; # SMALL

    print $SIGMA =~ /$SIGMA/i ? "ok 795\n" : "not ok 795\n";
    print $SIGMA =~ /$Sigma/i ? "ok 796\n" : "not ok 796\n";
    print $SIGMA =~ /$sigma/i ? "ok 797\n" : "not ok 797\n";

    print $Sigma =~ /$SIGMA/i ? "ok 798\n" : "not ok 798\n";
    print $Sigma =~ /$Sigma/i ? "ok 799\n" : "not ok 799\n";
    print $Sigma =~ /$sigma/i ? "ok 800\n" : "not ok 800\n";

    print $sigma =~ /$SIGMA/i ? "ok 801\n" : "not ok 801\n";
    print $sigma =~ /$Sigma/i ? "ok 802\n" : "not ok 802\n";
    print $sigma =~ /$sigma/i ? "ok 803\n" : "not ok 803\n";
    
    print $SIGMA =~ /[$SIGMA]/i ? "ok 804\n" : "not ok 804\n";
    print $SIGMA =~ /[$Sigma]/i ? "ok 805\n" : "not ok 805\n";
    print $SIGMA =~ /[$sigma]/i ? "ok 806\n" : "not ok 806\n";

    print $Sigma =~ /[$SIGMA]/i ? "ok 807\n" : "not ok 807\n";
    print $Sigma =~ /[$Sigma]/i ? "ok 808\n" : "not ok 808\n";
    print $Sigma =~ /[$sigma]/i ? "ok 809\n" : "not ok 809\n";

    print $sigma =~ /[$SIGMA]/i ? "ok 810\n" : "not ok 810\n";
    print $sigma =~ /[$Sigma]/i ? "ok 811\n" : "not ok 811\n";
    print $sigma =~ /[$sigma]/i ? "ok 812\n" : "not ok 812\n";
}

{
    print "# parlez-vous?\n";

    use charnames ':full';

    print "fran\N{LATIN SMALL LETTER C}ais" =~
	  /fran.ais/ &&
	$& eq "francais" ?
	"ok 813\n" : "not ok 813\n";

    print "fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" =~
	  /fran.ais/ &&
	$& eq "fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" ?
	"ok 814\n" : "not ok 814\n";

    print "fran\N{LATIN SMALL LETTER C}ais" =~
	   /fran\Cais/ &&
        $& eq "francais" ?
	"ok 815\n" : "not ok 815\n";

    print "franc\N{COMBINING CEDILLA}ais" =~
	  /franc\C\Cais/ ? # COMBINING CEDILLA is two bytes when encoded
	"ok 816\n" : "not ok 816\n";

    print "fran\N{LATIN SMALL LETTER C}ais" =~
	  /fran\Xais/ &&
	$& eq "francais" ?
	"ok 817\n" : "not ok 817\n";

    print "fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" =~
	  /fran\Xais/  &&
        $& eq "fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" ?
	"ok 818\n" : "not ok 818\n";

    print "franc\N{COMBINING CEDILLA}ais" =~
	  /fran\Xais/ &&
         $& eq "franc\N{COMBINING CEDILLA}ais" ?
	 "ok 819\n" : "not ok 819\n";

    print "fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" =~
	  /fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais/  &&
        $& eq "fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" ?
	"ok 820\n" : "not ok 820\n";

    print "franc\N{COMBINING CEDILLA}ais" =~
	  /franc\N{COMBINING CEDILLA}ais/  &&
        $& eq "franc\N{COMBINING CEDILLA}ais" ?
	"ok 821\n" : "not ok 821\n";

    print "fran\N{LATIN SMALL LETTER C}ais" =~
	  /fran(?:c\N{COMBINING CEDILLA}?|\N{LATIN SMALL LETTER C WITH CEDILLA})ais/ &&
	$& eq "francais" ?
	"ok 822\n" : "not ok 822\n";

    print "fran\N{LATIN SMALL LETTER C}ais" =~
	  /fran(?:c\N{COMBINING CEDILLA}?|\N{LATIN SMALL LETTER C WITH CEDILLA})ais/ &&
	$& eq "francais" ?
	"ok 823\n" : "not ok 823\n";

    print "fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" =~
	  /fran(?:c\N{COMBINING CEDILLA}?|\N{LATIN SMALL LETTER C WITH CEDILLA})ais/ &&
	$& eq "fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" ?
	"ok 824\n" : "not ok 824\n";

    print "franc\N{COMBINING CEDILLA}ais" =~
	  /fran(?:c\N{COMBINING CEDILLA}?|\N{LATIN SMALL LETTER C WITH CEDILLA})ais/ &&
	$& eq "franc\N{COMBINING CEDILLA}ais" ?
	"ok 825\n" : "not ok 825\n";
}

{
    print "# Does lingering (and useless) UTF8 flag mess up /i matching?\n";

    {
	my $regex  = "ABcde";
	my $string = "abcDE\x{100}";
	chop($string);
	if ($string =~ m/$regex/i) {
	    print "ok 826\n";
	} else {
	    print "not ok 826\n";
	}
    }

    {
	my $regex  = "ABcde\x{100}";
	my $string = "abcDE";
	chop($regex);
	if ($string =~ m/$regex/i) {
	    print "ok 827\n";
	} else {
	    print "not ok 827\n";
	}
    }

    {
	my $regex  = "ABcde\x{100}";
	my $string = "abcDE\x{100}";
	chop($regex);
	chop($string);
	if ($string =~ m/$regex/i) {
	    print "ok 828\n";
	} else {
	    print "not ok 828\n";
	}
    }
}

{
    print "# more SIGMAs\n";

    my $SIGMA = "\x{03A3}"; # CAPITAL
    my $Sigma = "\x{03C2}"; # SMALL FINAL
    my $sigma = "\x{03C3}"; # SMALL

    my $S3 = "$SIGMA$Sigma$sigma";

    print ":$S3:" =~ /:(($SIGMA)+):/i   && $1 eq $S3 && $2 eq $sigma ?
	"ok 829\n" : "not ok 829\n";
    print ":$S3:" =~ /:(($Sigma)+):/i   && $1 eq $S3 && $2 eq $sigma ?
	"ok 830\n" : "not ok 830\n";
    print ":$S3:" =~ /:(($sigma)+):/i   && $1 eq $S3 && $2 eq $sigma ?
	"ok 831\n" : "not ok 831\n";

    print ":$S3:" =~ /:(([$SIGMA])+):/i && $1 eq $S3 && $2 eq $sigma ?
	"ok 832\n" : "not ok 832\n";
    print ":$S3:" =~ /:(([$Sigma])+):/i && $1 eq $S3 && $2 eq $sigma ?
	"ok 833\n" : "not ok 833\n";
    print ":$S3:" =~ /:(([$sigma])+):/i && $1 eq $S3 && $2 eq $sigma ?
	"ok 834\n" : "not ok 834\n";
}

{
    print "# LATIN SMALL LETTER SHARP S\n";

    use charnames ':full';

    print "\N{LATIN SMALL LETTER SHARP S}" =~
	/\N{LATIN SMALL LETTER SHARP S}/    ? "ok 835\n" : "not ok 835\n";

    print "\N{LATIN SMALL LETTER SHARP S}" =~
	/\N{LATIN SMALL LETTER SHARP S}/i   ? "ok 836\n" : "not ok 836\n";

    print "\N{LATIN SMALL LETTER SHARP S}" =~
	/[\N{LATIN SMALL LETTER SHARP S}]/  ? "ok 837\n" : "not ok 837\n";

    print "\N{LATIN SMALL LETTER SHARP S}" =~
	/[\N{LATIN SMALL LETTER SHARP S}]/i ? "ok 838\n" : "not ok 838\n";

    print "ss" =~
	/\N{LATIN SMALL LETTER SHARP S}/i   ? "ok 839\n" : "not ok 839\n";

    print "SS" =~
	/\N{LATIN SMALL LETTER SHARP S}/i   ? "ok 840\n" : "not ok 840\n";

    print "ss" =~
	/[\N{LATIN SMALL LETTER SHARP S}]/i ? "ok 841\n" : "not ok 841\n";

    print "SS" =~
	/[\N{LATIN SMALL LETTER SHARP S}]/i ? "ok 842\n" : "not ok 842\n";

    print "\N{LATIN SMALL LETTER SHARP S}" =~ /ss/i ?
	"ok 843\n" : "not ok 843\n";

    print "\N{LATIN SMALL LETTER SHARP S}" =~ /SS/i ?
	"ok 844\n" : "not ok 844\n";
}

{
    print "# more whitespace: U+0085, U+2028, U+2029\n";

    # U+0085 needs to be forced to be Unicode, the \x{100} does that.
    print "<\x{100}\x{0085}>" =~ /<\x{100}\s>/ ? "ok 845\n" : "not ok 845\n";
    print "<\x{2028}>" =~ /<\s>/ ? "ok 846\n" : "not ok 846\n";
    print "<\x{2029}>" =~ /<\s>/ ? "ok 847\n" : "not ok 847\n";
}

{
    print "# . with /s should work on characters, as opposed to bytes\n";

    my $s = "\x{e4}\x{100}";

    # This is not expected to match: the point is that
    # neither should we get "Malformed UTF-8" warnings.
    print $s =~ /\G(.+?)\n/gcs ?
	"not ok 848\n" : "ok 848\n";

    my @c;

    while ($s =~ /\G(.)/gs) {
	push @c, $1;
    }

    print join("", @c) eq $s ? "ok 849\n" : "not ok 849\n";

    my $t1 = "Q003\n\n\x{e4}\x{f6}\n\nQ004\n\n\x{e7}"; # test only chars < 256
    my $r1 = "";
    while ($t1 =~ / \G ( .+? ) \n\s+ ( .+? ) ( $ | \n\s+ ) /xgcs) {
	$r1 .= $1 . $2;
    }

    my $t2 = $t1 . "\x{100}"; # repeat with a larger char
    my $r2 = "";
    while ($t2 =~ / \G ( .+? ) \n\s+ ( .+? ) ( $ | \n\s+ ) /xgcs) {
	$r2 .= $1 . $2;
    }
    $r2 =~ s/\x{100}//;
    print $r1 eq $r2 ? "ok 850\n" : "not ok 850\n";
}

{
    print "# Unicode lookbehind\n";

    print "A\x{100}B"        =~ /(?<=A.)B/  ? "ok 851\n" : "not ok 851\n";
    print "A\x{200}\x{300}B" =~ /(?<=A..)B/ ? "ok 852\n" : "not ok 852\n";
    print "\x{400}AB"        =~ /(?<=\x{400}.)B/ ? "ok 853\n" : "not ok 853\n";
    print "\x{500\x{600}}B"  =~ /(?<=\x{500}.)B/ ? "ok 854\n" : "not ok 854\n";
}

{
    print "# UTF-8 hash keys and /\$/\n";
    # http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2002-01/msg01327.html

    my $u = "a\x{100}";
    my $v = substr($u,0,1);
    my $w = substr($u,1,1);
    my %u = ( $u => $u, $v => $v, $w => $w );
    my $i = 855; 
    for (keys %u) {
	my $m1 = /^\w*$/ ? 1 : 0;
	my $m2 = $u{$_}=~/^\w*$/ ? 1 : 0;
	print $m1 == $m2 ? "ok $i\n" : "not ok $i # $m1 $m2\n";
	$i++;
    }
}

{
    print "# [ID 20020124.005]\n";
    # Fixed by #14795.
    my $i = 858;
    for my $char ("a", "\x{df}", "\x{100}"){
	$x = "$char b $char";
	$x =~ s{($char)}{
	    "c" =~ /c/;
	    "x";
	}ge;
	print substr($x,0,1) eq substr($x,-1,1) ?
	    "ok $i\n" : "not ok $i # debug: $x\n";
 	$i++;
   }
}

{
    print "# SEGV in s/// and UTF-8\n";
    $s = "s#\x{100}" x 4;
    $s =~ s/[^\w]/ /g;
    print $s eq "s \x{100}" x 4 ? "ok 861\n" : "not ok 861\n";
}

{
    print "# UTF-8 bug (maybe alreayd known?)\n";
    my $u;

    $u = "foo";
    $u =~ s/./\x{100}/g;
    print $u eq "\x{100}\x{100}\x{100}" ? "ok 862\n" : "not ok 862\n";

    $u = "foobar";
    $u =~ s/[ao]/\x{100}/g;
    print $u eq "f\x{100}\x{100}b\x{100}r" ? "ok 863\n" : "not ok 863\n";

    $u =~ s/\x{100}/e/g;
    print $u eq "feeber" ? "ok 864\n" : "not ok 864\n";
}

{
    print "# UTF-8 bug with s///\n";
    # check utf8/non-utf8 mixtures
    # try to force all float/anchored check combinations
    my $c = "\x{100}";
    my $test = 865;
    my $subst;
    for my $re (
	"xx.*$c", "x.*$c$c", "$c.*xx", "$c$c.*x", "xx.*(?=$c)", "(?=$c).*xx",
    ) {
	print "xxx" =~ /$re/ ? "not ok $test\n" : "ok $test\n";
	++$test;
	print +($subst = "xxx") =~ s/$re// ? "not ok $test\n" : "ok $test\n";
	++$test;
    }
    for my $re ("xx.*$c*", "$c*.*xx") {
	print "xxx" =~ /$re/ ? "ok $test\n" : "not ok $test\n";
	++$test;
	($subst = "xxx") =~ s/$re//;
	print $subst eq '' ? "ok $test\n" : "not ok $test\t# $subst\n";
	++$test;
    }
    for my $re ("xxy*", "y*xx") {
	print "xx$c" =~ /$re/ ? "ok $test\n" : "not ok $test\n";
	++$test;
	($subst = "xx$c") =~ s/$re//;
	print $subst eq $c ? "ok $test\n" : "not ok $test\n";
	++$test;
	print "xy$c" =~ /$re/ ? "not ok $test\n" : "ok $test\n";
	++$test;
	print +($subst = "xy$c") =~ /$re/ ? "not ok $test\n" : "ok $test\n";
	++$test;
    }
    for my $re ("xy$c*z", "x$c*yz") {
	print "xyz" =~ /$re/ ? "ok $test\n" : "not ok $test\n";
	++$test;
	($subst = "xyz") =~ s/$re//;
	print $subst eq '' ? "ok $test\n" : "not ok $test\n";
	++$test;
    }
}

{
    print "# qr/.../x\n";
    my $test = 893;

    my $R = qr/ A B C # D E/x;

    print eval {"ABCDE" =~ $R} ? "ok $test\n" : "not ok $test\n";
    $test++;

    print eval {"ABCDE" =~ m/$R/} ? "ok $test\n" : "not ok $test\n";
    $test++;

    print eval {"ABCDE" =~ m/($R)/} ? "ok $test\n" : "not ok $test\n";
    $test++;
}

{
    print "# illegal Unicode properties\n";
    my $test = 896;

    print eval { "a" =~ /\pq / }      ? "not ok $test\n" : "ok $test\n";
    $test++;

    print eval { "a" =~ /\p{qrst} / } ? "not ok $test\n" : "ok $test\n";
    $test++;
}

{
    print "# [ID 20020412.005] wrong pmop flags checked when empty pattern\n";
    # requires reuse of last successful pattern
    my $test = 898;
    $test =~ /\d/;
    for (0 .. 1) {
	my $match = ?? + 0;
	if ($match != $_) {
	    print "ok $test\n";
	} else {
	    printf "not ok %s\t# 'match once' %s on %s iteration\n", $test,
		    $match ? 'succeeded' : 'failed', $_ ? 'second' : 'first';
	}
	++$test;
    }
    $test =~ /(\d)/;
    my $result = join '', $test =~ //g;
    if ($result eq $test) {
	print "ok $test\n";
    } else {
	printf "not ok %s\t# expected '%s', got '%s'\n", $test, $test, $result;
    }
    ++$test;
}

print "# user-defined character properties\n";

sub InKana1 {
    return <<'END';
3040	309F
30A0	30FF
END
}

sub InKana2 {
    return <<'END';
+utf8::InHiragana
+utf8::InKatakana
END
}

sub InKana3 {
    return <<'END';
+utf8::InHiragana
+utf8::InKatakana
-utf8::IsCn
END
}

sub InNotKana {
    return <<'END';
!utf8::InHiragana
-utf8::InKatakana
+utf8::IsCn
END
}

$test = 901;

print "\x{3040}" =~ /\p{InKana1}/ ? "ok $test\n" : "not ok $test\n"; $test++;
print "\x{303F}" =~ /\P{InKana1}/ ? "ok $test\n" : "not ok $test\n"; $test++;

print "\x{3040}" =~ /\p{InKana2}/ ? "ok $test\n" : "not ok $test\n"; $test++;
print "\x{303F}" =~ /\P{InKana2}/ ? "ok $test\n" : "not ok $test\n"; $test++;

print "\x{3041}" =~ /\p{InKana3}/ ? "ok $test\n" : "not ok $test\n"; $test++;
print "\x{3040}" =~ /\P{InKana3}/ ? "ok $test\n" : "not ok $test\n"; $test++;

print "\x{3040}" =~ /\p{InNotKana}/ ? "ok $test\n" : "not ok $test\n"; $test++;
print "\x{3041}" =~ /\P{InNotKana}/ ? "ok $test\n" : "not ok $test\n"; $test++;

sub InConsonant { # Not EBCDIC-aware.
    return <<EOF;
0061	007f
-0061
-0065
-0069
-006f
-0075
EOF
}

print "d" =~ /\p{InConsonant}/ ? "ok $test\n" : "not ok $test\n"; $test++;
print "e" =~ /\P{InConsonant}/ ? "ok $test\n" : "not ok $test\n"; $test++;

{
    print "# [ID 20020630.002] utf8 regex only matches 32k\n";
    $test = 911;
    for ([ 'byte', "\x{ff}" ], [ 'utf8', "\x{1ff}" ]) {
	my($type, $char) = @$_;
	for my $len (32000, 32768, 33000) {
	    my $s = $char . "f" x $len;
	    my $r = $s =~ /$char([f]*)/gc;
	    print $r ? "ok $test\n" : "not ok $test\t# <$type x $len> fail\n";
	    ++$test;
	    print +(!$r or pos($s) == $len + 1) ? "ok $test\n"
		: "not ok $test\t# <$type x $len> pos @{[ pos($s) ]}\n";
	    ++$test;
	}
    }
}

$test = 923;

$a = bless qr/foo/, 'Foo';
print(('goodfood' =~ $a ? '' : 'not '),
	"ok $test\t# reblessed qr// matches\n");
++$test;

print(($a eq '(?-xism:foo)' ? '' : 'not '),
	"ok $test\t# reblessed qr// stringizes\n");
++$test;

$x = "\x{3fe}";
$z=$y = "\317\276"; # $y is byte representation of $x

$a = qr/$x/;
print(($x =~ $a ? '' : 'not '), "ok $test - utf8 interpolation in qr//\n");
++$test;

print(("a$a" =~ $x ? '' : 'not '),
      "ok $test - stringifed qr// preserves utf8\n");
++$test;

print(("a$x" =~ /^a$a\z/ ? '' : 'not '),
      "ok $test - interpolated qr// preserves utf8\n");
++$test;

print(("a$x" =~ /^a(??{$a})\z/ ? '' : 'not '),
      "ok $test - postponed interpolation of qr// preserves utf8\n");
++$test;

print((length(qr/##/x) == 12 ? '' : 'not '),
      "ok $test - ## in qr// doesn't corrupt memory [perl #17776]\n");
++$test;

{ use re 'eval';

print(("$x$x" =~ /^$x(??{$x})\z/ ? '' : 'not '),
      "ok $test - postponed utf8 string in utf8 re matches utf8\n");
++$test;

print(("$y$x" =~ /^$y(??{$x})\z/ ? '' : 'not '),
      "ok $test - postponed utf8 string in non-utf8 re matches utf8\n");
++$test;

print(("$y$x" !~ /^$y(??{$y})\z/ ? '' : 'not '),
      "ok $test - postponed non-utf8 string in non-utf8 re doesn't match utf8\n");
++$test;

print(("$x$x" !~ /^$x(??{$y})\z/ ? '' : 'not '),
      "ok $test - postponed non-utf8 string in utf8 re doesn't match utf8\n");
++$test;

print(("$y$y" =~ /^$y(??{$y})\z/ ? '' : 'not '),
      "ok $test - postponed non-utf8 string in non-utf8 re matches non-utf8\n");
++$test;

print(("$x$y" =~ /^$x(??{$y})\z/ ? '' : 'not '),
      "ok $test - postponed non-utf8 string in utf8 re matches non-utf8\n");
++$test;
$y = $z; # reset $y after upgrade

print(("$x$y" !~ /^$x(??{$x})\z/ ? '' : 'not '),
      "ok $test - postponed utf8 string in utf8 re doesn't match non-utf8\n");
++$test;
$y = $z; # reset $y after upgrade

print(("$y$y" !~ /^$y(??{$x})\z/ ? '' : 'not '),
      "ok $test - postponed utf8 string in non-utf8 re doesn't match non-utf8\n");
++$test;

} # no re 'eval'

print "# more user-defined character properties\n";

sub IsSyriac1 {
    return <<'END';
0712	072C
0730	074A
END
}

print "\x{0712}" =~ /\p{IsSyriac1}/ ? "ok $test\n" : "not ok $test\n"; $test++;
print "\x{072F}" =~ /\P{IsSyriac1}/ ? "ok $test\n" : "not ok $test\n"; $test++;

sub Syriac1 {
    return <<'END';
0712	072C
0730	074A
END
}

print "\x{0712}" =~ /\p{Syriac1}/ ? "ok $test\n" : "not ok $test\n"; $test++;
print "\x{072F}" =~ /\P{Syriac1}/ ? "ok $test\n" : "not ok $test\n"; $test++;

{
    print "# Change #18179\n";
    # previously failed with "panic: end_shift
    my $s = "\x{100}" x 5;
    my $ok = $s =~ /(\x{100}{4})/;
    my($ord, $len) = (ord $1, length $1);
    print +($ok && $ord == 0x100 && $len == 4)
	    ? "ok $test\n" : "not ok $test\t# $ok/$ord/$len\n";
    ++$test;
}

{
    print "# [perl #15763]\n";

    $a = "x\x{100}";
    chop $a; # but leaves the UTF-8 flag
    $a .= "y"; # 1 byte before "y"

    ok($a =~ /^\C/,      'match one \C on 1-byte UTF-8');
    ok($a =~ /^\C{1}/,   'match \C{1}');

    ok($a =~ /^\Cy/,      'match \Cy');
    ok($a =~ /^\C{1}y/,   'match \C{1}y');

    $a = "\x{100}y"; # 2 bytes before "y"

    ok($a =~ /^\C/,       'match one \C on 2-byte UTF-8');
    ok($a =~ /^\C{1}/,    'match \C{1}');
    ok($a =~ /^\C\C/,     'match two \C');
    ok($a =~ /^\C{2}/,    'match \C{2}');

    ok($a =~ /^\C\C\C/,    'match three \C on 2-byte UTF-8 and a byte');
    ok($a =~ /^\C{3}/,     'match \C{3}');

    ok($a =~ /^\C\Cy/,     'match two \C');
    ok($a =~ /^\C{2}y/,    'match \C{2}');

    ok($a !~ /^\C\C\Cy/,    q{don't match three \Cy});
    ok($a !~ /^\C{2}\Cy/,   q{don't match \C{3}y});

    $a = "\x{1000}y"; # 3 bytes before "y"

    ok($a =~ /^\C/,         'match one \C on three-byte UTF-8');
    ok($a =~ /^\C{1}/,      'match \C{1}');
    ok($a =~ /^\C\C/,       'match two \C');
    ok($a =~ /^\C{2}/,      'match \C{2}');
    ok($a =~ /^\C\C\C/,     'match three \C');
    ok($a =~ /^\C{3}/,      'match \C{3}');

    ok($a =~ /^\C\C\C\C/,   'match four \C on three-byte UTF-8 and a byte');
    ok($a =~ /^\C{4}/,      'match \C{4}');

    ok($a =~ /^\C\C\Cy/,    'match three \Cy');
    ok($a =~ /^\C{3}y/,     'match \C{3}y');

    ok($a !~ /^\C\C\C\C\y/, q{don't match four \Cy});
    ok($a !~ /^\C{4}y/,     q{don't match \C{4}y});
}

$_ = 'aaaaaaaaaa';
utf8::upgrade($_); chop $_; $\="\n";
ok(/[^\s]+/, "m/[^\s]/ utf8");
ok(/[^\d]+/, "m/[^\d]/ utf8");
ok(($a = $_, $_ =~ s/[^\s]+/./g), "s/[^\s]/ utf8");
ok(($a = $_, $a =~ s/[^\d]+/./g), "s/[^\s]/ utf8");

ok("\x{100}" =~ /\x{100}/, "[perl #15397]");
ok("\x{100}" =~ /(\x{100})/, "[perl #15397]");
ok("\x{100}" =~ /(\x{100}){1}/, "[perl #15397]");
ok("\x{100}\x{100}" =~ /(\x{100}){2}/, "[perl #15397]");
ok("\x{100}\x{100}" =~ /(\x{100})(\x{100})/, "[perl #15397]");

$x = "CD";
$x =~ /(AB)*?CD/;
ok(!defined $1, "[perl #7471]");

$x = "CD";
$x =~ /(AB)*CD/;
ok(!defined $1, "[perl #7471]");

$pattern = "^(b+?|a){1,2}c";
ok("bac"    =~ /$pattern/ && $1 eq 'a', "[perl #3547]");
ok("bbac"   =~ /$pattern/ && $1 eq 'a', "[perl #3547]");
ok("bbbac"  =~ /$pattern/ && $1 eq 'a', "[perl #3547]");
ok("bbbbac" =~ /$pattern/ && $1 eq 'a', "[perl #3547]");

{
    # [perl #18232]
    "\x{100}" =~ /(.)/;
    ok( $1 eq "\x{100}", '$1 is utf-8 [perl #18232]' );
    { 'a' =~ /./; }
    ok( $1 eq "\x{100}", '$1 is still utf-8' );
    ok( $1 ne "\xC4\x80", '$1 is not non-utf-8' );
}

{
    use utf8;
    my $attr = 'Name-1' ;

    my $NormalChar          = qr/[\p{IsDigit}\p{IsLower}\p{IsUpper}]/;
    my $NormalWord          = qr/${NormalChar}+?/;
    my $PredNameHyphen      = qr/^${NormalWord}(\-${NormalWord})*?$/;

    $attr =~ /^$/;
    ok( $attr =~ $PredNameHyphen, "[perl #19767] original test" );
}

{
    use utf8;
    "a" =~ m/[b]/;
    ok ( "0" =~ /\p{N}+\z/, "[perl #19767] variant test" );
}

{

    $p = 1;
    foreach (1,2,3,4) {
	    $p++ if /(??{ $p })/
    }
    ok ($p == 5, "[perl #20683] (??{ }) returns stale values");
    { package P; $a=1; sub TIESCALAR { bless[] } sub FETCH { $a++ } }
    tie $p, P;
    foreach (1,2,3,4) {
	    /(??{ $p })/
    }
    ok ( $p == 5, "(??{ }) returns stale values");
}

{
  # Subject: Odd regexp behavior
  # From: Markus Kuhn <Markus.Kuhn@cl.cam.ac.uk>
  # Date: Wed, 26 Feb 2003 16:53:12 +0000
  # Message-Id: <E18o4nw-0008Ly-00@wisbech.cl.cam.ac.uk>
  # To: perl-unicode@perl.org
    
  $x = "\x{2019}\nk"; $x =~ s/(\S)\n(\S)/$1 $2/sg;
  ok($x eq "\x{2019} k", "Markus Kuhn 2003-02-26");

  $x = "b\nk"; $x =~ s/(\S)\n(\S)/$1 $2/sg;
  ok($x eq "b k", "Markus Kuhn 2003-02-26");

  ok("\x{2019}" =~ /\S/, "Markus Kuhn 2003-02-26");
}

{
    my $i;
    ok('-1-3-5-' eq join('', split /((??{$i++}))/, '-1-3-5-'),
	"[perl #21411] (??{ .. }) corrupts split's stack");
    split /(?{'WOW'})/, 'abc';
    ok('a|b|c' eq join ('|', @_),
       "[perl #21411] (?{ .. }) version of the above");
}

{
    split /(?{ split "" })/, "abc";
    ok(1,'cache_re & "(?{": it dumps core in 5.6.1 & 5.8.0');
}

{
    ok("\x{100}\n" =~ /\x{100}\n$/, "UTF8 length cache and fbm_compile");  
}

{
    package Str;
    use overload q/""/ => sub { ${$_[0]}; };
    sub new { my ($c, $v) = @_; bless \$v, $c; }

    package main;
    $_ = Str->new("a\x{100}/\x{100}b");
    ok(join(":", /\b(.)\x{100}/g) eq "a:/", "re_intuit_start and PL_bostr");
}

{
    $_ = "code:   'x' { '...' }\n"; study;
    my @x; push @x, $& while m/'[^\']*'/gx;
    ok(join(":", @x) eq "'x':'...'",
       "[perl #17757] Parse::RecDescent triggers infinite loop");
}

{
    my $re = qq/^([^X]*)X/;
    utf8::upgrade($re);
    ok("\x{100}X" =~ /$re/, "S_cl_and ANYOF_UNICODE & ANYOF_INVERTED");
}

ok(1, 'skip - $* not deprecated in Perl 5.8') for 1..6;

# bug #19049
$_="abcdef\n";
@x = m/./g;
ok("abcde" eq "$`", '# TODO #19049 - global match not setting $`');

ok("123\x{100}" =~ /^.*1.*23\x{100}$/, 'uft8 + multiple floating substr');

# LATIN SMALL/CAPITAL LETTER A WITH MACRON
ok("  \x{101}" =~ qr/\x{100}/i,
   "<20030808193656.5109.1@llama.ni-s.u-net.com>");

# LATIN SMALL/CAPITAL LETTER A WITH RING BELOW
ok("  \x{1E01}" =~ qr/\x{1E00}/i,
   "<20030808193656.5109.1@llama.ni-s.u-net.com>");

# DESERET SMALL/CAPITAL LETTER LONG I
ok("  \x{10428}" =~ qr/\x{10400}/i,
   "<20030808193656.5109.1@llama.ni-s.u-net.com>");

# LATIN SMALL/CAPITAL LETTER A WITH RING BELOW + 'X'
ok("  \x{1E01}x" =~ qr/\x{1E00}X/i,
   "<20030808193656.5109.1@llama.ni-s.u-net.com>");

{
    # [perl #23769] Unicode regex broken on simple example
    # regrepeat() didn't handle UTF-8 EXACT case right.

    my $s = "\x{a0}\x{a0}\x{a0}\x{100}"; chop $s;

    ok($s =~ /\x{a0}/,       "[perl #23769]");
    ok($s =~ /\x{a0}+/,      "[perl #23769]");
    ok($s =~ /\x{a0}\x{a0}/, "[perl #23769]");

    ok("aaa\x{100}" =~ /(a+)/, "[perl #23769] easy invariant");
    ok($1 eq "aaa", "[perl #23769]");

    ok("\xa0\xa0\xa0\x{100}" =~ /(\xa0+)/, "[perl #23769] regrepeat invariant");
    ok($1 eq "\xa0\xa0\xa0", "[perl #23769]");

    ok("ababab\x{100}  " =~ /((?:ab)+)/, "[perl #23769] hard invariant");
    ok($1 eq "ababab", "[perl #23769]");

    ok("\xa0\xa1\xa0\xa1\xa0\xa1\x{100}" =~ /((?:\xa0\xa1)+)/, "[perl #23769] hard variant");
    ok($1 eq "\xa0\xa1\xa0\xa1\xa0\xa1", "[perl #23769]");

    ok("aaa\x{100}     " =~ /(a+?)/, "[perl #23769] easy invariant");
    ok($1 eq "a", "[perl #23769]");

    ok("\xa0\xa0\xa0\x{100}    " =~ /(\xa0+?)/, "[perl #23769] regrepeat variant");
    ok($1 eq "\xa0", "[perl #23769]");

    ok("ababab\x{100}  " =~ /((?:ab)+?)/, "[perl #23769] hard invariant");
    ok($1 eq "ab", "[perl #23769]");

    ok("\xa0\xa1\xa0\xa1\xa0\xa1\x{100}" =~ /((?:\xa0\xa1)+?)/, "[perl #23769] hard variant");
    ok($1 eq "\xa0\xa1", "[perl #23769]");

    ok("\xc4\xc4\xc4" !~ /(\x{100}+)/, "[perl #23769] don't match first byte of utf8 representation");
    ok("\xc4\xc4\xc4" !~ /(\x{100}+?)/, "[perl #23769] don't match first byte of utf8 representation");
}

for (120 .. 130) {
    my $head = 'x' x $_;
    for my $tail ('\x{0061}', '\x{1234}') {
	ok(
	    eval qq{ "$head$tail" =~ /$head$tail/ },
	    '\x{...} misparsed in regexp near 127 char EXACT limit'
	);
    }
}

# last test 1055

