#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    unless (find PerlIO::Layer 'perlio') {
	print "1..0 # Skip: not perlio\n";
	exit 0;
    }
}

no utf8; # needed for use utf8 not griping about the raw octets

$| = 1;
print "1..31\n";

open(F,"+>:utf8",'a');
print F chr(0x100).'£';
print '#'.tell(F)."\n";
print "not " unless tell(F) == 4;
print "ok 1\n";
print F "\n";
print '#'.tell(F)."\n";
print "not " unless tell(F) >= 5;
print "ok 2\n";
seek(F,0,0);
print "not " unless getc(F) eq chr(0x100);
print "ok 3\n";
print "not " unless getc(F) eq "£";
print "ok 4\n";
print "not " unless getc(F) eq "\n";
print "ok 5\n";
seek(F,0,0);
binmode(F,":bytes");
my $chr = chr(0xc4);
if (ord('A') == 193) { $chr = chr(0x8c); } # EBCDIC
print "not " unless getc(F) eq $chr;
print "ok 6\n";
$chr = chr(0x80);
if (ord('A') == 193) { $chr = chr(0x41); } # EBCDIC
print "not " unless getc(F) eq $chr;
print "ok 7\n";
$chr = chr(0xc2);
if (ord('A') == 193) { $chr = chr(0x80); } # EBCDIC
print "not " unless getc(F) eq $chr;
print "ok 8\n";
$chr = chr(0xa3);
if (ord('A') == 193) { $chr = chr(0x44); } # EBCDIC
print "not " unless getc(F) eq $chr;
print "ok 9\n";
print "not " unless getc(F) eq "\n";
print "ok 10\n";
seek(F,0,0);
binmode(F,":utf8");
print "not " unless scalar(<F>) eq "\x{100}£\n";
print "ok 11\n";
seek(F,0,0);
$buf = chr(0x200);
$count = read(F,$buf,2,1);
print "not " unless $count == 2;
print "ok 12\n";
print "not " unless $buf eq "\x{200}\x{100}£";
print "ok 13\n";
close(F);

{
    $a = chr(300); # This *is* UTF-encoded
    $b = chr(130); # This is not.

    open F, ">:utf8", 'a' or die $!;
    print F $a,"\n";
    close F;

    open F, "<:utf8", 'a' or die $!;
    $x = <F>;
    chomp($x);
    print "not " unless $x eq chr(300);
    print "ok 14\n";

    open F, "a" or die $!; # Not UTF
    binmode(F, ":bytes");
    $x = <F>;
    chomp($x);
    $chr = chr(196).chr(172);
    if (ord('A') == 193) { $chr = chr(141).chr(83); } # EBCDIC
    print "not " unless $x eq $chr;
    print "ok 15\n";
    close F;

    open F, ">:utf8", 'a' or die $!;
    binmode(F);  # we write a "\n" and then tell() - avoid CRLF issues.
    binmode(F,":utf8"); # turn UTF-8-ness back on
    print F $a;
    my $y;
    { my $x = tell(F);
      { use bytes; $y = length($a);}
      print "not " unless $x == $y;
      print "ok 16\n";
  }

    { # Check byte length of $b
	use bytes; my $y = length($b);
	print "not " unless $y == 1;
	print "ok 17\n";
    }

    print F $b,"\n"; # Don't upgrades $b

    { # Check byte length of $b
	use bytes; my $y = length($b);
	print "not ($y) " unless $y == 1;
	print "ok 18\n";
    }

    {
	my $x = tell(F);
	{ use bytes; if (ord('A')==193){$y += 2;}else{$y += 3;}} # EBCDIC ASCII
	print "not ($x,$y) " unless $x == $y;
	print "ok 19\n";
    }

    close F;

    open F, "a" or die $!; # Not UTF
    binmode(F, ":bytes");
    $x = <F>;
    chomp($x);
    $chr = v196.172.194.130;
    if (ord('A') == 193) { $chr = v141.83.130; } # EBCDIC
    printf "not (%vd) ", $x unless $x eq $chr;
    print "ok 20\n";

    open F, "<:utf8", "a" or die $!;
    $x = <F>;
    chomp($x);
    close F;
    printf "not (%vd) ", $x unless $x eq chr(300).chr(130);
    print "ok 21\n";

    open F, ">", "a" or die $!;
    if (${^OPEN} =~ /:utf8/) {
        binmode(F, ":bytes:");
    }

    # Now let's make it suffer.
    my $w;
    {
	use warnings 'utf8';
	local $SIG{__WARN__} = sub { $w = $_[0] };
	print F $a;
	print "not " if ($@ || $w !~ /Wide character in print/i);
    }
    print "ok 22\n";
}

