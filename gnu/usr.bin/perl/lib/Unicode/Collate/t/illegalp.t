
BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
}

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use Test;
BEGIN { plan tests => 17 };

use strict;
use warnings;

ok(1);

#
# No test for Unicode::Collate is included in this .t file.
#
# UCA conformance test requires completely ignorable characters
# (including noncharacters) must be able to be ordered in code point order;
# If not so, Unicode::Collate must not be compliant with UCA.
# 
# ~~~ CollationTest_SHIFTED.txt in CollationTest-4.0.0
#
# 206F 0021;	# ! NOMINAL DIGIT SHAPES	[| | | 0251]
# D800 0021;	# ! <surrogate-D800>	[| | | 0251]
# DFFF 0021;	# ! <surrogate-DFFF>	[| | | 0251]
# FDD0 0021;	# ! <noncharacter-FDD0>	[| | | 0251]
# FFFB 0021;	# ! INTERLINEAR ANNOTATION TERMINATOR	[| | | 0251]
# FFFE 0021;	# ! <noncharacter-FFFE>	[| | | 0251]
# FFFF 0021;	# ! <noncharacter-FFFF>	[| | | 0251]
# 1D165 0021;	# ! MS. Cm. STEM	[| | | 0251]
#
# ~~~ CollationTest_NON_IGNORABLE.txt in CollationTest-4.0.0
#
# 206F 0021;	# ! NOMINAL DIGIT SHAPES	[0251 | 0020 | 0002 |]
# D800 0021;	# ! <surrogate-D800>	[0251 | 0020 | 0002 |]
# DFFF 0021;	# ! <surrogate-DFFF>	[0251 | 0020 | 0002 |]
# FDD0 0021;	# ! <noncharacter-FDD0>	[0251 | 0020 | 0002 |]
# FFFB 0021;	# ! INTERLINEAR ANNOTATION TERMINATOR	[0251 | 0020 | 0002 |]
# FFFE 0021;	# ! <noncharacter-FFFE>	[0251 | 0020 | 0002 |]
# FFFF 0021;	# ! <noncharacter-FFFF>	[0251 | 0020 | 0002 |]
# 1D165 0021;	# ! MS. Cm. STEM	[0251 | 0020 | 0002 |]
#

no warnings 'utf8';

ok("\x{206F}!" lt "\x{D800}!");
ok(pack('U*', 0x206F, 0x21) lt pack('U*', 0xD800, 0x21));

ok("\x{D800}!" lt "\x{DFFF}!");
ok(pack('U*', 0xD800, 0x21) lt pack('U*', 0xDFFF, 0x21));

ok("\x{DFFF}!" lt "\x{FDD0}!");
ok(pack('U*', 0xDFFF, 0x21) lt pack('U*', 0xFDD0, 0x21) );

ok("\x{FDD0}!" lt "\x{FFFB}!");
ok(pack('U*', 0xFDD0, 0x21) lt pack('U*', 0xFFFB, 0x21));

ok("\x{FFFB}!" lt "\x{FFFE}!");
ok(pack('U*', 0xFFFB, 0x21) lt pack('U*', 0xFFFE, 0x21));

ok("\x{FFFE}!" lt "\x{FFFF}!");
ok(pack('U*', 0xFFFE, 0x21) lt pack('U*', 0xFFFF, 0x21));

ok("\x{FFFF}!" lt "\x{1D165}!");
ok(pack('U*', 0xFFFF, 0x21) lt pack('U*', 0x1D165, 0x21));

ok("\000!" lt "\x{FFFF}!");
ok(pack('U*', 0, 0x21) lt pack('U*', 0xFFFF, 0x21));

