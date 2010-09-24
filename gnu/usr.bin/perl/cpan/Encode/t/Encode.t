BEGIN {
    if ($ENV{'PERL_CORE'}){
        chdir 't';
        unshift @INC, '../lib';
    }
    if (ord("A") == 193) {
        print "1..0 # Skip: EBCDIC\n";
        exit 0;
    }
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bEncode\b/) {
      print "1..0 # Skip: Encode was not built\n";
      exit 0;
    }
}
use strict;
use Test;
use Encode qw(from_to encode decode
          encode_utf8 decode_utf8
          find_encoding is_utf8);
use charnames qw(greek);
my @encodings = grep(/iso-?8859/,Encode::encodings());
my $n = 2;
my @character_set = ('0'..'9', 'A'..'Z', 'a'..'z');
my @source = qw(ascii iso8859-1 cp1250);
my @destiny = qw(cp1047 cp37 posix-bc);
my @ebcdic_sets = qw(cp1047 cp37 posix-bc);
plan test => 38+$n*@encodings + 2*@source*@destiny*@character_set + 2*@ebcdic_sets*256 + 6 + 2;
my $str = join('',map(chr($_),0x20..0x7E));
my $cpy = $str;
ok(length($str),from_to($cpy,'iso8859-1','Unicode'),"Length Wrong");
ok($cpy,$str,"ASCII mangled by translating from iso8859-1 to Unicode");
$cpy = $str;
ok(from_to($cpy,'Unicode','iso8859-1'),length($str),"Length wrong");
ok($cpy,$str,"ASCII mangled by translating from Unicode to iso8859-1");

$str = join('',map(chr($_),0xa0..0xff));
$cpy = $str;
ok(length($str),from_to($cpy,'iso8859-1','Unicode'),"Length Wrong");

my $sym = Encode->getEncoding('symbol');
my $uni = $sym->decode(encode(ascii => 'a'));
ok("\N{alpha}",substr($uni,0,1),"alpha does not map to symbol 'a'");
$str = $sym->encode("\N{Beta}");
ok("B",decode(ascii => substr($str,0,1)),"Symbol 'B' does not map to Beta");

foreach my $enc (qw(symbol dingbats ascii),@encodings)
 {
  my $tab = Encode->getEncoding($enc);
  ok(1,defined($tab),"Could not load $enc");
  $str = join('',map(chr($_),0x20..0x7E));
  $uni = $tab->decode($str);
  $cpy = $tab->encode($uni);
  ok($cpy,$str,"$enc mangled translating to Unicode and back");
 }

# On ASCII based machines see if we can map several codepoints from
# three distinct ASCII sets to three distinct EBCDIC coded character sets.
# On EBCDIC machines see if we can map from three EBCDIC sets to three
# distinct ASCII sets.

my @expectation = (240..249, 193..201,209..217,226..233, 129..137,145..153,162..169);
if (ord('A') != 65) {
    my @temp = @destiny;
    @destiny = @source;
    @source = @temp;
    undef(@temp);
    @expectation = (48..57, 65..90, 97..122);
}

foreach my $to (@destiny)
 {
  foreach my $from (@source)
   {
    my @expected = @expectation;
    foreach my $chr (@character_set)
     {
      my $native_chr = $chr;
      my $cpy = $chr;
      my $rc = from_to($cpy,$from,$to);
      ok(1,$rc,"Could not translate from $from to $to");
      ok(ord($cpy),shift(@expected),"mangled translating $native_chr from $from to $to");
     }
   }
 }

# On either ASCII or EBCDIC machines ensure we can take the full one
# byte repetoire to EBCDIC sets and back.

my $enc_as = 'iso8859-1';
foreach my $enc_eb (@ebcdic_sets)
 {
  foreach my $ord (0..255)
   {
    $str = chr($ord);
    my $rc = from_to($str,$enc_as,$enc_eb);
    $rc += from_to($str,$enc_eb,$enc_as);
    ok($rc,2,"return code for $ord $enc_eb -> $enc_as -> $enc_eb was not obtained");
    ok($ord,ord($str),"$enc_as mangled translating $ord to $enc_eb and back");
   }
 }

my $mime = find_encoding('iso-8859-2');
ok(defined($mime),1,"Cannot find MIME-ish'iso-8859-2'");
my $x11 = find_encoding('iso8859-2');
ok(defined($x11),1,"Cannot find X11-ish 'iso8859-2'");
ok($mime,$x11,"iso8598-2 and iso-8859-2 not same");
my $spc = find_encoding('iso 8859-2');
ok(defined($spc),1,"Cannot find 'iso 8859-2'");
ok($spc,$mime,"iso 8859-2 and iso-8859-2 not same");

for my $i (256,128,129,256)
 {
  my $c = chr($i);
  my $s = "$c\n".sprintf("%02X",$i);
  ok(utf8::valid($s),1,"concat of $i botched");
  utf8::upgrade($s);
  ok(utf8::valid($s),1,"concat of $i botched");
 }

# Spot check a few points in/out of utf8
for my $i (ord('A'),128,256,0x20AC)
 {
  my $c = chr($i);
  my $o = encode_utf8($c);
  ok(decode_utf8($o),$c,"decode_utf8 not inverse of encode_utf8 for $i");
  ok(encode('utf8',$c),$o,"utf8 encode by name broken for $i");
  ok(decode('utf8',$o),$c,"utf8 decode by name broken for $i");
 }


# is_utf8

ok(  is_utf8("\x{100}"));
ok(! is_utf8("a"));
ok(! is_utf8(""));
"\x{100}" =~ /(.)/;
ok(  is_utf8($1)); # ID 20011127.151
$a = $1;
ok(  is_utf8($a));
$a = "\x{100}";
chop $a;
ok(  is_utf8($a)); # weird but true: an empty UTF-8 string

# non-string arguments
package Encode::Dummy;
use overload q("") => sub { $_[0]->[0] };
sub new { my $class = shift; bless [ @_  ] => $class }
package main;
ok(decode(latin1 => Encode::Dummy->new("foobar")), "foobar");
ok(encode(utf8   => Encode::Dummy->new("foobar")), "foobar");
