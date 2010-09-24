#!./perl

my @WARN;

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib';
	require './test.pl';
    }
    $SIG{__WARN__} = sub { push @WARN, @_ };
}

require File::Spec;

$| = 1;

print "1..80\n";

use charnames ':full';

print "not " unless "Here\N{EXCLAMATION MARK}?" eq "Here!?";
print "ok 1\n";

{
  use bytes;			# TEST -utf8 can switch utf8 on

  print "# \$res=$res \$\@='$@'\nnot "
    if $res = eval <<'EOE'
use charnames ":full";
"Here: \N{CYRILLIC SMALL LETTER BE}!";
1
EOE
      or $@ !~ /above 0xFF/;
  print "ok 2\n";
  # print "# \$res=$res \$\@='$@'\n";

  print "# \$res=$res \$\@='$@'\nnot "
    if $res = eval <<'EOE'
use charnames 'cyrillic';
"Here: \N{Be}!";
1
EOE
      or $@ !~ /CYRILLIC CAPITAL LETTER BE.*above 0xFF/;
  print "ok 3\n";
}

# If octal representation of unicode char is \0xyzt, then the utf8 is \3xy\2zt
if (ord('A') == 65) { # as on ASCII or UTF-8 machines
    $encoded_be = "\320\261";
    $encoded_alpha = "\316\261";
    $encoded_bet = "\327\221";
    $encoded_deseng = "\360\220\221\215";
}
else { # EBCDIC where UTF-EBCDIC may be used (this may be 1047 specific since
       # UTF-EBCDIC is codepage specific)
    $encoded_be = "\270\102\130";
    $encoded_alpha = "\264\130";
    $encoded_bet = "\270\125\130";
    $encoded_deseng = "\336\102\103\124";
}

sub to_bytes {
    unpack"U0a*", shift;
}

{
  use charnames ':full';

  print "not " unless to_bytes("\N{CYRILLIC SMALL LETTER BE}") eq $encoded_be;
  print "ok 4\n";

  use charnames qw(cyrillic greek :short);

  print "not " unless to_bytes("\N{be},\N{alpha},\N{hebrew:bet}")
    eq "$encoded_be,$encoded_alpha,$encoded_bet";
  print "ok 5\n";
}

{
    use charnames ':full';
    print "not " unless "\x{263a}" eq "\N{WHITE SMILING FACE}";
    print "ok 6\n";
    print "not " unless length("\x{263a}") == 1;
    print "ok 7\n";
    print "not " unless length("\N{WHITE SMILING FACE}") == 1;
    print "ok 8\n";
    print "not " unless sprintf("%vx", "\x{263a}") eq "263a";
    print "ok 9\n";
    print "not " unless sprintf("%vx", "\N{WHITE SMILING FACE}") eq "263a";
    print "ok 10\n";
    print "not " unless sprintf("%vx", "\xFF\N{WHITE SMILING FACE}") eq "ff.263a";
    print "ok 11\n";
    print "not " unless sprintf("%vx", "\x{ff}\N{WHITE SMILING FACE}") eq "ff.263a";
    print "ok 12\n";
}

{
   use charnames qw(:full);
   use utf8;

    my $x = "\x{221b}";
    my $named = "\N{CUBE ROOT}";

    print "not " unless ord($x) == ord($named);
    print "ok 13\n";
}

{
   use charnames qw(:full);
   use utf8;
   print "not " unless "\x{100}\N{CENT SIGN}" eq "\x{100}"."\N{CENT SIGN}";
   print "ok 14\n";
}

{
  use charnames ':full';

  print "not "
      unless to_bytes("\N{DESERET SMALL LETTER ENG}") eq $encoded_deseng;
  print "ok 15\n";
}

{
  # 20001114.001

  no utf8; # naked Latin-1

  if (ord("Ä") == 0xc4) { # Try to do this only on Latin-1.
      use charnames ':full';
      my $text = "\N{LATIN CAPITAL LETTER A WITH DIAERESIS}";
      print "not " unless $text eq "\xc4" && ord($text) == 0xc4;
      print "ok 16\n";
  } else {
      print "ok 16 # Skip: not Latin-1\n";
  }
}

{
    print "not " unless charnames::viacode(0x1234) eq "ETHIOPIC SYLLABLE SEE";
    print "ok 17\n";

    # Unused Hebrew.
    print "not " if defined charnames::viacode(0x0590);
    print "ok 18\n";
}

{
    print "not " unless
	sprintf("%04X", charnames::vianame("GOTHIC LETTER AHSA")) eq "10330";
    print "ok 19\n";

    print "not " if
	defined charnames::vianame("NONE SUCH");
    print "ok 20\n";
}

