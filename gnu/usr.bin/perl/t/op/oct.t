#!./perl

# tests 51 onwards aren't all warnings clean. (intentionally)

print "1..71\n";

my $test = 1;

sub test ($$$) {
  my ($act, $string, $value) = @_;
  my $result;
  if ($act eq 'oct') {
    $result = oct $string;
  } elsif ($act eq 'hex') {
    $result = hex $string;
  } else {
    die "Unknown action 'act'";
  }
  if ($value == $result) {
    if ($^O eq 'VMS' && length $string > 256) {
      $string = '';
    } else {
      $string = "\"$string\"";
    }
    print "ok $test # $act $string\n";
  } else {
    my ($valstr, $resstr);
    if ($act eq 'hex' or $string =~ /x/) {
      $valstr = sprintf "0x%X", $value;
      $resstr = sprintf "0x%X", $result;
    } elsif ($string =~ /b/) {
      $valstr = sprintf "0b%b", $value;
      $resstr = sprintf "0b%b", $result;
    } else {
      $valstr = sprintf "0%o", $value;
      $resstr = sprintf "0%o", $result;
    }
    print "not ok $test # $act \"$string\" gives \"$result\" ($resstr), not $value ($valstr)\n";
  }
  $test++;
}

test ('oct', '0b1_0101', 0b101_01);
test ('oct', '0b10_101', 0_2_5);
test ('oct', '0b101_01', 2_1);
test ('oct', '0b1010_1', 0x1_5);

test ('oct', 'b1_0101', 0b10101);
test ('oct', 'b10_101', 025);
test ('oct', 'b101_01', 21);
test ('oct', 'b1010_1', 0x15);

test ('oct', '01_234', 0b10_1001_1100);
test ('oct', '012_34', 01234);
test ('oct', '0123_4', 668);
test ('oct', '01234', 0x29c);

test ('oct', '0x1_234', 0b10010_00110100);
test ('oct', '0x12_34', 01_1064);
test ('oct', '0x123_4', 4660);
test ('oct', '0x1234', 0x12_34);

test ('oct', 'x1_234', 0b100100011010_0);
test ('oct', 'x12_34', 0_11064);
test ('oct', 'x123_4', 4660);
test ('oct', 'x1234', 0x_1234);

test ('hex', '01_234', 0b_1001000110100);
test ('hex', '012_34', 011064);
test ('hex', '0123_4', 4660);
test ('hex', '01234_', 0x1234);

test ('hex', '0x_1234', 0b1001000110100);
test ('hex', '0x1_234', 011064);
test ('hex', '0x12_34', 4660);
test ('hex', '0x1234_', 0x1234);

test ('hex', 'x_1234', 0b1001000110100);
test ('hex', 'x12_34', 011064);
test ('hex', 'x123_4', 4660);
test ('hex', 'x1234_', 0x1234);

test ('oct', '0b1111_1111_1111_1111_1111_1111_1111_1111', 4294967295);
test ('oct', '037_777_777_777', 4294967295);
test ('oct', '0xffff_ffff', 4294967295);
test ('hex', '0xff_ff_ff_ff', 4294967295);

$_ = "\0_7_7";
print length eq 5                      ? "ok" : "not ok", " 37\n";
print $_ eq "\0"."_"."7"."_"."7"       ? "ok" : "not ok", " 38\n";
chop, chop, chop, chop;
print $_ eq "\0"                       ? "ok" : "not ok", " 39\n";
if (ord("\t") != 9) {
    # question mark is 111 in 1047, 037, && POSIX-BC
    print "\157_" eq "?_"                  ? "ok" : "not ok", " 40\n";
}
else {
    print "\077_" eq "?_"                  ? "ok" : "not ok", " 40\n";
}

$_ = "\x_7_7";
print length eq 5                      ? "ok" : "not ok", " 41\n";
print $_ eq "\0"."_"."7"."_"."7"       ? "ok" : "not ok", " 42\n";
chop, chop, chop, chop;
print $_ eq "\0"                       ? "ok" : "not ok", " 43\n";
if (ord("\t") != 9) {
    # / is 97 in 1047, 037, && POSIX-BC
    print "\x61_" eq "/_"                  ? "ok" : "not ok", " 44\n";
}
else {
    print "\x2F_" eq "/_"                  ? "ok" : "not ok", " 44\n";
}

$test = 45;
test ('oct', '0b'.(  '0'x10).'1_0101', 0b101_01);
test ('oct', '0b'.( '0'x100).'1_0101', 0b101_01);
test ('oct', '0b'.('0'x1000).'1_0101', 0b101_01);

test ('hex', (  '0'x10).'01234', 0x1234);
test ('hex', ( '0'x100).'01234', 0x1234);
test ('hex', ('0'x1000).'01234', 0x1234);

# Things that perl 5.6.1 and 5.7.2 did wrong (plus some they got right)
test ('oct', "b00b0101", 0);
test ('oct', "bb0101",	 0);
test ('oct', "0bb0101",	 0);

test ('oct', "0x0x3A",	 0);
test ('oct', "0xx3A",	 0);
test ('oct', "x0x3A",	 0);
test ('oct', "xx3A",	 0);
test ('oct', "0x3A",	 0x3A);
test ('oct', "x3A",	 0x3A);

test ('oct', "0x0x4",	 0);
test ('oct', "0xx4",	 0);
test ('oct', "x0x4",	 0);
test ('oct', "xx4",	 0);
test ('oct', "0x4",	 4);
test ('oct', "x4",	 4);

test ('hex', "0x3A",	 0x3A);
test ('hex', "x3A",	 0x3A);

test ('hex', "0x4",	 4);
test ('hex', "x4",	 4);

eval '$a = oct "10\x{100}"';
print $@ =~ /Wide character/ ? "ok $test\n" : "not ok $test\n"; $test++;

eval '$a = hex "ab\x{100}"';
print $@ =~ /Wide character/ ? "ok $test\n" : "not ok $test\n"; $test++;
