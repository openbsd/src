#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

use Config;

require "test.pl";

my $file = "crlf$$.dat";
END {
    1 while unlink($file);
}

if (find PerlIO::Layer 'perlio') {
 plan(tests => 16);
 ok(open(FOO,">:crlf",$file));
 ok(print FOO 'a'.((('a' x 14).qq{\n}) x 2000) || close(FOO));
 ok(open(FOO,"<:crlf",$file));

 my $text;
 { local $/; $text = <FOO> }
 is(count_chars($text, "\015\012"), 0);
 is(count_chars($text, "\n"), 2000);

 binmode(FOO);
 seek(FOO,0,0);
 { local $/; $text = <FOO> }
 is(count_chars($text, "\015\012"), 2000);

 SKIP:
 {
  skip("miniperl can't rely on loading PerlIO::scalar")
      if $ENV{PERL_CORE_MINITEST};
  skip("no PerlIO::scalar") unless $Config{extensions} =~ m!\bPerlIO/scalar\b!;
  require PerlIO::scalar;
  my $fcontents = join "", map {"$_\015\012"} "a".."zzz";
  open my $fh, "<:crlf", \$fcontents;
  local $/ = "xxx";
  local $_ = <$fh>;
  my $pos = tell $fh; # pos must be behind "xxx", before "\nyyy\n"
  seek $fh, $pos, 0;
  $/ = "\n";
  $s = <$fh>.<$fh>;
  ok($s eq "\nxxy\n");
 }

 ok(close(FOO));

 # binmode :crlf should not cumulate.
 # Try it first once and then twice so that even UNIXy boxes
 # get to exercise this, for DOSish boxes even once is enough.
 # Try also pushing :utf8 first so that there are other layers
 # in between (this should not matter: CRLF layers still should
 # not accumulate).
 for my $utf8 ('', ':utf8') {
     for my $binmode (1..2) {
	 open(FOO, ">$file");
	 # require PerlIO; print PerlIO::get_layers(FOO), "\n";
	 binmode(FOO, "$utf8:crlf") for 1..$binmode;
	 # require PerlIO; print PerlIO::get_layers(FOO), "\n";
	 print FOO "Hello\n";
	 close FOO;
	 open(FOO, "<$file");
	 binmode(FOO);
	 my $foo = scalar <FOO>;
	 close FOO;
	 print join(" ", "#", map { sprintf("%02x", $_) } unpack("C*", $foo)),
	       "\n";
	 ok($foo =~ /\x0d\x0a$/);
	 ok($foo !~ /\x0d\x0d/);
     }
 }
}
else {
 skip_all("No perlio, so no :crlf");
}

sub count_chars {
  my($text, $chars) = @_;
  my $seen = 0;
  $seen++ while $text =~ /$chars/g;
  return $seen;
}