{
    # check that caching at least hasn't broken anything

    print "not " unless charnames::viacode(0x1234) eq "ETHIOPIC SYLLABLE SEE";
    print "ok 21\n";

    print "not " unless
	sprintf("%04X", charnames::vianame("GOTHIC LETTER AHSA")) eq "10330";
    print "ok 22\n";

}

print "not " unless "\N{CHARACTER TABULATION}" eq "\t";
print "ok 23\n";

print "not " unless "\N{ESCAPE}" eq "\e";
print "ok 24\n";

print "not " unless "\N{NULL}" eq "\c@";
print "ok 25\n";

print "not " unless "\N{LINE FEED (LF)}" eq "\n";
print "ok 26\n";

print "not " unless "\N{LINE FEED}" eq "\n";
print "ok 27\n";

print "not " unless "\N{LF}" eq "\n";
print "ok 28\n";

my $nel = ord("A") == 193 ? qr/^(?:\x15|\x25)$/ : qr/^\x85$/;

print "not " unless "\N{NEXT LINE (NEL)}" =~ $nel;
print "ok 29\n";

print "not " unless "\N{NEXT LINE}" =~ $nel;
print "ok 30\n";

print "not " unless "\N{NEL}" =~ $nel;
print "ok 31\n";

print "not " unless "\N{BYTE ORDER MARK}" eq chr(0xFEFF);
print "ok 32\n";

print "not " unless "\N{BOM}" eq chr(0xFEFF);
print "ok 33\n";

{
    use warnings 'deprecated';

    print "not " unless "\N{HORIZONTAL TABULATION}" eq "\t";
    print "ok 34\n";

    print "not " unless grep { /"HORIZONTAL TABULATION" is deprecated/ } @WARN;
    print "ok 35\n";

    no warnings 'deprecated';

    print "not " unless "\N{VERTICAL TABULATION}" eq "\013";
    print "ok 36\n";

    print "not " if grep { /"VERTICAL TABULATION" is deprecated/ } @WARN;
    print "ok 37\n";
}

print "not " unless charnames::viacode(0xFEFF) eq "ZERO WIDTH NO-BREAK SPACE";
print "ok 38\n";

{
    use warnings;
    print "not " unless ord("\N{BOM}") == 0xFEFF;
    print "ok 39\n";
}

print "not " unless ord("\N{ZWNJ}") == 0x200C;
print "ok 40\n";

print "not " unless ord("\N{ZWJ}") == 0x200D;
print "ok 41\n";

print "not " unless "\N{U+263A}" eq "\N{WHITE SMILING FACE}";
print "ok 42\n";

{
    print "not " unless
	0x3093 == charnames::vianame("HIRAGANA LETTER N");
    print "ok 43\n";

    print "not " unless
	0x0397 == charnames::vianame("GREEK CAPITAL LETTER ETA");
    print "ok 44\n";
}

print "not " if defined charnames::viacode(0x110000);
print "ok 45\n";

print "not " if grep { /you asked for U+110000/ } @WARN;
print "ok 46\n";


# ---- Alias extensions

my $alifile = File::Spec->catfile(File::Spec->updir, qw(lib unicore xyzzy_alias.pl));
my $i = 0;

my @prgs;
{   local $/ = undef;
    @prgs = split "\n########\n", <DATA>;
    }

