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
is(CGI::Util::escape($uri), "%E5%B0%8F%E9%A3%BC%20%E5%BC%BE.txt",
   "# Escape string with UTF-8 flag");
__END__
