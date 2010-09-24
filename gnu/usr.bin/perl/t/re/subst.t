#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
}

require './test.pl';
plan( tests => 143 );

$x = 'foo';
$_ = "x";
s/x/\$x/;
ok( $_ eq '$x', ":$_: eq :\$x:" );

$_ = "x";
s/x/$x/;
ok( $_ eq 'foo', ":$_: eq :foo:" );

$_ = "x";
s/x/\$x $x/;
ok( $_ eq '$x foo', ":$_: eq :\$x foo:" );

$b = 'cd';
($a = 'abcdef') =~ s<(b${b}e)>'\n$1';
ok( $1 eq 'bcde' && $a eq 'a\n$1f', ":$1: eq :bcde: ; :$a: eq :a\\n\$1f:" );

$a = 'abacada';
ok( ($a =~ s/a/x/g) == 4 && $a eq 'xbxcxdx' );

ok( ($a =~ s/a/y/g) == 0 && $a eq 'xbxcxdx' );

ok( ($a =~ s/b/y/g) == 1 && $a eq 'xyxcxdx' );

$_ = 'ABACADA';
ok( /a/i && s///gi && $_ eq 'BCD' );

$_ = '\\' x 4;
ok( length($_) == 4 );
$snum = s/\\/\\\\/g;
ok( $_ eq '\\' x 8 && $snum == 4 );

$_ = '\/' x 4;
ok( length($_) == 8 );
$snum = s/\//\/\//g;
ok( $_ eq '\\//' x 4 && $snum == 4 );
ok( length($_) == 12 );

$_ = 'aaaXXXXbbb';
s/^a//;
ok( $_ eq 'aaXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/a//;
ok( $_ eq 'aaXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/^a/b/;
ok( $_ eq 'baaXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/a/b/;
ok( $_ eq 'baaXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/aa//;
ok( $_ eq 'aXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/aa/b/;
ok( $_ eq 'baXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/b$//;
ok( $_ eq 'aaaXXXXbb' );

$_ = 'aaaXXXXbbb';
s/b//;
ok( $_ eq 'aaaXXXXbb' );

$_ = 'aaaXXXXbbb';
s/bb//;
ok( $_ eq 'aaaXXXXb' );

$_ = 'aaaXXXXbbb';
s/aX/y/;
ok( $_ eq 'aayXXXbbb' );

$_ = 'aaaXXXXbbb';
s/Xb/z/;
ok( $_ eq 'aaaXXXzbb' );

$_ = 'aaaXXXXbbb';
s/aaX.*Xbb//;
ok( $_ eq 'ab' );

$_ = 'aaaXXXXbbb';
s/bb/x/;
ok( $_ eq 'aaaXXXXxb' );

# now for some unoptimized versions of the same.

$_ = 'aaaXXXXbbb';
$x ne $x || s/^a//;
ok( $_ eq 'aaXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/a//;
ok( $_ eq 'aaXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/^a/b/;
ok( $_ eq 'baaXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/a/b/;
ok( $_ eq 'baaXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/aa//;
ok( $_ eq 'aXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/aa/b/;
ok( $_ eq 'baXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/b$//;
ok( $_ eq 'aaaXXXXbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/b//;
ok( $_ eq 'aaaXXXXbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/bb//;
ok( $_ eq 'aaaXXXXb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/aX/y/;
ok( $_ eq 'aayXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/Xb/z/;
ok( $_ eq 'aaaXXXzbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/aaX.*Xbb//;
ok( $_ eq 'ab' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/bb/x/;
ok( $_ eq 'aaaXXXXxb' );

$_ = 'abc123xyz';
s/(\d+)/$1*2/e;              # yields 'abc246xyz'
ok( $_ eq 'abc246xyz' );
s/(\d+)/sprintf("%5d",$1)/e; # yields 'abc  246xyz'
ok( $_ eq 'abc  246xyz' );
s/(\w)/$1 x 2/eg;            # yields 'aabbcc  224466xxyyzz'
ok( $_ eq 'aabbcc  224466xxyyzz' );

$_ = "aaaaa";
ok( y/a/b/ == 5 );
ok( y/a/b/ == 0 );
ok( y/b// == 5 );
ok( y/b/c/s == 5 );
ok( y/c// == 1 );
ok( y/c//d == 1 );
ok( $_ eq "" );

$_ = "Now is the %#*! time for all good men...";
ok( ($x=(y/a-zA-Z //cd)) == 7 );
ok( y/ / /s == 8 );

$_ = 'abcdefghijklmnopqrstuvwxyz0123456789';
tr/a-z/A-Z/;

ok( $_ eq 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789' );

# same as tr/A-Z/a-z/;
if (defined $Config{ebcdic} && $Config{ebcdic} eq 'define') {	# EBCDIC.
    no utf8;
    y[\301-\351][\201-\251];
} else {		# Ye Olde ASCII.  Or something like it.
    y[\101-\132][\141-\172];
}

ok( $_ eq 'abcdefghijklmnopqrstuvwxyz0123456789' );

SKIP: {
    skip("not ASCII",1) unless (ord("+") == ord(",") - 1
			     && ord(",") == ord("-") - 1
			     && ord("a") == ord("b") - 1
			     && ord("b") == ord("c") - 1);
    $_ = '+,-';
    tr/+--/a-c/;
    ok( $_ eq 'abc' );
}

$_ = '+,-';
tr/+\--/a\/c/;
ok( $_ eq 'a,/' );

$_ = '+,-';
tr/-+,/ab\-/;
ok( $_ eq 'b-a' );


# test recursive substitutions
# code based on the recursive expansion of makefile variables

my %MK = (
    AAAAA => '$(B)', B=>'$(C)', C => 'D',			# long->short
    E     => '$(F)', F=>'p $(G) q', G => 'HHHHH',	# short->long
    DIR => '$(UNDEFINEDNAME)/xxx',
);
sub var { 
    my($var,$level) = @_;
    return "\$($var)" unless exists $MK{$var};
    return exp_vars($MK{$var}, $level+1); # can recurse
}
sub exp_vars { 
    my($str,$level) = @_;
    $str =~ s/\$\((\w+)\)/var($1, $level+1)/ge; # can recurse
    #warn "exp_vars $level = '$str'\n";
    $str;
}

ok( exp_vars('$(AAAAA)',0)           eq 'D' );
ok( exp_vars('$(E)',0)               eq 'p HHHHH q' );
ok( exp_vars('$(DIR)',0)             eq '$(UNDEFINEDNAME)/xxx' );
ok( exp_vars('foo $(DIR)/yyy bar',0) eq 'foo $(UNDEFINEDNAME)/xxx/yyy bar' );

$_ = "abcd";
s/(..)/$x = $1, m#.#/eg;
ok( $x eq "cd", 'a match nested in the RHS of a substitution' );

# Subst and lookbehind

$_="ccccc";
$snum = s/(?<!x)c/x/g;
ok( $_ eq "xxxxx" && $snum == 5 );

$_="ccccc";
$snum = s/(?<!x)(c)/x/g;
ok( $_ eq "xxxxx" && $snum == 5 );

$_="foobbarfoobbar";
$snum = s/(?<!r)foobbar/foobar/g;
ok( $_ eq "foobarfoobbar" && $snum == 1 );

$_="foobbarfoobbar";
$snum = s/(?<!ar)(foobbar)/foobar/g;
ok( $_ eq "foobarfoobbar" && $snum == 1 );

$_="foobbarfoobbar";
$snum = s/(?<!ar)foobbar/foobar/g;
ok( $_ eq "foobarfoobbar" && $snum == 1 );

eval 's{foo} # this is a comment, not a delimiter
       {bar};';
ok( ! @?, 'parsing of split subst with comment' );

$snum = eval '$_="exactly"; s sxsys;m 3(yactl)3;$1';
is( $snum, 'yactl', 'alpha delimiters are allowed' );

$_="baacbaa";
$snum = tr/a/b/s;
ok( $_ eq "bbcbb" && $snum == 4,
    'check if squashing works at the end of string' );

$_ = "ab";
ok( s/a/b/ == 1 );

$_ = <<'EOL';
     $url = new URI::URL "http://www/";   die if $url eq "xXx";
EOL
$^R = 'junk';

$foo = ' $@%#lowercase $@%# lowercase UPPERCASE$@%#UPPERCASE' .
  ' $@%#lowercase$@%#lowercase$@%# lowercase lowercase $@%#lowercase' .
  ' lowercase $@%#MiXeD$@%# ';

$snum =
s{  \d+          \b [,.;]? (?{ 'digits' })
   |
    [a-z]+       \b [,.;]? (?{ 'lowercase' })
   |
    [A-Z]+       \b [,.;]? (?{ 'UPPERCASE' })
   |
    [A-Z] [a-z]+ \b [,.;]? (?{ 'Capitalized' })
   |
    [A-Za-z]+    \b [,.;]? (?{ 'MiXeD' })
   |
    [A-Za-z0-9]+ \b [,.;]? (?{ 'alphanumeric' })
   |
    \s+                    (?{ ' ' })
   |
    [^A-Za-z0-9\s]+          (?{ '$@%#' })
}{$^R}xg;
ok( $_ eq $foo );
ok( $snum == 31 );

$_ = 'a' x 6;
$snum = s/a(?{})//g;
ok( $_ eq '' && $snum == 6 );

$_ = 'x' x 20; 
$snum = s/(\d*|x)/<$1>/g; 
$foo = '<>' . ('<x><>' x 20) ;
ok( $_ eq $foo && $snum == 41 );

$t = 'aaaaaaaaa'; 

$_ = $t;
pos = 6;
$snum = s/\Ga/xx/g;
ok( $_ eq 'aaaaaaxxxxxx' && $snum == 3 );

$_ = $t;
pos = 6;
$snum = s/\Ga/x/g;
ok( $_ eq 'aaaaaaxxx' && $snum == 3 );

$_ = $t;
pos = 6;
s/\Ga/xx/;
ok( $_ eq 'aaaaaaxxaa' );

$_ = $t;
pos = 6;
s/\Ga/x/;
ok( $_ eq 'aaaaaaxaa' );

$_ = $t;
$snum = s/\Ga/xx/g;
ok( $_ eq 'xxxxxxxxxxxxxxxxxx' && $snum == 9 );

$_ = $t;
$snum = s/\Ga/x/g;
ok( $_ eq 'xxxxxxxxx' && $snum == 9 );

$_ = $t;
s/\Ga/xx/;
ok( $_ eq 'xxaaaaaaaa' );

$_ = $t;
s/\Ga/x/;
ok( $_ eq 'xaaaaaaaa' );

$_ = 'aaaa';
$snum = s/\ba/./g;
ok( $_ eq '.aaa' && $snum == 1 );

eval q% s/a/"b"}/e %;
ok( $@ =~ /Bad evalled substitution/ );
eval q% ($_ = "x") =~ s/(.)/"$1 "/e %;
ok( $_ eq "x " and !length $@ );
$x = $x = 'interp';
eval q% ($_ = "x") =~ s/x(($x)*)/"$1"/e %;
ok( $_ eq '' and !length $@ );

$_ = "C:/";
ok( !s/^([a-z]:)/\u$1/ );

$_ = "Charles Bronson";
$snum = s/\B\w//g;
ok( $_ eq "C B" && $snum == 12 );

{
    use utf8;
    my $s = "H\303\266he";
    my $l = my $r = $s;
    $l =~ s/[^\w]//g;
    $r =~ s/[^\w\.]//g;
    is($l, $r, "use utf8 \\w");
}

my $pv1 = my $pv2  = "Andreas J. K\303\266nig";
$pv1 =~ s/A/\x{100}/;
substr($pv2,0,1) = "\x{100}";
is($pv1, $pv2);

SKIP: {
    skip("EBCDIC", 3) if ord("A") == 193; 

    {   
	# Gregor Chrupala <gregor.chrupala@star-group.net>
	use utf8;
	$a = 'Espa&ntilde;a';
	$a =~ s/&ntilde;/ñ/;
	like($a, qr/ñ/, "use utf8 RHS");
    }

    {
	use utf8;
	$a = 'España España';
	$a =~ s/ñ/&ntilde;/;
	like($a, qr/ñ/, "use utf8 LHS");
    }

    {
	use utf8;
	$a = 'España';
	$a =~ s/ñ/ñ/;
	like($a, qr/ñ/, "use utf8 LHS and RHS");
    }
}

{
    # SADAHIRO Tomoyuki <bqw10602@nifty.com>

    $a = "\x{100}\x{101}";
    $a =~ s/\x{101}/\xFF/;
    like($a, qr/\xFF/);
    is(length($a), 2, "SADAHIRO utf8 s///");

    $a = "\x{100}\x{101}";
    $a =~ s/\x{101}/"\xFF"/e;
    like($a, qr/\xFF/);
    is(length($a), 2);

    $a = "\x{100}\x{101}";
    $a =~ s/\x{101}/\xFF\xFF\xFF/;
    like($a, qr/\xFF\xFF\xFF/);
    is(length($a), 4);

    $a = "\x{100}\x{101}";
    $a =~ s/\x{101}/"\xFF\xFF\xFF"/e;
    like($a, qr/\xFF\xFF\xFF/);
    is(length($a), 4);

    $a = "\xFF\x{101}";
    $a =~ s/\xFF/\x{100}/;
    like($a, qr/\x{100}/);
    is(length($a), 2);

    $a = "\xFF\x{101}";
    $a =~ s/\xFF/"\x{100}"/e;
    like($a, qr/\x{100}/);
    is(length($a), 2);

    $a = "\xFF";
    $a =~ s/\xFF/\x{100}/;
    like($a, qr/\x{100}/);
    is(length($a), 1);

    $a = "\xFF";
    $a =~ s/\xFF/"\x{100}"/e;
    like($a, qr/\x{100}/);
    is(length($a), 1);
}

{
    # subst with mixed utf8/non-utf8 type
    my($ua, $ub, $uc, $ud) = ("\x{101}", "\x{102}", "\x{103}", "\x{104}");
    my($na, $nb) = ("\x{ff}", "\x{fe}");
    my $a = "$ua--$ub";
    my $b;
    ($b = $a) =~ s/--/$na/;
    is($b, "$ua$na$ub", "s///: replace non-utf8 into utf8");
    ($b = $a) =~ s/--/--$na--/;
    is($b, "$ua--$na--$ub", "s///: replace long non-utf8 into utf8");
    ($b = $a) =~ s/--/$uc/;
    is($b, "$ua$uc$ub", "s///: replace utf8 into utf8");
    ($b = $a) =~ s/--/--$uc--/;
    is($b, "$ua--$uc--$ub", "s///: replace long utf8 into utf8");
    $a = "$na--$nb";
    ($b = $a) =~ s/--/$ua/;
    is($b, "$na$ua$nb", "s///: replace utf8 into non-utf8");
    ($b = $a) =~ s/--/--$ua--/;
    is($b, "$na--$ua--$nb", "s///: replace long utf8 into non-utf8");

    # now with utf8 pattern
    $a = "$ua--$ub";
    ($b = $a) =~ s/-($ud)?-/$na/;
    is($b, "$ua$na$ub", "s///: replace non-utf8 into utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/--$na--/;
    is($b, "$ua--$na--$ub", "s///: replace long non-utf8 into utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/$uc/;
    is($b, "$ua$uc$ub", "s///: replace utf8 into utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/--$uc--/;
    is($b, "$ua--$uc--$ub", "s///: replace long utf8 into utf8 (utf8 pattern)");
    $a = "$na--$nb";
    ($b = $a) =~ s/-($ud)?-/$ua/;
    is($b, "$na$ua$nb", "s///: replace utf8 into non-utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/--$ua--/;
    is($b, "$na--$ua--$nb", "s///: replace long utf8 into non-utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/$na/;
    is($b, "$na$na$nb", "s///: replace non-utf8 into non-utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/--$na--/;
    is($b, "$na--$na--$nb", "s///: replace long non-utf8 into non-utf8 (utf8 pattern)");
}

$_ = 'aaaa';
$r = 'x';
$s = s/a(?{})/$r/g;
is("<$_> <$s>", "<xxxx> <4>", "[perl #7806]");

$_ = 'aaaa';
$s = s/a(?{})//g;
is("<$_> <$s>", "<> <4>", "[perl #7806]");

# [perl #19048] Coredump in silly replacement
{
    local $^W = 0;
    $_="abcdef\n";
    s!.!!eg;
    is($_, "\n", "[perl #19048]");
}

# [perl #17757] interaction between saw_ampersand and study
{
    my $f = eval q{ $& };
    $f = "xx";
    study $f;
    $f =~ s/x/y/g;
    is($f, "yy", "[perl #17757]");
}

# [perl #20684] returned a zero count
$_ = "1111";
is(s/(??{1})/2/eg, 4, '#20684 s/// with (??{..}) inside');

# [perl #20682] @- not visible in replacement
$_ = "123";
/(2)/;	# seed @- with something else
s/(1)(2)(3)/$#- (@-)/;
is($_, "3 (0 0 1 2)", '#20682 @- not visible in replacement');

# [perl #20682] $^N not visible in replacement
$_ = "abc";
/(a)/; s/(b)|(c)/-$^N/g;
is($_,'a-b-c','#20682 $^N not visible in replacement');

# [perl #22351] perl bug with 'e' substitution modifier
my $name = "chris";
{
    no warnings 'uninitialized';
    $name =~ s/hr//e;
}
is($name, "cis", q[#22351 bug with 'e' substitution modifier]);


# [perl #34171] $1 didn't honour 'use bytes' in s//e
{
    my $s="\x{100}";
    my $x;
    {
	use bytes;
	$s=~ s/(..)/$x=$1/e
    }
    is(length($x), 2, '[perl #34171]');
}


{ # [perl #27940] perlbug: [\x00-\x1f] works, [\c@-\c_] does not
    my $c;

    ($c = "\x20\c@\x30\cA\x40\cZ\x50\c_\x60") =~ s/[\c@-\c_]//g;
    is($c, "\x20\x30\x40\x50\x60", "s/[\\c\@-\\c_]//g");

    ($c = "\x20\x00\x30\x01\x40\x1A\x50\x1F\x60") =~ s/[\x00-\x1f]//g;
    is($c, "\x20\x30\x40\x50\x60", "s/[\\x00-\\x1f]//g");
}
{
    $_ = "xy";
    no warnings 'uninitialized';
    /(((((((((x)))))))))(z)/;	# clear $10
    s/(((((((((x)))))))))(y)/${10}/;
    is($_,"y","RT#6006: \$_ eq '$_'");
    $_ = "xr";
    s/(((((((((x)))))))))(r)/fooba${10}/;
    is($_,"foobar","RT#6006: \$_ eq '$_'");
}
{
    my $want=("\n" x 11).("B\n" x 11)."B";
    $_="B";
    our $i;
    for $i(1..11){
	s/^.*$/$&/gm;
	$_="\n$_\n$&";
    }
    is($want,$_,"RT#17542");
}

{
    my @tests = ('ABC', "\xA3\xA4\xA5", "\x{410}\x{411}\x{412}");
    foreach (@tests) {
	my $id = ord $_;
	s/./pos/ge;
	is($_, "012", "RT#52104: $id");
    }
}

fresh_perl_is( '$_=q(foo);s/(.)\G//g;print' => 'foo', '[perl #69056] positive GPOS regex segfault' );
fresh_perl_is( '$_="abcef"; s/bc|(.)\G(.)/$1 ? "[$1-$2]" : "XX"/ge; print' => 'aXX[c-e][e-f]f', 'positive GPOS regex substitution failure' );

# [perl #~~~~~] $var =~ s/$qr//e calling get-magic on $_ as well as $var
{
 local *_;
 my $scratch;
 sub qrBug::TIESCALAR { bless[pop], 'qrBug' }
 sub qrBug::FETCH { $scratch .= "[fetching $_[0][0]]"; 'prew' }
 sub qrBug::STORE{}
 tie my $kror, qrBug => '$kror';
 tie $_, qrBug => '$_';
 my $qr = qr/(?:)/;
 $kror =~ s/$qr/""/e;
 is(
   $scratch, '[fetching $kror]',
  'bug: $var =~ s/$qr//e calling get-magic on $_ as well as $var',
 );
}
