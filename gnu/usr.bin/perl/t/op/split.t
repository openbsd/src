#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..46\n";

$FS = ':';

$_ = 'a:b:c';

($a,$b,$c) = split($FS,$_);

if (join(';',$a,$b,$c) eq 'a;b;c') {print "ok 1\n";} else {print "not ok 1\n";}

@ary = split(/:b:/);
if (join("$_",@ary) eq 'aa:b:cc') {print "ok 2\n";} else {print "not ok 2\n";}

$_ = "abc\n";
my @xyz = (@ary = split(//));
if (join(".",@ary) eq "a.b.c.\n") {print "ok 3\n";} else {print "not ok 3\n";}

$_ = "a:b:c::::";
@ary = split(/:/);
if (join(".",@ary) eq "a.b.c") {print "ok 4\n";} else {print "not ok 4\n";}

$_ = join(':',split(' ',"    a b\tc \t d "));
if ($_ eq 'a:b:c:d') {print "ok 5\n";} else {print "not ok 5 #$_#\n";}

$_ = join(':',split(/ */,"foo  bar bie\tdoll"));
if ($_ eq "f:o:o:b:a:r:b:i:e:\t:d:o:l:l")
	{print "ok 6\n";} else {print "not ok 6\n";}

$_ = join(':', 'foo', split(/ /,'a b  c'), 'bar');
if ($_ eq "foo:a:b::c:bar") {print "ok 7\n";} else {print "not ok 7 $_\n";}

# Can we say how many fields to split to?
$_ = join(':', split(' ','1 2 3 4 5 6', 3));
print $_ eq '1:2:3 4 5 6' ? "ok 8\n" : "not ok 8 $_\n";

# Can we do it as a variable?
$x = 4;
$_ = join(':', split(' ','1 2 3 4 5 6', $x));
print $_ eq '1:2:3:4 5 6' ? "ok 9\n" : "not ok 9 $_\n";

# Does the 999 suppress null field chopping?
$_ = join(':', split(/:/,'1:2:3:4:5:6:::', 999));
print $_ eq '1:2:3:4:5:6:::' ? "ok 10\n" : "not ok 10 $_\n";

# Does assignment to a list imply split to one more field than that?
if ($^O eq 'MSWin32') { $foo = `.\\perl -D1024 -e "(\$a,\$b) = split;" 2>&1` }
elsif ($^O eq 'NetWare') { $foo = `perl -D1024 -e "(\$a,\$b) = split;" 2>&1` }
elsif ($^O eq 'VMS')  { $foo = `./perl "-D1024" -e "(\$a,\$b) = split;" 2>&1` }
elsif ($^O eq 'MacOS'){ $foo = `$^X "-D1024" -e "(\$a,\$b) = split;"` }
else                  { $foo = `./perl -D1024 -e '(\$a,\$b) = split;' 2>&1` }
print $foo =~ /DEBUGGING/ || $foo =~ /SV = (VOID|IV\(3\))/ ? "ok 11\n" : "not ok 11\n";

# Can we say how many fields to split to when assigning to a list?
($a,$b) = split(' ','1 2 3 4 5 6', 2);
$_ = join(':',$a,$b);
print $_ eq '1:2 3 4 5 6' ? "ok 12\n" : "not ok 12 $_\n";

# do subpatterns generate additional fields (without trailing nulls)?
$_ = join '|', split(/,|(-)/, "1-10,20,,,");
print $_ eq "1|-|10||20" ? "ok 13\n" : "not ok 13\n";

# do subpatterns generate additional fields (with a limit)?
$_ = join '|', split(/,|(-)/, "1-10,20,,,", 10);
print $_ eq "1|-|10||20||||||" ? "ok 14\n" : "not ok 14\n";

# is the 'two undefs' bug fixed?
(undef, $a, undef, $b) = qw(1 2 3 4);
print "$a|$b" eq "2|4" ? "ok 15\n" : "not ok 15\n";

# .. even for locals?
{
  local(undef, $a, undef, $b) = qw(1 2 3 4);
  print "$a|$b" eq "2|4" ? "ok 16\n" : "not ok 16\n";
}

# check splitting of null string
$_ = join('|', split(/x/,   '',-1), 'Z');
print $_ eq "Z" ? "ok 17\n" : "#$_\nnot ok 17\n";

$_ = join('|', split(/x/,   '', 1), 'Z');
print $_ eq "Z" ? "ok 18\n" : "#$_\nnot ok 18\n";

$_ = join('|', split(/(p+)/,'',-1), 'Z');
print $_ eq "Z" ? "ok 19\n" : "#$_\nnot ok 19\n";

$_ = join('|', split(/.?/,  '',-1), 'Z');
print $_ eq "Z" ? "ok 20\n" : "#$_\nnot ok 20\n";


# Are /^/m patterns scanned?
$_ = join '|', split(/^a/m, "a b a\na d a", 20);
print $_ eq "| b a\n| d a" ? "ok 21\n" : "not ok 21\n# `$_'\n";

# Are /$/m patterns scanned?
$_ = join '|', split(/a$/m, "a b a\na d a", 20);
print $_ eq "a b |\na d |" ? "ok 22\n" : "not ok 22\n# `$_'\n";

# Are /^/m patterns scanned?
$_ = join '|', split(/^aa/m, "aa b aa\naa d aa", 20);
print $_ eq "| b aa\n| d aa" ? "ok 23\n" : "not ok 23\n# `$_'\n";

# Are /$/m patterns scanned?
$_ = join '|', split(/aa$/m, "aa b aa\naa d aa", 20);
print $_ eq "aa b |\naa d |" ? "ok 24\n" : "not ok 24\n# `$_'\n";

# Greedyness:
$_ = "a : b :c: d";
@ary = split(/\s*:\s*/);
if (($res = join(".",@ary)) eq "a.b.c.d") {print "ok 25\n";} else {print "not ok 25\n# res=`$res' != `a.b.c.d'\n";}

# use of match result as pattern (!)
'p:q:r:s' eq join ':', split('abc' =~ /b/, 'p1q1r1s') or print "not ";
print "ok 26\n";

# /^/ treated as /^/m
$_ = join ':', split /^/, "ab\ncd\nef\n";
print "not " if $_ ne "ab\n:cd\n:ef\n";
print "ok 27\n";

# see if @a = @b = split(...) optimization works
@list1 = @list2 = split ('p',"a p b c p");
print "not " if @list1 != @list2 or "@list1" ne "@list2"
             or @list1 != 2 or "@list1" ne "a   b c ";
print "ok 28\n";

# zero-width assertion
$_ = join ':', split /(?=\w)/, "rm b";
print "not" if $_ ne "r:m :b";
print "ok 29\n";

# unicode splittage

@ary = map {ord} split //, v1.20.300.4000.50000.4000.300.20.1;
print "not " unless "@ary" eq "1 20 300 4000 50000 4000 300 20 1";
print "ok 30\n";

@ary = split(/\x{FE}/, "\x{FF}\x{FE}\x{FD}"); # bug id 20010105.016
print "not " unless @ary == 2 &&
                    $ary[0] eq "\xFF"   && $ary[1] eq "\xFD" &&
                    $ary[0] eq "\x{FF}" && $ary[1] eq "\x{FD}";
print "ok 31\n";

@ary = split(/(\x{FE}\xFE)/, "\xFF\x{FF}\xFE\x{FE}\xFD\x{FD}"); # variant of 31
print "not " unless @ary == 3 &&
                    $ary[0] eq "\xFF\xFF"     &&
                    $ary[0] eq "\x{FF}\xFF"   &&
                    $ary[0] eq "\x{FF}\x{FF}" &&
                    $ary[1] eq "\xFE\xFE"     &&
                    $ary[1] eq "\x{FE}\xFE"   &&
                    $ary[1] eq "\x{FE}\x{FE}" &&
                    $ary[2] eq "\xFD\xFD"     &&
                    $ary[2] eq "\x{FD}\xFD"   &&
                    $ary[2] eq "\x{FD}\x{FD}";
print "ok 32\n";


{
    my @a = map ord, split(//, join("", map chr, (1234, 123, 2345)));
    print "not " unless "@a" eq "1234 123 2345";
    print "ok 33\n";
}

{
    my $x = 'A';
    my @a = map ord, split(/$x/, join("", map chr, (1234, ord($x), 2345)));
    print "not " unless "@a" eq "1234 2345";
    print "ok 34\n";
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

    print "not " unless $r eq " U+B36C U+5A8C U+FF5B U+5079 U+505B";
    print "ok 35\n";
}

{
    my $s = "\x20\x40\x{80}\x{100}\x{80}\x40\x20";

    if (ord('A') == 193) {
	print "ok 36 # Skip: EBCDIC\n";
    } else {
	# bug id 20000426.003


	my ($a, $b, $c) = split(/\x40/, $s);
	print "not "
	    unless $a eq "\x20" && $b eq "\x{80}\x{100}\x{80}" && $c eq $a;
	print "ok 36\n";
    }

    my ($a, $b) = split(/\x{100}/, $s);
    print "not " unless $a eq "\x20\x40\x{80}" && $b eq "\x{80}\x40\x20";
    print "ok 37\n";

    my ($a, $b) = split(/\x{80}\x{100}\x{80}/, $s);
    print "not " unless $a eq "\x20\x40" && $b eq "\x40\x20";
    print "ok 38\n";

    if (ord('A') == 193) {
	print "ok 39 # Skip: EBCDIC\n";
    }  else {
	my ($a, $b) = split(/\x40\x{80}/, $s);
	print "not " unless $a eq "\x20" && $b eq "\x{100}\x{80}\x40\x20";
	print "ok 39\n";
    }

    my ($a, $b, $c) = split(/[\x40\x{80}]+/, $s);
    print "not " unless $a eq "\x20" && $b eq "\x{100}" && $c eq "\x20";
    print "ok 40\n";
}

{
    # 20001205.014

    my $a = "ABC\x{263A}";

    my @b = split( //, $a );

    print "not " unless @b == 4;
    print "ok 41\n";

    print "not " unless length($b[3]) == 1 && $b[3] eq "\x{263A}";
    print "ok 42\n";

    $a =~ s/^A/Z/;
    print "not " unless length($a) == 4 && $a eq "ZBC\x{263A}";
    print "ok 43\n";
}

{
    my @a = split(/\xFE/, "\xFF\xFE\xFD");

    print "not " unless @a == 2 && $a[0] eq "\xFF" && $a[1] eq "\xFD";
    print "ok 44\n";
}

{
    # check that PMf_WHITE is cleared after \s+ is used
    # reported in <20010627113312.RWGY6087.viemta06@localhost>
    my $r;
    foreach my $pat ( qr/\s+/, qr/ll/ ) {
	$r = join ':' => split($pat, "hello cruel world");
    }
    print "not " unless $r eq "he:o cruel world";
    print "ok 45\n";
}


{
    # split /(A)|B/, "1B2" should return (1, undef, 2)
    my @x = split /(A)|B/, "1B2";
    print "not " unless
      $x[0] eq '1' and
      (not defined $x[1]) and
      $x[2] eq '2';
    print "ok 46\n";
}
