#!perl

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
}
plan 912;

eval "#line 8 foo\nsub t004 :method (\$a) { }";
is $@, "Experimental subroutine signatures not enabled at foo line 8\.\n",
    "error when not enabled";

eval "#line 8 foo\nsub t005 (\$) (\$a) { }";
is $@, "Experimental subroutine signatures not enabled at foo line 8\.\n",
    "error when not enabled";

no warnings "illegalproto";

our $a = 123;
our $z;

sub t000 ($a) { $a || "z" }
is prototype(\&t000), "\$a", "(\$a) interpreted as protoype when not enabled";
is &t000(456), 123, "(\$a) not signature when not enabled";
is $a, 123;

no warnings "experimental::signatures";
use feature "signatures";

sub t001 { $a || "z" }
is prototype(\&t001), undef;
is eval("t001()"), 123;
is eval("t001(456)"), 123;
is eval("t001(456, 789)"), 123;
is $a, 123;

sub t002 () { $a || "z" }
is prototype(\&t002), undef;
is eval("t002()"), 123;
is eval("t002(456)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t002(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t003 ( ) { $a || "z" }
is prototype(\&t003), undef;
is eval("t003()"), 123;
is eval("t003(456)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t003(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t006 ($a) { $a || "z" }
is prototype(\&t006), undef;
is eval("t006()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t006(0)"), "z";
is eval("t006(456)"), 456;
is eval("t006(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t006(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t007 ($a, $b) { $a.$b }
is prototype(\&t007), undef;
is eval("t007()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t007(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t007(456, 789)"), "456789";
is eval("t007(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t007(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t008 ($a, $b, $c) { $a.$b.$c }
is prototype(\&t008), undef;
is eval("t008()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t008(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t008(456, 789)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t008(456, 789, 987)"), "456789987";
is eval("t008(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t009 ($abc, $def) { $abc.$def }
is prototype(\&t009), undef;
is eval("t009()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t009(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t009(456, 789)"), "456789";
is eval("t009(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t009(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t010 ($a, $) { $a || "z" }
is prototype(\&t010), undef;
is eval("t010()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t010(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t010(0, 789)"), "z";
is eval("t010(456, 789)"), 456;
is eval("t010(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t010(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t011 ($, $a) { $a || "z" }
is prototype(\&t011), undef;
is eval("t011()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t011(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t011(456, 0)"), "z";
is eval("t011(456, 789)"), 789;
is eval("t011(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t011(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t012 ($, $) { $a || "z" }
is prototype(\&t012), undef;
is eval("t012()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t012(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t012(0, 789)"), 123;
is eval("t012(456, 789)"), 123;
is eval("t012(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t012(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t013 ($) { $a || "z" }
is prototype(\&t013), undef;
is eval("t013()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t013(0)"), 123;
is eval("t013(456)"), 123;
is eval("t013(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t013(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t013(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t014 ($a = 222) { $a // "z" }
is prototype(\&t014), undef;
is eval("t014()"), 222;
is eval("t014(0)"), 0;
is eval("t014(undef)"), "z";
is eval("t014(456)"), 456;
is eval("t014(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t014(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t015 ($a = undef) { $a // "z" }
is prototype(\&t015), undef;
is eval("t015()"), "z";
is eval("t015(0)"), 0;
is eval("t015(undef)"), "z";
is eval("t015(456)"), 456;
is eval("t015(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t015(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t016 ($a = do { $z++; 222 }) { $a // "z" }
$z = 0;
is prototype(\&t016), undef;
is eval("t016()"), 222;
is $z, 1;
is eval("t016(0)"), 0;
is eval("t016(undef)"), "z";
is eval("t016(456)"), 456;
is eval("t016(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t016(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $z, 1;
is eval("t016()"), 222;
is $z, 2;
is $a, 123;

sub t018 { join("/", @_) }
sub t017 ($p = t018 222, $a = 333) { $p // "z" }
is prototype(\&t017), undef;
is eval("t017()"), "222/333";
is $a, 333;
$a = 123;
is eval("t017(0)"), 0;
is eval("t017(undef)"), "z";
is eval("t017(456)"), 456;
is eval("t017(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t017(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t019 ($p = 222, $a = 333) { "$p/$a" }
is prototype(\&t019), undef;
is eval("t019()"), "222/333";
is eval("t019(0)"), "0/333";
is eval("t019(456)"), "456/333";
is eval("t019(456, 789)"), "456/789";
is eval("t019(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t020 :prototype($) { $_[0]."z" }
sub t021 ($p = t020 222, $a = 333) { "$p/$a" }
is prototype(\&t021), undef;
is eval("t021()"), "222z/333";
is eval("t021(0)"), "0/333";
is eval("t021(456)"), "456/333";
is eval("t021(456, 789)"), "456/789";
is eval("t021(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t022 ($p = do { $z += 10; 222 }, $a = do { $z++; 333 }) { "$p/$a" }
$z = 0;
is prototype(\&t022), undef;
is eval("t022()"), "222/333";
is $z, 11;
is eval("t022(0)"), "0/333";
is $z, 12;
is eval("t022(456)"), "456/333";
is $z, 13;
is eval("t022(456, 789)"), "456/789";
is eval("t022(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $z, 13;
is $a, 123;

sub t023 ($a = sub { $_[0]."z" }) { $a->("a")."y" }
is prototype(\&t023), undef;
is eval("t023()"), "azy";
is eval("t023(sub { \"x\".\$_[0].\"x\" })"), "xaxy";
is eval("t023(sub { \"x\".\$_[0].\"x\" }, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t036 ($a = $a."x") { $a."y" }
is prototype(\&t036), undef;
is eval("t036()"), "123xy";
is eval("t036(0)"), "0y";
is eval("t036(456)"), "456y";
is eval("t036(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t120 ($a = $_) { $a // "z" }
is prototype(\&t120), undef;
$_ = "___";
is eval("t120()"), "___";
$_ = "___";
is eval("t120(undef)"), "z";
$_ = "___";
is eval("t120(0)"), 0;
$_ = "___";
is eval("t120(456)"), 456;
$_ = "___";
is eval("t120(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t121 ($a = caller) { $a // "z" }
is prototype(\&t121), undef;
is eval("t121()"), "main";
is eval("t121(undef)"), "z";
is eval("t121(0)"), 0;
is eval("t121(456)"), 456;
is eval("t121(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("package T121::Z; ::t121()"), "T121::Z";
is eval("package T121::Z; ::t121(undef)"), "z";
is eval("package T121::Z; ::t121(0)"), 0;
is eval("package T121::Z; ::t121(456)"), 456;
is eval("package T121::Z; ::t121(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t129 ($a = return 222) { $a."x" }
is prototype(\&t129), undef;
is eval("t129()"), "222";
is eval("t129(0)"), "0x";
is eval("t129(456)"), "456x";
is eval("t129(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

use feature "current_sub";
sub t122 ($c = 5, $r = $c > 0 ? __SUB__->($c - 1) : "") { $c.$r }
is prototype(\&t122), undef;
is eval("t122()"), "543210";
is eval("t122(0)"), "0";
is eval("t122(1)"), "10";
is eval("t122(5)"), "543210";
is eval("t122(5, 789)"), "5789";
is eval("t122(5, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t123 ($list = wantarray) { $list ? "list" : "scalar" }
is prototype(\&t123), undef;
is eval("scalar(t123())"), "scalar";
is eval("(t123())[0]"), "list";
is eval("scalar(t123(0))"), "scalar";
is eval("(t123(0))[0]"), "scalar";
is eval("scalar(t123(1))"), "list";
is eval("(t123(1))[0]"), "list";
is eval("t123(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t124 ($b = (local $a = $a + 1)) { "$a/$b" }
is prototype(\&t124), undef;
is eval("t124()"), "124/124";
is $a, 123;
is eval("t124(456)"), "123/456";
is $a, 123;
is eval("t124(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t125 ($c = (our $t125_counter)++) { $c }
is prototype(\&t125), undef;
is eval("t125()"), 0;
is eval("t125()"), 1;
is eval("t125()"), 2;
is eval("t125(456)"), 456;
is eval("t125(789)"), 789;
is eval("t125()"), 3;
is eval("t125()"), 4;
is eval("t125(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

use feature "state";
sub t126 ($c = (state $s = $z++)) { $c }
is prototype(\&t126), undef;
$z = 222;
is eval("t126(456)"), 456;
is $z, 222;
is eval("t126()"), 222;
is $z, 223;
is eval("t126(456)"), 456;
is $z, 223;
is eval("t126()"), 222;
is $z, 223;
is eval("t126(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $z, 223;
is $a, 123;

sub t127 ($c = do { state $s = $z++; $s++ }) { $c }
is prototype(\&t127), undef;
$z = 222;
is eval("t127(456)"), 456;
is $z, 222;
is eval("t127()"), 222;
is $z, 223;
is eval("t127()"), 223;
is eval("t127()"), 224;
is $z, 223;
is eval("t127(456)"), 456;
is eval("t127(789)"), 789;
is eval("t127()"), 225;
is eval("t127()"), 226;
is eval("t127(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $z, 223;
is $a, 123;

sub t037 ($a = 222, $b = $a."x") { "$a/$b" }
is prototype(\&t037), undef;
is eval("t037()"), "222/222x";
is eval("t037(0)"), "0/0x";
is eval("t037(456)"), "456/456x";
is eval("t037(456, 789)"), "456/789";
is eval("t037(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t128 ($a = 222, $b = ($a = 333)) { "$a/$b" }
is prototype(\&t128), undef;
is eval("t128()"), "333/333";
is eval("t128(0)"), "333/333";
is eval("t128(456)"), "333/333";
is eval("t128(456, 789)"), "456/789";
is eval("t128(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t130 { join(",", @_).";".scalar(@_) }
sub t131 ($a = 222, $b = goto &t130) { "$a/$b" }
is prototype(\&t131), undef;
is eval("t131()"), ";0";
is eval("t131(0)"), "0;1";
is eval("t131(456)"), "456;1";
is eval("t131(456, 789)"), "456/789";
is eval("t131(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

eval "#line 8 foo\nsub t024 (\$a =) { }";
is $@, "Optional parameter lacks default expression at foo line 8\.\n";

sub t025 ($ = undef) { $a // "z" }
is prototype(\&t025), undef;
is eval("t025()"), 123;
is eval("t025(0)"), 123;
is eval("t025(456)"), 123;
is eval("t025(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t025(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t025(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t026 ($ = 222) { $a // "z" }
is prototype(\&t026), undef;
is eval("t026()"), 123;
is eval("t026(0)"), 123;
is eval("t026(456)"), 123;
is eval("t026(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t026(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t026(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t032 ($ = do { $z++; 222 }) { $a // "z" }
$z = 0;
is prototype(\&t032), undef;
is eval("t032()"), 123;
is $z, 1;
is eval("t032(0)"), 123;
is eval("t032(456)"), 123;
is eval("t032(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t032(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t032(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $z, 1;
is $a, 123;

sub t027 ($ =) { $a // "z" }
is prototype(\&t027), undef;
is eval("t027()"), 123;
is eval("t027(0)"), 123;
is eval("t027(456)"), 123;
is eval("t027(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t027(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t027(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t119 ($ =, $a = 333) { $a // "z" }
is prototype(\&t119), undef;
is eval("t119()"), 333;
is eval("t119(0)"), 333;
is eval("t119(456)"), 333;
is eval("t119(456, 789)"), 789;
is eval("t119(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t119(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t028 ($a, $b = 333) { "$a/$b" }
is prototype(\&t028), undef;
is eval("t028()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t028(0)"), "0/333";
is eval("t028(456)"), "456/333";
is eval("t028(456, 789)"), "456/789";
is eval("t028(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t045 ($a, $ = 333) { "$a/" }
is prototype(\&t045), undef;
is eval("t045()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t045(0)"), "0/";
is eval("t045(456)"), "456/";
is eval("t045(456, 789)"), "456/";
is eval("t045(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t046 ($, $b = 333) { "$a/$b" }
is prototype(\&t046), undef;
is eval("t046()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t046(0)"), "123/333";
is eval("t046(456)"), "123/333";
is eval("t046(456, 789)"), "123/789";
is eval("t046(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t047 ($, $ = 333) { "$a/" }
is prototype(\&t047), undef;
is eval("t047()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t047(0)"), "123/";
is eval("t047(456)"), "123/";
is eval("t047(456, 789)"), "123/";
is eval("t047(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t029 ($a, $b, $c = 222, $d = 333) { "$a/$b/$c/$d" }
is prototype(\&t029), undef;
is eval("t029()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t029(0)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t029(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t029(456, 789)"), "456/789/222/333";
is eval("t029(456, 789, 987)"), "456/789/987/333";
is eval("t029(456, 789, 987, 654)"), "456/789/987/654";
is eval("t029(456, 789, 987, 654, 321)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t029(456, 789, 987, 654, 321, 111)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t038 ($a, $b = $a."x") { "$a/$b" }
is prototype(\&t038), undef;
is eval("t038()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t038(0)"), "0/0x";
is eval("t038(456)"), "456/456x";
is eval("t038(456, 789)"), "456/789";
is eval("t038(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

eval "#line 8 foo\nsub t030 (\$a = 222, \$b) { }";
is $@, "Mandatory parameter follows optional parameter at foo line 8\.\n";

eval "#line 8 foo\nsub t031 (\$a = 222, \$b = 333, \$c, \$d) { }";
is $@, "Mandatory parameter follows optional parameter at foo line 8\.\n";

sub t034 (@abc) { join("/", @abc).";".scalar(@abc) }
is prototype(\&t034), undef;
is eval("t034()"), ";0";
is eval("t034(0)"), "0;1";
is eval("t034(456)"), "456;1";
is eval("t034(456, 789)"), "456/789;2";
is eval("t034(456, 789, 987)"), "456/789/987;3";
is eval("t034(456, 789, 987, 654)"), "456/789/987/654;4";
is eval("t034(456, 789, 987, 654, 321)"), "456/789/987/654/321;5";
is eval("t034(456, 789, 987, 654, 321, 111)"), "456/789/987/654/321/111;6";
is $a, 123;

eval "#line 8 foo\nsub t136 (\@abc = 222) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t137 (\@abc =) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

sub t035 (@) { $a }
is prototype(\&t035), undef;
is eval("t035()"), 123;
is eval("t035(0)"), 123;
is eval("t035(456)"), 123;
is eval("t035(456, 789)"), 123;
is eval("t035(456, 789, 987)"), 123;
is eval("t035(456, 789, 987, 654)"), 123;
is eval("t035(456, 789, 987, 654, 321)"), 123;
is eval("t035(456, 789, 987, 654, 321, 111)"), 123;
is $a, 123;

eval "#line 8 foo\nsub t138 (\@ = 222) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t139 (\@ =) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

sub t039 (%abc) { join("/", map { $_."=".$abc{$_} } sort keys %abc) }
is prototype(\&t039), undef;
is eval("t039()"), "";
is eval("t039(0)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t039(456)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t039(456, 789)"), "456=789";
is eval("t039(456, 789, 987)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t039(456, 789, 987, 654)"), "456=789/987=654";
is eval("t039(456, 789, 987, 654, 321)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t039(456, 789, 987, 654, 321, 111)"), "321=111/456=789/987=654";
is $a, 123;

eval "#line 8 foo\nsub t140 (\%abc = 222) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t141 (\%abc =) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

sub t040 (%) { $a }
is prototype(\&t040), undef;
is eval("t040()"), 123;
is eval("t040(0)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t040(456)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t040(456, 789)"), 123;
is eval("t040(456, 789, 987)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t040(456, 789, 987, 654)"), 123;
is eval("t040(456, 789, 987, 654, 321)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t040(456, 789, 987, 654, 321, 111)"), 123;
is $a, 123;

eval "#line 8 foo\nsub t142 (\% = 222) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t143 (\% =) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

sub t041 ($a, @b) { $a.";".join("/", @b) }
is prototype(\&t041), undef;
is eval("t041()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t041(0)"), "0;";
is eval("t041(456)"), "456;";
is eval("t041(456, 789)"), "456;789";
is eval("t041(456, 789, 987)"), "456;789/987";
is eval("t041(456, 789, 987, 654)"), "456;789/987/654";
is eval("t041(456, 789, 987, 654, 321)"), "456;789/987/654/321";
is eval("t041(456, 789, 987, 654, 321, 111)"), "456;789/987/654/321/111";
is $a, 123;

sub t042 ($a, @) { $a.";" }
is prototype(\&t042), undef;
is eval("t042()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t042(0)"), "0;";
is eval("t042(456)"), "456;";
is eval("t042(456, 789)"), "456;";
is eval("t042(456, 789, 987)"), "456;";
is eval("t042(456, 789, 987, 654)"), "456;";
is eval("t042(456, 789, 987, 654, 321)"), "456;";
is eval("t042(456, 789, 987, 654, 321, 111)"), "456;";
is $a, 123;

sub t043 ($, @b) { $a.";".join("/", @b) }
is prototype(\&t043), undef;
is eval("t043()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t043(0)"), "123;";
is eval("t043(456)"), "123;";
is eval("t043(456, 789)"), "123;789";
is eval("t043(456, 789, 987)"), "123;789/987";
is eval("t043(456, 789, 987, 654)"), "123;789/987/654";
is eval("t043(456, 789, 987, 654, 321)"), "123;789/987/654/321";
is eval("t043(456, 789, 987, 654, 321, 111)"), "123;789/987/654/321/111";
is $a, 123;

sub t044 ($, @) { $a.";" }
is prototype(\&t044), undef;
is eval("t044()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t044(0)"), "123;";
is eval("t044(456)"), "123;";
is eval("t044(456, 789)"), "123;";
is eval("t044(456, 789, 987)"), "123;";
is eval("t044(456, 789, 987, 654)"), "123;";
is eval("t044(456, 789, 987, 654, 321)"), "123;";
is eval("t044(456, 789, 987, 654, 321, 111)"), "123;";
is $a, 123;

sub t049 ($a, %b) { $a.";".join("/", map { $_."=".$b{$_} } sort keys %b) }
is prototype(\&t049), undef;
is eval("t049()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t049(222)"), "222;";
is eval("t049(222, 456)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t049(222, 456, 789)"), "222;456=789";
is eval("t049(222, 456, 789, 987)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t049(222, 456, 789, 987, 654)"), "222;456=789/987=654";
is eval("t049(222, 456, 789, 987, 654, 321)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t049(222, 456, 789, 987, 654, 321, 111)"),
    "222;321=111/456=789/987=654";
is $a, 123;

sub t051 ($a, $b, $c, @d) { "$a;$b;$c;".join("/", @d).";".scalar(@d) }
is prototype(\&t051), undef;
is eval("t051()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t051(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t051(456, 789)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t051(456, 789, 987)"), "456;789;987;;0";
is eval("t051(456, 789, 987, 654)"), "456;789;987;654;1";
is eval("t051(456, 789, 987, 654, 321)"), "456;789;987;654/321;2";
is eval("t051(456, 789, 987, 654, 321, 111)"), "456;789;987;654/321/111;3";
is $a, 123;

sub t052 ($a, $b, %c) { "$a;$b;".join("/", map { $_."=".$c{$_} } sort keys %c) }
is prototype(\&t052), undef;
is eval("t052()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t052(222)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t052(222, 333)"), "222;333;";
is eval("t052(222, 333, 456)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t052(222, 333, 456, 789)"), "222;333;456=789";
is eval("t052(222, 333, 456, 789, 987)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t052(222, 333, 456, 789, 987, 654)"), "222;333;456=789/987=654";
is eval("t052(222, 333, 456, 789, 987, 654, 321)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t052(222, 333, 456, 789, 987, 654, 321, 111)"),
    "222;333;321=111/456=789/987=654";
is $a, 123;

sub t053 ($a, $b, $c, %d) {
    "$a;$b;$c;".join("/", map { $_."=".$d{$_} } sort keys %d)
}
is prototype(\&t053), undef;
is eval("t053()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t053(222)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t053(222, 333)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t053(222, 333, 444)"), "222;333;444;";
is eval("t053(222, 333, 444, 456)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t053(222, 333, 444, 456, 789)"), "222;333;444;456=789";
is eval("t053(222, 333, 444, 456, 789, 987)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t053(222, 333, 444, 456, 789, 987, 654)"),
    "222;333;444;456=789/987=654";
is eval("t053(222, 333, 444, 456, 789, 987, 654, 321)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t053(222, 333, 444, 456, 789, 987, 654, 321, 111)"),
    "222;333;444;321=111/456=789/987=654";
is $a, 123;

sub t048 ($a = 222, @b) { $a.";".join("/", @b).";".scalar(@b) }
is prototype(\&t048), undef;
is eval("t048()"), "222;;0";
is eval("t048(0)"), "0;;0";
is eval("t048(456)"), "456;;0";
is eval("t048(456, 789)"), "456;789;1";
is eval("t048(456, 789, 987)"), "456;789/987;2";
is eval("t048(456, 789, 987, 654)"), "456;789/987/654;3";
is eval("t048(456, 789, 987, 654, 321)"), "456;789/987/654/321;4";
is eval("t048(456, 789, 987, 654, 321, 111)"), "456;789/987/654/321/111;5";
is $a, 123;

sub t054 ($a = 222, $b = 333, @c) { "$a;$b;".join("/", @c).";".scalar(@c) }
is prototype(\&t054), undef;
is eval("t054()"), "222;333;;0";
is eval("t054(456)"), "456;333;;0";
is eval("t054(456, 789)"), "456;789;;0";
is eval("t054(456, 789, 987)"), "456;789;987;1";
is eval("t054(456, 789, 987, 654)"), "456;789;987/654;2";
is eval("t054(456, 789, 987, 654, 321)"), "456;789;987/654/321;3";
is eval("t054(456, 789, 987, 654, 321, 111)"), "456;789;987/654/321/111;4";
is $a, 123;

sub t055 ($a = 222, $b = 333, $c = 444, @d) {
    "$a;$b;$c;".join("/", @d).";".scalar(@d)
}
is prototype(\&t055), undef;
is eval("t055()"), "222;333;444;;0";
is eval("t055(456)"), "456;333;444;;0";
is eval("t055(456, 789)"), "456;789;444;;0";
is eval("t055(456, 789, 987)"), "456;789;987;;0";
is eval("t055(456, 789, 987, 654)"), "456;789;987;654;1";
is eval("t055(456, 789, 987, 654, 321)"), "456;789;987;654/321;2";
is eval("t055(456, 789, 987, 654, 321, 111)"), "456;789;987;654/321/111;3";
is $a, 123;

sub t050 ($a = 211, %b) { $a.";".join("/", map { $_."=".$b{$_} } sort keys %b) }
is prototype(\&t050), undef;
is eval("t050()"), "211;";
is eval("t050(222)"), "222;";
is eval("t050(222, 456)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t050(222, 456, 789)"), "222;456=789";
is eval("t050(222, 456, 789, 987)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t050(222, 456, 789, 987, 654)"), "222;456=789/987=654";
is eval("t050(222, 456, 789, 987, 654, 321)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t050(222, 456, 789, 987, 654, 321, 111)"),
    "222;321=111/456=789/987=654";
is $a, 123;

sub t056 ($a = 211, $b = 311, %c) {
    "$a;$b;".join("/", map { $_."=".$c{$_} } sort keys %c)
}
is prototype(\&t056), undef;
is eval("t056()"), "211;311;";
is eval("t056(222)"), "222;311;";
is eval("t056(222, 333)"), "222;333;";
is eval("t056(222, 333, 456)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t056(222, 333, 456, 789)"), "222;333;456=789";
is eval("t056(222, 333, 456, 789, 987)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t056(222, 333, 456, 789, 987, 654)"), "222;333;456=789/987=654";
is eval("t056(222, 333, 456, 789, 987, 654, 321)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t056(222, 333, 456, 789, 987, 654, 321, 111)"),
    "222;333;321=111/456=789/987=654";
is $a, 123;

sub t057 ($a = 211, $b = 311, $c = 411, %d) {
    "$a;$b;$c;".join("/", map { $_."=".$d{$_} } sort keys %d)
}
is prototype(\&t057), undef;
is eval("t057()"), "211;311;411;";
is eval("t057(222)"), "222;311;411;";
is eval("t057(222, 333)"), "222;333;411;";
is eval("t057(222, 333, 444)"), "222;333;444;";
is eval("t057(222, 333, 444, 456)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t057(222, 333, 444, 456, 789)"), "222;333;444;456=789";
is eval("t057(222, 333, 444, 456, 789, 987)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t057(222, 333, 444, 456, 789, 987, 654)"),
    "222;333;444;456=789/987=654";
is eval("t057(222, 333, 444, 456, 789, 987, 654, 321)"), undef;
like $@, qr#\AOdd name/value argument for subroutine at#;
is eval("t057(222, 333, 444, 456, 789, 987, 654, 321, 111)"),
    "222;333;444;321=111/456=789/987=654";
is $a, 123;

sub t058 ($a, $b = 333, @c) { "$a;$b;".join("/", @c).";".scalar(@c) }
is prototype(\&t058), undef;
is eval("t058()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t058(456)"), "456;333;;0";
is eval("t058(456, 789)"), "456;789;;0";
is eval("t058(456, 789, 987)"), "456;789;987;1";
is eval("t058(456, 789, 987, 654)"), "456;789;987/654;2";
is eval("t058(456, 789, 987, 654, 321)"), "456;789;987/654/321;3";
is eval("t058(456, 789, 987, 654, 321, 111)"), "456;789;987/654/321/111;4";
is $a, 123;

eval "#line 8 foo\nsub t059 (\@a, \$b) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t060 (\@a, \$b = 222) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t061 (\@a, \@b) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t062 (\@a, \%b) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t063 (\@, \$b) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t064 (\@, \$b = 222) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t065 (\@, \@b) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t066 (\@, \%b) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t067 (\@a, \$) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t068 (\@a, \$ = 222) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t069 (\@a, \@) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t070 (\@a, \%) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t071 (\@, \$) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t072 (\@, \$ = 222) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t073 (\@, \@) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t074 (\@, \%) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t075 (\%a, \$b) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t076 (\%, \$b) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t077 (\$a, \@b, \$c) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t078 (\$a, \%b, \$c) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

eval "#line 8 foo\nsub t079 (\$a, \@b, \$c, \$d) { }";
is $@, "Slurpy parameter not last at foo line 8\.\n";

sub t080 ($a,,, $b) { $a.$b }
is prototype(\&t080), undef;
is eval("t080()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t080(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t080(456, 789)"), "456789";
is eval("t080(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t080(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t081 ($a, $b,,) { $a.$b }
is prototype(\&t081), undef;
is eval("t081()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t081(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t081(456, 789)"), "456789";
is eval("t081(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t081(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

eval "#line 8 foo\nsub t082 (, \$a) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t083 (,) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

sub t084($a,$b){ $a.$b }
is prototype(\&t084), undef;
is eval("t084()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t084(456)"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t084(456, 789)"), "456789";
is eval("t084(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t084(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t085
    (
    $
    a
    ,
    ,
    $
    b
    =
    333
    ,
    ,
    )
    { $a.$b }
is prototype(\&t085), undef;
is eval("t085()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t085(456)"), "456333";
is eval("t085(456, 789)"), "456789";
is eval("t085(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t085(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t086
    ( #foo)))
    $ #foo)))
    a #foo)))
    , #foo)))
    , #foo)))
    $ #foo)))
    b #foo)))
    = #foo)))
    333 #foo)))
    , #foo)))
    , #foo)))
    ) #foo)))
    { $a.$b }
is prototype(\&t086), undef;
is eval("t086()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t086(456)"), "456333";
is eval("t086(456, 789)"), "456789";
is eval("t086(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t086(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t087
    (#foo)))
    $ #foo)))
    a#foo)))
    ,#foo)))
    ,#foo)))
    $ #foo)))
    b#foo)))
    =#foo)))
    333#foo)))
    ,#foo)))
    ,#foo)))
    )#foo)))
    { $a.$b }
is prototype(\&t087), undef;
is eval("t087()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t087(456)"), "456333";
is eval("t087(456, 789)"), "456789";
is eval("t087(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t087(456, 789, 987, 654)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

eval "#line 8 foo\nsub t088 (\$ #foo\na) { }";
is $@, "";

eval "#line 8 foo\nsub t089 (\$#foo\na) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t090 (\@ #foo\na) { }";
is $@, "";

eval "#line 8 foo\nsub t091 (\@#foo\na) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t092 (\% #foo\na) { }";
is $@, "";

eval "#line 8 foo\nsub t093 (\%#foo\na) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t094 (123) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t095 (\$a, 123) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t096 (\$a 123) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t097 (\$a { }) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t098 (\$a; \$b) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

eval "#line 8 foo\nsub t099 (\$\$) { }";
like $@, qr/\AParse error at foo line 8\.\n/;

no warnings "experimental::lexical_topic";
sub t100 ($_) { "$::_/$_" }
is prototype(\&t100), undef;
$_ = "___";
is eval("t100()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
$_ = "___";
is eval("t100(0)"), "___/0";
$_ = "___";
is eval("t100(456)"), "___/456";
$_ = "___";
is eval("t100(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
$_ = "___";
is eval("t100(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

eval "#line 8 foo\nsub t101 (\@_) { }";
like $@, qr/\ACan't use global \@_ in "my" at foo line 8/;

eval "#line 8 foo\nsub t102 (\%_) { }";
like $@, qr/\ACan't use global \%_ in "my" at foo line 8/;

my $t103 = sub ($a) { $a || "z" };
is prototype($t103), undef;
is eval("\$t103->()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("\$t103->(0)"), "z";
is eval("\$t103->(456)"), 456;
is eval("\$t103->(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("\$t103->(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

my $t118 = sub :prototype($) ($a) { $a || "z" };
is prototype($t118), "\$";
is eval("\$t118->()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("\$t118->(0)"), "z";
is eval("\$t118->(456)"), 456;
is eval("\$t118->(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("\$t118->(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t033 ($a = sub ($a) { $a."z" }) { $a->("a")."y" }
is prototype(\&t033), undef;
is eval("t033()"), "azy";
is eval("t033(sub { \"x\".\$_[0].\"x\" })"), "xaxy";
is eval("t033(sub { \"x\".\$_[0].\"x\" }, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t133 ($a = sub ($a = 222) { $a."z" }) { $a->()."/".$a->("a") }
is prototype(\&t133), undef;
is eval("t133()"), "222z/az";
is eval("t133(sub { \"x\".(\$_[0] // \"u\").\"x\" })"), "xux/xax";
is eval("t133(sub { \"x\".(\$_[0] // \"u\").\"x\" }, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t134 ($a = sub ($a, $t = sub { $_[0]."p" }) { $t->($a)."z" }) {
    $a->("a")."/".$a->("b", sub { $_[0]."q" } )
}
is prototype(\&t134), undef;
is eval("t134()"), "apz/bqz";
is eval("t134(sub { \"x\".(\$_[1] // sub{\$_[0]})->(\$_[0]).\"x\" })"),
    "xax/xbqx";
is eval("t134(sub { \"x\".(\$_[1] // sub{\$_[0]})->(\$_[0]).\"x\" }, 789)"),
    undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t135 ($a = sub ($a, $t = sub ($p) { $p."p" }) { $t->($a)."z" }) {
    $a->("a")."/".$a->("b", sub { $_[0]."q" } )
}
is prototype(\&t135), undef;
is eval("t135()"), "apz/bqz";
is eval("t135(sub { \"x\".(\$_[1] // sub{\$_[0]})->(\$_[0]).\"x\" })"),
    "xax/xbqx";
is eval("t135(sub { \"x\".(\$_[1] // sub{\$_[0]})->(\$_[0]).\"x\" }, 789)"),
    undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t132 (
    $a = sub ($a, $t = sub ($p = 222) { $p."p" }) { $t->($a)."z".$t->() },
) {
    $a->("a")."/".$a->("b", sub { ($_[0] // "u")."q" } )
}
is prototype(\&t132), undef;
is eval("t132()"), "apz222p/bqzuq";
is eval("t132(sub { \"x\".(\$_[1] // sub{\$_[0]})->(\$_[0]).\"x\" })"),
    "xax/xbqx";
is eval("t132(sub { \"x\".(\$_[1] // sub{\$_[0]})->(\$_[0]).\"x\" }, 789)"),
    undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t104 :method ($a) { $a || "z" }
is prototype(\&t104), undef;
is eval("t104()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t104(0)"), "z";
is eval("t104(456)"), 456;
is eval("t104(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t104(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

sub t105 :prototype($) ($a) { $a || "z" }
is prototype(\&t105), "\$";
is eval("t105()"), undef;
like $@, qr/\ANot enough arguments for main::t105 /;
is eval("t105(0)"), "z";
is eval("t105(456)"), 456;
is eval("t105(456, 789)"), undef;
like $@, qr/\AToo many arguments for main::t105 at/;
is eval("t105(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for main::t105 at/;
is $a, 123;

sub t106 :prototype(@) ($a) { $a || "z" }
is prototype(\&t106), "\@";
is eval("t106()"), undef;
like $@, qr/\AToo few arguments for subroutine at/;
is eval("t106(0)"), "z";
is eval("t106(456)"), 456;
is eval("t106(456, 789)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is eval("t106(456, 789, 987)"), undef;
like $@, qr/\AToo many arguments for subroutine at/;
is $a, 123;

eval "#line 8 foo\nsub t107 (\$a) :method { }";
isnt $@, "";

eval "#line 8 foo\nsub t108 (\$a) :prototype(\$) { }";
isnt $@, "";

sub t109 { }
is prototype(\&t109), undef;
is scalar(@{[ t109() ]}), 0;
is scalar(t109()), undef;

sub t110 () { }
is prototype(\&t110), undef;
is scalar(@{[ t110() ]}), 0;
is scalar(t110()), undef;

sub t111 ($a) { }
is prototype(\&t111), undef;
is scalar(@{[ t111(222) ]}), 0;
is scalar(t111(222)), undef;

sub t112 ($) { }
is prototype(\&t112), undef;
is scalar(@{[ t112(222) ]}), 0;
is scalar(t112(222)), undef;

sub t114 ($a = undef) { }
is prototype(\&t114), undef;
is scalar(@{[ t114() ]}), 0;
is scalar(t114()), undef;
is scalar(@{[ t114(333) ]}), 0;
is scalar(t114(333)), undef;

sub t113 ($a = 222) { }
is prototype(\&t113), undef;
is scalar(@{[ t113() ]}), 0;
is scalar(t113()), undef;
is scalar(@{[ t113(333) ]}), 0;
is scalar(t113(333)), undef;

sub t115 ($a = do { $z++; 222 }) { }
is prototype(\&t115), undef;
$z = 0;
is scalar(@{[ t115() ]}), 0;
is $z, 1;
is scalar(t115()), undef;
is $z, 2;
is scalar(@{[ t115(333) ]}), 0;
is scalar(t115(333)), undef;
is $z, 2;

sub t116 (@a) { }
is prototype(\&t116), undef;
is scalar(@{[ t116() ]}), 0;
is scalar(t116()), undef;
is scalar(@{[ t116(333) ]}), 0;
is scalar(t116(333)), undef;

sub t117 (%a) { }
is prototype(\&t117), undef;
is scalar(@{[ t117() ]}), 0;
is scalar(t117()), undef;
is scalar(@{[ t117(333, 444) ]}), 0;
is scalar(t117(333, 444)), undef;

1;