# Hm. Time to get more evil.
open F, ">:utf8", "a" or die $!;
print F $a;
binmode(F, ":bytes");
print F chr(130)."\n";
close F;

open F, "<", "a" or die $!;
binmode(F, ":bytes");
$x = <F>; chomp $x;
$chr = v196.172.130;
if (ord('A') == 193) { $chr = v141.83.130; } # EBCDIC
print "not " unless $x eq $chr;
print "ok 23\n";

# Right.
open F, ">:utf8", "a" or die $!;
print F $a;
close F;
open F, ">>", "a" or die $!;
print F chr(130)."\n";
close F;

open F, "<", "a" or die $!;
$x = <F>; chomp $x;
print "not " unless $x eq $chr;
print "ok 24\n";

# Now we have a deformed file.

if (ord('A') == 193) {
    print "ok 25 # Skip: EBCDIC\n"; # EBCDIC doesn't complain
} else {
    open F, "<:utf8", "a" or die $!;
    $x = <F>; chomp $x;
    local $SIG{__WARN__} = sub { print "ok 25\n" };
    eval { sprintf "%vd\n", $x };
}

close F;
unlink('a');

open F, ">:utf8", "a";
@a = map { chr(1 << ($_ << 2)) } 0..5; # 0x1, 0x10, .., 0x100000
unshift @a, chr(0); # ... and a null byte in front just for fun
print F @a;
close F;

my $c;

# read() should work on characters, not bytes
open F, "<:utf8", "a";
$a = 0;
for (@a) {
    unless (($c = read(F, $b, 1) == 1)  &&
            length($b)           == 1  &&
            ord($b)              == ord($_) &&
            tell(F)              == ($a += bytes::length($b))) {
        print '# ord($_)           == ', ord($_), "\n";
        print '# ord($b)           == ', ord($b), "\n";
        print '# length($b)        == ', length($b), "\n";
        print '# bytes::length($b) == ', bytes::length($b), "\n";
        print '# tell(F)           == ', tell(F), "\n";
        print '# $a                == ', $a, "\n";
        print '# $c                == ', $c, "\n";
        print "not ";
        last;
    }
}
close F;
print "ok 26\n";

{
    # Check that warnings are on on I/O, and that they can be muffled.

    local $SIG{__WARN__} = sub { $@ = shift };

    undef $@;
    open F, ">a";
    binmode(F, ":bytes");
    print F chr(0x100);
    close(F);

    print $@ =~ /Wide character in print/ ? "ok 27\n" : "not ok 27\n";

    undef $@;
    open F, ">:utf8", "a";
    print F chr(0x100);
    close(F);

    print defined $@ ? "not ok 28\n" : "ok 28\n";

    undef $@;
    open F, ">a";
    binmode(F, ":utf8");
    print F chr(0x100);
    close(F);

    print defined $@ ? "not ok 29\n" : "ok 29\n";

    no warnings 'utf8';

    undef $@;
    open F, ">a";
    print F chr(0x100);
    close(F);

    print defined $@ ? "not ok 30\n" : "ok 30\n";

    use warnings 'utf8';

    undef $@;
    open F, ">a";
    binmode(F, ":bytes");
    print F chr(0x100);
    close(F);

    print $@ =~ /Wide character in print/ ? "ok 31\n" : "not ok 31\n";
}

# sysread() and syswrite() tested in lib/open.t since Fnctl is used

END {
    1 while unlink "a";
    1 while unlink "b";
}

