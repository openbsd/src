# test CGI::Util::escape
use Test::More tests => 4;
use_ok("CGI::Util");

# Byte strings should be escaped byte by byte:
# 1) not a valid utf-8 sequence:
my $uri = "pe\x{f8}\x{ed}\x{e8}ko.ogg";
is(CGI::Util::escape($uri), "pe%F8%ED%E8ko.ogg", "Escape a Latin-2 string");

# 2) is a valid utf-8 sequence, but not an UTF-8-flagged string
#    This happens often: people write utf-8 strings to source, but forget
#    to tell perl about it by "use utf8;"--this is obviously wrong, but we
#    have to handle it gracefully, for compatibility with GCI.pm under
#    perl-5.8.x
#
$uri = "pe\x{c5}\x{99}\x{c3}\x{ad}\x{c4}\x{8d}ko.ogg";
is(CGI::Util::escape($uri), "pe%C5%99%C3%AD%C4%8Dko.ogg",
	"Escape an utf-8 byte string");

SKIP:
{
	# This tests CGI::Util::escape() when fed with UTF-8-flagged string
	# -- dankogai
	skip("Unicode strings not available in $]", 1) if ($] < 5.008);
	$uri = "\x{5c0f}\x{98fc} \x{5f3e}.txt"; # KOGAI, Dan, in Kanji
	is(CGI::Util::escape($uri), "%E5%B0%8F%E9%A3%BC%20%E5%BC%BE.txt",
   		"Escape string with UTF-8 flag");
}
__END__