my $i = 46;
for (@prgs) {
    my ($code, $exp) = ((split m/\nEXPECT\n/), '$');
    my ($prog, $fil) = ((split m/\nFILE\n/, $code), "");
    my $tmpfile = tempfile();
    open my $tmp, "> $tmpfile" or die "Could not open $tmpfile: $!";
    print $tmp $prog, "\n";
    close $tmp or die "Could not close $tmpfile: $!";
    if ($fil) {
	$fil .= "\n";
	open my $ali, "> $alifile" or die "Could not open $alifile: $!";
	print $ali $fil;
	close $ali or die "Could not close $alifile: $!";
	}
    my $res = runperl( switches => $switch, 
                       progfile => $tmpfile,
                       stderr => 1 );
    my $status = $?;
    $res =~ s/[\r\n]+$//;
    $res =~ s/tmp\d+/-/g;			# fake $prog from STDIN
    $res =~ s/\n%[A-Z]+-[SIWEF]-.*$//		# clip off DCL status msg
	if $^O eq "VMS";
    $exp =~ s/[\r\n]+$//;
    my $pfx = ($res =~ s/^PREFIX\n//);
    my $rexp = qr{^$exp};
    if ($res =~ s/^SKIPPED\n//) {
	print "$results\n";
	}
    elsif (($pfx and $res !~ /^\Q$expected/) or
	  (!$pfx and $res !~ $rexp)) {
        print STDERR
	    "PROG:\n$prog\n",
	    "FILE:\n$fil",
	    "EXPECTED:\n$exp\n",
	    "GOT:\n$res\n";
        print "not ";
	}
    print "ok ", ++$i, "\n";
    $fil or next;
    1 while unlink $alifile;
    }

# [perl #30409] charnames.pm clobbers default variable
$_ = 'foobar';
eval "use charnames ':full';";
print "not " unless $_ eq 'foobar';
print "ok 74\n";

# Unicode slowdown noted by Phil Pennock, traced to a bug fix in index
# SADAHIRO Tomoyuki's suggestion is to ensure that the UTF-8ness of both
# arguments are indentical before calling index.
# To do this can take advantage of the fact that unicore/Name.pl is 7 bit
# (or at least should be). So assert that that it's true here.

my $names = do "unicore/Name.pl";
print defined $names ? "ok 75\n" : "not ok 75\n";
if (ord('A') == 65) { # as on ASCII or UTF-8 machines
  my $non_ascii = $names =~ tr/\0-\177//c;
  print $non_ascii ? "not ok 76 # $non_ascii\n" : "ok 76\n";
} else {
  print "ok 76\n";
}

# Verify that charnames propagate to eval("")
my $evaltry = eval q[ "Eval: \N{LEFT-POINTING DOUBLE ANGLE QUOTATION MARK}" ];
if ($@) {
    print "# $@not ok 77\nnot ok 78\n";
} else {
    print "ok 77\n";
    print "not " unless $evaltry eq "Eval: \N{LEFT-POINTING DOUBLE ANGLE QUOTATION MARK}";
    print "ok 78\n";
}

# Verify that db includes the normative NameAliases.txt names
print "not " unless "\N{U+1D0C5}" eq "\N{BYZANTINE MUSICAL SYMBOL FTHORA SKLIRON CHROMA VASIS}";
print "ok 79\n";

# [perl #73174] use of \N{FOO} used to reset %^H

{
    use charnames ":full";
    my $res;
    BEGIN { $^H{73174} = "foo" }
    BEGIN { $res = ($^H{73174} // "") }
    # forces loading of utf8.pm, which used to reset %^H
    $res .= '-1' if ":" =~ /\N{COLON}/i;
    BEGIN { $res .= '-' . ($^H{73174} // "") }
    $res .= '-' . ($^H{73174} // "");
    $res .= '-2' if ":" =~ /\N{COLON}/;
    $res .= '-3' if ":" =~ /\N{COLON}/i;
    print $res eq "foo-foo-1--2-3" ? "" : "not ",
	"ok 80 - \$^H{foo} correct after /\\N{bar}/i (res=$res)\n";
}

__END__
# unsupported pragma
use charnames ":scoobydoo";
"Here: \N{e_ACUTE}!\n";
EXPECT
unsupported special ':scoobydoo' in charnames at
########
# wrong type of alias (missing colon)
use charnames "alias";
"Here: \N{e_ACUTE}!\n";
EXPECT
Unknown charname 'e_ACUTE' at
########
# alias without an argument
use charnames ":alias";
"Here: \N{e_ACUTE}!\n";
EXPECT
:alias needs an argument in charnames at
########
# reversed sequence
use charnames ":alias" => ":full";
"Here: \N{e_ACUTE}!\n";
EXPECT
:alias cannot use existing pragma :full \(reversed order\?\) at
########
# alias with hashref but no :full
use charnames ":alias" => { e_ACUTE => "LATIN SMALL LETTER E WITH ACUTE" };
"Here: \N{e_ACUTE}!\n";
EXPECT
Unknown charname 'LATIN SMALL LETTER E WITH ACUTE' at
########
# alias with hashref but with :short
use charnames ":short", ":alias" => { e_ACUTE => "LATIN SMALL LETTER E WITH ACUTE" };
"Here: \N{e_ACUTE}!\n";
EXPECT
Unknown charname 'LATIN SMALL LETTER E WITH ACUTE' at
########
# alias with hashref to :full OK
use charnames ":full", ":alias" => { e_ACUTE => "LATIN SMALL LETTER E WITH ACUTE" };
"Here: \N{e_ACUTE}!\n";
EXPECT
$
########
# alias with hashref to :short but using :full
use charnames ":full", ":alias" => { e_ACUTE => "LATIN:e WITH ACUTE" };
"Here: \N{e_ACUTE}!\n";
EXPECT
Unknown charname 'LATIN:e WITH ACUTE' at
########
# alias with hashref to :short OK
use charnames ":short", ":alias" => { e_ACUTE => "LATIN:e WITH ACUTE" };
"Here: \N{e_ACUTE}!\n";
EXPECT
$
########
# alias with bad hashref
use charnames ":short", ":alias" => "e_ACUTE";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
unicore/e_ACUTE_alias.pl cannot be used as alias file for charnames at
########
# alias with arrayref
use charnames ":short", ":alias" => [ e_ACUTE => "LATIN:e WITH ACUTE" ];
"Here: \N{e_ACUTE}!\n";
EXPECT
Only HASH reference supported as argument to :alias at
########
# alias with bad hashref
use charnames ":short", ":alias" => { e_ACUTE => "LATIN:e WITH ACUTE", "a_ACUTE" };
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
Use of uninitialized value
########
# alias with hashref two aliases
use charnames ":short", ":alias" => {
    e_ACUTE => "LATIN:e WITH ACUTE",
    a_ACUTE => "",
    };
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
Unknown charname '' at
########
# alias with hashref two aliases
use charnames ":short", ":alias" => {
    e_ACUTE => "LATIN:e WITH ACUTE",
    a_ACUTE => "LATIN:a WITH ACUTE",
    };
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
$
########
# alias with hashref using mixed aliasses
use charnames ":short", ":alias" => {
    e_ACUTE => "LATIN:e WITH ACUTE",
    a_ACUTE => "LATIN SMALL LETTER A WITH ACUT",
    };
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
Unknown charname 'LATIN SMALL LETTER A WITH ACUT' at
########
# alias with hashref using mixed aliasses
use charnames ":short", ":alias" => {
    e_ACUTE => "LATIN:e WITH ACUTE",
    a_ACUTE => "LATIN SMALL LETTER A WITH ACUTE",
    };
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
Unknown charname 'LATIN SMALL LETTER A WITH ACUTE' at
########
# alias with hashref using mixed aliasses
use charnames ":full", ":alias" => {
    e_ACUTE => "LATIN:e WITH ACUTE",
    a_ACUTE => "LATIN SMALL LETTER A WITH ACUTE",
    };
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
Unknown charname 'LATIN:e WITH ACUTE' at
########
# alias with nonexisting file
use charnames ":full", ":alias" => "xyzzy";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
unicore/xyzzy_alias.pl cannot be used as alias file for charnames at
########
# alias with bad file name
use charnames ":full", ":alias" => "xy 7-";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
Charnames alias files can only have identifier characters at
########
# alias with non_absolute (existing) file name (which it should /not/ use)
use charnames ":full", ":alias" => "perl";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
EXPECT
unicore/perl_alias.pl cannot be used as alias file for charnames at
########
# alias with bad file
use charnames ":full", ":alias" => "xyzzy";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
FILE
#!perl
0;
EXPECT
unicore/xyzzy_alias.pl did not return a \(valid\) list of alias pairs at
########
# alias with file with empty list
use charnames ":full", ":alias" => "xyzzy";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
FILE
#!perl
();
EXPECT
Unknown charname 'e_ACUTE' at
########
# alias with file OK but file has :short aliasses
use charnames ":full", ":alias" => "xyzzy";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
FILE
#!perl
(   e_ACUTE => "LATIN:e WITH ACUTE",
    a_ACUTE => "LATIN:a WITH ACUTE",
    );
EXPECT
Unknown charname 'LATIN:e WITH ACUTE' at
########
# alias with :short and file OK
use charnames ":short", ":alias" => "xyzzy";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
FILE
#!perl
(   e_ACUTE => "LATIN:e WITH ACUTE",
    a_ACUTE => "LATIN:a WITH ACUTE",
    );
EXPECT
$
########
# alias with :short and file OK has :long aliasses
use charnames ":short", ":alias" => "xyzzy";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
FILE
#!perl
(   e_ACUTE => "LATIN SMALL LETTER E WITH ACUTE",
    a_ACUTE => "LATIN SMALL LETTER A WITH ACUTE",
    );
EXPECT
Unknown charname 'LATIN SMALL LETTER E WITH ACUTE' at
########
# alias with file implicit :full but file has :short aliasses
use charnames ":alias" => ":xyzzy";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
FILE
#!perl
(   e_ACUTE => "LATIN:e WITH ACUTE",
    a_ACUTE => "LATIN:a WITH ACUTE",
    );
EXPECT
Unknown charname 'LATIN:e WITH ACUTE' at
########
# alias with file implicit :full and file has :long aliasses
use charnames ":alias" => ":xyzzy";
"Here: \N{e_ACUTE}\N{a_ACUTE}!\n";
FILE
#!perl
(   e_ACUTE => "LATIN SMALL LETTER E WITH ACUTE",
    a_ACUTE => "LATIN SMALL LETTER A WITH ACUTE",
    );
EXPECT
$
