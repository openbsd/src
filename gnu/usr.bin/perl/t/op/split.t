#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 55;

$FS = ':';

$_ = 'a:b:c';

($a,$b,$c) = split($FS,$_);

is(join(';',$a,$b,$c), 'a;b;c');

@ary = split(/:b:/);
is(join("$_",@ary), 'aa:b:cc');

$_ = "abc\n";
my @xyz = (@ary = split(//));
is(join(".",@ary), "a.b.c.\n");

$_ = "a:b:c::::";
@ary = split(/:/);
is(join(".",@ary), "a.b.c");

$_ = join(':',split(' ',"    a b\tc \t d "));
is($_, 'a:b:c:d');

$_ = join(':',split(/ */,"foo  bar bie\tdoll"));
is($_ , "f:o:o:b:a:r:b:i:e:\t:d:o:l:l");

$_ = join(':', 'foo', split(/ /,'a b  c'), 'bar');
is($_, "foo:a:b::c:bar");

# Can we say how many fields to split to?
$_ = join(':', split(' ','1 2 3 4 5 6', 3));
is($_, '1:2:3 4 5 6');

# Can we do it as a variable?
$x = 4;
$_ = join(':', split(' ','1 2 3 4 5 6', $x));
is($_, '1:2:3:4 5 6');

# Does the 999 suppress null field chopping?
$_ = join(':', split(/:/,'1:2:3:4:5:6:::', 999));
is($_ , '1:2:3:4:5:6:::');

# Does assignment to a list imply split to one more field than that?
if ($^O eq 'MSWin32') { $foo = `.\\perl -Dt -e "(\$a,\$b) = split;" 2>&1` }
elsif ($^O eq 'NetWare') { $foo = `perl -Dt -e "(\$a,\$b) = split;" 2>&1` }
elsif ($^O eq 'VMS')  { $foo = `./perl "-Dt" -e "(\$a,\$b) = split;" 2>&1` }
elsif ($^O eq 'MacOS'){ $foo = `$^X "-Dt" -e "(\$a,\$b) = split;"` }
else                  { $foo = `./perl -Dt -e '(\$a,\$b) = split;' 2>&1` }
ok($foo =~ /DEBUGGING/ || $foo =~ /\Qconst(IV(3))\E/);

# Can we say how many fields to split to when assigning to a list?
($a,$b) = split(' ','1 2 3 4 5 6', 2);
$_ = join(':',$a,$b);
is($_, '1:2 3 4 5 6');

# do subpatterns generate additional fields (without trailing nulls)?
$_ = join '|', split(/,|(-)/, "1-10,20,,,");
is($_, "1|-|10||20");

# do subpatterns generate additional fields (with a limit)?
$_ = join '|', split(/,|(-)/, "1-10,20,,,", 10);
is($_, "1|-|10||20||||||");

# is the 'two undefs' bug fixed?
(undef, $a, undef, $b) = qw(1 2 3 4);
is("$a|$b", "2|4");

# .. even for locals?
{
  local(undef, $a, undef, $b) = qw(1 2 3 4);
  is("$a|$b", "2|4");
}

# check splitting of null string
$_ = join('|', split(/x/,   '',-1), 'Z');
is($_, "Z");

$_ = join('|', split(/x/,   '', 1), 'Z');
is($_, "Z");

$_ = join('|', split(/(p+)/,'',-1), 'Z');
is($_, "Z");

$_ = join('|', split(/.?/,  '',-1), 'Z');
is($_, "Z");


# Are /^/m patterns scanned?
$_ = join '|', split(/^a/m, "a b a\na d a", 20);
is($_, "| b a\n| d a");

# Are /$/m patterns scanned?
$_ = join '|', split(/a$/m, "a b a\na d a", 20);
is($_, "a b |\na d |");

# Are /^/m patterns scanned?
$_ = join '|', split(/^aa/m, "aa b aa\naa d aa", 20);
is($_, "| b aa\n| d aa");

# Are /$/m patterns scanned?
$_ = join '|', split(/aa$/m, "aa b aa\naa d aa", 20);
is($_, "aa b |\naa d |");

# Greedyness:
$_ = "a : b :c: d";
@ary = split(/\s*:\s*/);
is(($res = join(".",@ary)), "a.b.c.d", $res);

# use of match result as pattern (!)
is('p:q:r:s', join ':', split('abc' =~ /b/, 'p1q1r1s'));

# /^/ treated as /^/m
$_ = join ':', split /^/, "ab\ncd\nef\n";
is($_, "ab\n:cd\n:ef\n");

# see if @a = @b = split(...) optimization works
@list1 = @list2 = split ('p',"a p b c p");
ok(@list1 == @list2 &&
   "@list1" eq "@list2" &&
   @list1 == 2 &&
   "@list1" eq "a   b c ");

# zero-width assertion
$_ = join ':', split /(?=\w)/, "rm b";
is($_, "r:m :b");

# unicode splittage

@ary = map {ord} split //, v1.20.300.4000.50000.4000.300.20.1;
is("@ary", "1 20 300 4000 50000 4000 300 20 1");

@ary = split(/\x{FE}/, "\x{FF}\x{FE}\x{FD}"); # bug id 20010105.016
ok(@ary == 2 &&
   $ary[0] eq "\xFF"   && $ary[1] eq "\xFD" &&
   $ary[0] eq "\x{FF}" && $ary[1] eq "\x{FD}");

@ary = split(/(\x{FE}\xFE)/, "\xFF\x{FF}\xFE\x{FE}\xFD\x{FD}"); # variant of 31
ok(@ary == 3 &&
   $ary[0] eq "\xFF\xFF"     &&
   $ary[0] eq "\x{FF}\xFF"   &&
   $ary[0] eq "\x{FF}\x{FF}" &&
   $ary[1] eq "\xFE\xFE"     &&
   $ary[1] eq "\x{FE}\xFE"   &&
   $ary[1] eq "\x{FE}\x{FE}" &&
   $ary[2] eq "\xFD\xFD"     &&
   $ary[2] eq "\x{FD}\xFD"   &&
   $ary[2] eq "\x{FD}\x{FD}");

{
    my @a = map ord, split(//, join("", map chr, (1234, 123, 2345)));
    is("@a", "1234 123 2345");
}

{
    my $x = 'A';
    my @a = map ord, split(/$x/, join("", map chr, (1234, ord($x), 2345)));
    is("@a", "1234 2345");
}

{
    # bug id 20000427.003 

    use warnings;
    use strict;

    my $sushi = "\x{b36c}\x{5a8c}\x{ff5b}\x{5079}\x{505b}";

    my @charlist = split //, $sushi;
    my $r = '';
    foreach my $ch (@charlist) {
	$r = $r . " " . sprintf "U+%04X", ord($ch);
    }

    is($r, " U+B36C U+5A8C U+FF5B U+5079 U+505B");
}

{
    my $s = "\x20\x40\x{80}\x{100}\x{80}\x40\x20";

  SKIP: {
    if (ord('A') == 193) {
	skip("EBCDIC", 1);
    } else {
	# bug id 20000426.003

	my ($a, $b, $c) = split(/\x40/, $s);
	ok($a eq "\x20" && $b eq "\x{80}\x{100}\x{80}" && $c eq $a);
    }
  }

    my ($a, $b) = split(/\x{100}/, $s);
    ok($a eq "\x20\x40\x{80}" && $b eq "\x{80}\x40\x20");

    my ($a, $b) = split(/\x{80}\x{100}\x{80}/, $s);
    ok($a eq "\x20\x40" && $b eq "\x40\x20");

  SKIP: {
    if (ord('A') == 193) {
	skip("EBCDIC", 1);
    }  else {
	my ($a, $b) = split(/\x40\x{80}/, $s);
	ok($a eq "\x20" && $b eq "\x{100}\x{80}\x40\x20");
    }
  }

    my ($a, $b, $c) = split(/[\x40\x{80}]+/, $s);
    ok($a eq "\x20" && $b eq "\x{100}" && $c eq "\x20");
}

{
    # 20001205.014

    my $a = "ABC\x{263A}";

    my @b = split( //, $a );

    is(scalar @b, 4);

    ok(length($b[3]) == 1 && $b[3] eq "\x{263A}");

    $a =~ s/^A/Z/;
    ok(length($a) == 4 && $a eq "ZBC\x{263A}");
}

{
    my @a = split(/\xFE/, "\xFF\xFE\xFD");

    ok(@a == 2 && $a[0] eq "\xFF" && $a[1] eq "\xFD");
}

{
    # check that PMf_WHITE is cleared after \s+ is used
    # reported in <20010627113312.RWGY6087.viemta06@localhost>
    my $r;
    foreach my $pat ( qr/\s+/, qr/ll/ ) {
	$r = join ':' => split($pat, "hello cruel world");
    }
    is($r, "he:o cruel world");
}


{
    # split /(A)|B/, "1B2" should return (1, undef, 2)
    my @x = split /(A)|B/, "1B2";
    ok($x[0] eq '1' and (not defined $x[1]) and $x[2] eq '2');
}

{
    # [perl #17064]
    my $warn;
    local $SIG{__WARN__} = sub { $warn = join '', @_; chomp $warn };
    my $char = "\x{10f1ff}";
    my @a = split /\r?\n/, "$char\n";
    ok(@a == 1 && $a[0] eq $char && !defined($warn));
}

{
    # [perl #18195]
    for my $u (0, 1) {
	for my $a (0, 1) {
	    $_ = 'readin,database,readout';
	    utf8::upgrade $_ if $u;
	    /(.+)/;
	    my @d = split /[,]/,$1;
	    is(join (':',@d), 'readin:database:readout', "[perl #18195]");
	}
    }
}

{
    $p="a,b";
    utf8::upgrade $p;
    eval { @a=split(/[, ]+/,$p) };
    is ("$@-@a-", '-a b-', '#20912 - split() to array with /[]+/ and utf8');
}

{
    is (\@a, \@{"a"}, '@a must be global for following test');
    $p="";
    $n = @a = split /,/,$p;
    is ($n, 0, '#21765 - pmreplroot hack used to return undef for 0 iters');
}

{
    # [perl #28938]
    # assigning off the end of the array after a split could leave garbage
    # in the inner elements

    my $x;
    @a = split /,/, ',,,,,';
    $a[3]=1;
    $x = \$a[2];
    is (ref $x, 'SCALAR', '#28938 - garbage after extend');
}

