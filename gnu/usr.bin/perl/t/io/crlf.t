#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

use Config;

require "test.pl";

my $file = "crlf$$.dat";
END {
 unlink($file);
}

if (find PerlIO::Layer 'perlio') {
 plan(tests => 7);
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

 ok(close(FOO));
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
