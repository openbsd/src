#
# This tests CGI::Util::escape() when fed with UTF-8-flagged string
# -- dankogai
BEGIN {
    if ($] < 5.008) {
       print "1..0 # \$] == $] < 5.008\n";
       exit(0);
    }
}

use Test::More tests => 2;
use_ok("CGI::Util");
my $uri = "\x{5c0f}\x{98fc} \x{5f3e}.txt"; # KOGAI, Dan, in Kanji
if (ord('A') == 193) { # EBCDIC.
    is(CGI::Util::escape($uri), "%FC%C3%A0%EE%F9%E5%E7%F8%20%FC%C3%C7%CA.txt",
       "# Escape string with UTF-8 (UTF-EBCDIC) flag");
} else {
    is(CGI::Util::escape($uri), "%E5%B0%8F%E9%A3%BC%20%E5%BC%BE.txt",
       "# Escape string with UTF-8 flag");
}
__END__
