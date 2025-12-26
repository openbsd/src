use Test2::Bundle::Extended -target => 'Test2::Compare::Float';

my $num     = $CLASS->new(input => '22.0', tolerance => .001);
my $neg_num = $CLASS->new(input => -22,    tolerance => .001);
my $untrue  = $CLASS->new(input => 0);
my $pre_num = $CLASS->new(input => '22.0', precision => 3);

isa_ok($num,    $CLASS, 'Test2::Compare::Base');
isa_ok($untrue, $CLASS, 'Test2::Compare::Base');

subtest tolerance => sub {
    is($num->tolerance,    0.001, "got expected tolerance for number");
    is($untrue->tolerance, 1e-08, "got default tolerance for 0");
};

subtest name => sub {
    is($num->name,    '22.0 +/- ' . $num->tolerance, "got expected name for number");
    is($untrue->name, '0 +/- ' . $untrue->tolerance, "got expected name for 0");
    # Note: string length of mantissa varies by perl install, e.g. 1e-08 vs 1e-008

    is($pre_num->name, '22.000', "got expected 3 digits of precision in name for 22.0, precision=5");
    is($CLASS->new(input => '100.123456789012345', precision => 10)->name,
      '100.1234567890',
      'got expected precision in name at precision=10');
    is($CLASS->new(input => '100.123456789012345', precision => 15)->name,
      sprintf('%.*f', 15, '100.123456789012345'),
      'got expected precision in name at precision=15');  # likely not 100.123456789012345!
    is($CLASS->new(input => '100.123456789012345', precision => 20)->name,
      sprintf('%.*f', 20, '100.123456789012345'),
      'got expected precision in name at precision=20');
};

subtest operator => sub {
    is($num->operator(),      '',   "no operator for number + nothing");
    is($num->operator(undef), '',   "no operator for number + undef");
    is($num->operator(1),     '==', "== operator for number + number");

    is($untrue->operator(),      '',   "no operator for 0 + nothing");
    is($untrue->operator(undef), '',   "no operator for 0 + undef");
    is($untrue->operator(1),     '==', "== operator for 0 + number");

    is($pre_num->operator(),      '',   "no operator for precision number + nothing");
    is($pre_num->operator(undef), '',   "no operator for precision number + undef");
    is($pre_num->operator(1),     'eq', "eq operator for precision number + number");
};

subtest verify => sub {
    ok(!$num->verify(exists => 0, got => undef), 'does not verify against DNE');
    ok(!$num->verify(exists => 1, got => {}),    'ref will not verify');
    ok(!$num->verify(exists => 1, got => undef), 'looking for a number, not undef');
    ok(!$num->verify(exists => 1, got => 'x'),   'not looking for a string');
    ok(!$num->verify(exists => 1, got => 1),     'wrong number');
    ok($num->verify(exists => 1, got => 22),     '22.0 == 22');
    ok($num->verify(exists => 1, got => '22.0'), 'exact match with decimal');

    ok(!$untrue->verify(exists => 0, got => undef), 'does not verify against DNE');
    ok(!$untrue->verify(exists => 1, got => {}),    'ref will not verify');
    ok(!$untrue->verify(exists => 1, got => undef), 'undef is not 0 for this test');
    ok(!$untrue->verify(exists => 1, got => 'x'),   'x is not 0');
    ok(!$untrue->verify(exists => 1, got => 1),     '1 is not 0');
    ok(!$untrue->verify(exists => 1, got => ''),    '"" is not 0');
    ok(!$untrue->verify(exists => 1, got => ' '),   '" " is not 0');
    ok($untrue->verify(exists => 1, got => 0),      'got 0');
    ok($untrue->verify(exists => 1, got => '0.0'),  '0.0 == 0');
    ok($untrue->verify(exists => 1, got => '-0.0'), '-0.0 == 0');
};

subtest verify_float_tolerance => sub {
  ok($num->verify(exists => 1, got => "22.0"),     '22.0    == 22 +/- .001');
  ok($num->verify(exists => 1, got => "22.0009"),  '22.0009 == 22 +/- .001');
  ok($num->verify(exists => 1, got => "21.9991"),  '21.9991 == 22 +/- .001');
  ok(!$num->verify(exists => 1, got => "22.0011"), '22.0009 != 22 +/- .001');
  ok(!$num->verify(exists => 1, got => "21.9989"), '21.9989 != 22 +/- .001');
  ok(!$num->verify(exists => 1, got => "23"),      '23      != 22 +/- .001');

  ok($num->verify(exists => 1, got => 22.0),       '22.0    == 22 +/- .001');
  ok($num->verify(exists => 1, got => 22.0009),    '22.0009 == 22 +/- .001');
  ok($num->verify(exists => 1, got => 21.9991),    '21.9991 == 22 +/- .001');
  ok(!$num->verify(exists => 1, got => 22.0011),   '22.0009 != 22 +/- .001');
  ok(!$num->verify(exists => 1, got => 21.9989),   '21.9989 != 22 +/- .001');
  ok(!$num->verify(exists => 1, got => 23),        '23      != 22 +/- .001');

  ok($neg_num->verify(exists => 1, got => -22.0),       '-22.0    == -22 +/- .001');
  ok($neg_num->verify(exists => 1, got => -22.0009),    '-22.0009 == -22 +/- .001');
  ok($neg_num->verify(exists => 1, got => -21.9991),    '-21.9991 == -22 +/- .001');
  ok(!$neg_num->verify(exists => 1, got => -22.0011),   '-22.0009 != -22 +/- .001');
  ok(!$neg_num->verify(exists => 1, got => -21.9989),   '-21.9989 != -22 +/- .001');
  ok(!$neg_num->verify(exists => 1, got => -23),        '-23      != -22 +/- .001');
};
subtest verify_float_precision => sub {
  ok($pre_num->verify(exists => 1, got => "22.0"),     '22.0    == 22.000');
  ok($pre_num->verify(exists => 1, got => "22.0001"),  '22.0001 == 22.000');
  ok($pre_num->verify(exists => 1, got => "21.9999"),  '21.9999 == 22.000');
  ok(!$pre_num->verify(exists => 1, got => "22.0011"), '22.0011 != 22.000');
  ok(!$pre_num->verify(exists => 1, got => "21.9989"), '21.9989 != 22.000');
  ok(!$pre_num->verify(exists => 1, got => "23"),      '23      != 22.000');

  ok($pre_num->verify(exists => 1, got => 22.0),       '22.0    == 22.000');
  ok($pre_num->verify(exists => 1, got => 22.00049),    '22.00049 == 22.000');
  ok(!$pre_num->verify(exists => 1, got => 22.00051),    '22.00051 != 22.000');
  ok($pre_num->verify(exists => 1, got => 21.99951),   '21.99951 == 22.000');
  ok(!$pre_num->verify(exists => 1, got => 22.0009),   '22.0009 != 22.000');
  ok(!$pre_num->verify(exists => 1, got => 21.9989),   '21.9989 != 22.000');
  ok(!$pre_num->verify(exists => 1, got => 23),        '23      != 22.000');

  ok($neg_num->verify(exists => 1, got => -22.0),       '-22.0    == -22.000');
  ok($neg_num->verify(exists => 1, got => -22.0009),    '-22.0009 == -22.000');
  ok($neg_num->verify(exists => 1, got => -21.9991),    '-21.9991 == -22.000');
  ok(!$neg_num->verify(exists => 1, got => -22.0011),   '-22.0009 != -22.000');
  ok(!$neg_num->verify(exists => 1, got => -21.9989),   '-21.9989 != -22.000');
  ok(!$neg_num->verify(exists => 1, got => -23),        '-23      != -22.000');
};

subtest rounding_tolerance => sub {
    my $round_08    = $CLASS->new(input => '60.48');
    my $round_13    = $CLASS->new(input => '60.48', tolerance => 1e-13);
    my $round_14    = $CLASS->new(input => '60.48', tolerance => 1e-14);

    ok($round_08->verify(exists => 1, got => 60.48),       '      60.48 == ' . $round_08->name . " - inside tolerance");
    ok($round_13->verify(exists => 1, got => 60.48),       '      60.48 == ' . $round_13->name . " - inside tolerance");
    ok($round_14->verify(exists => 1, got => 60.48),       '      60.48 == ' . $round_14->name . " - inside tolerance");

    ok($round_08->verify(exists => 1, got => 125 - 64.52), '125 - 64.52 == ' . $round_08->name . " - inside tolerance");
    ok($round_13->verify(exists => 1, got => 125 - 64.52), '125 - 64.52 == ' . $round_13->name . " - inside tolerance");

    todo 'broken on some platforms' => sub {
        ok(!$round_14->verify(exists => 1, got => 125 - 64.52), '125 - 64.52 != ' . $round_14->name . " - outside tolerance");
    };
};

subtest rounding_precision => sub {
    my $round_08    = $CLASS->new(input => '60.48', precision =>  8 );
    my $round_13    = $CLASS->new(input => '60.48', precision => 13);
    my $round_14    = $CLASS->new(input => '60.48', precision => 14);

    ok($round_08->verify(exists => 1, got => 60.48),       '      60.48 == ' . $round_08->name . " - inside precision");
    ok($round_13->verify(exists => 1, got => 60.48),       '      60.48 == ' . $round_13->name . " - inside precision");
    ok($round_14->verify(exists => 1, got => 60.48),       '      60.48 == ' . $round_14->name . " - inside precision");

    ok($round_08->verify(exists => 1, got => 125 - 64.52), '125 - 64.52 == ' . $round_08->name . " - inside precision");
    ok($round_13->verify(exists => 1, got => 125 - 64.52), '125 - 64.52 == ' . $round_13->name . " - inside precision");

    # unlike TOLERANCE, this should work on 32 and 64 bit platforms.
    ok($round_14->verify(exists => 1, got => 125 - 64.52), '125 - 64.52 != ' . $round_14->name . " - outside precision");
};

like(
    dies { $CLASS->new() },
    qr/input must be defined for 'Float' check/,
    "Cannot use undef as a number"
);

like(
    dies { $CLASS->new(input => '') },
    qr/input must be a number for 'Float' check/,
    "Cannot use empty string as a number"
);

like(
    dies { $CLASS->new(input => ' ') },
    qr/input must be a number for 'Float' check/,
    "Cannot use whitespace string as a number"
);

like(
    dies { $CLASS->new(input => 1.234, precision => 5, tolerance => .001) },
    qr/can't set both tolerance and precision/,
    "Cannot use both precision and tolerance"
);
like(
    dies { $CLASS->new(input => 1.234, precision => .05) },
    qr/precision must be an integer/,
    "precision can't be fractional"
);
like(
    dies { $CLASS->new(input => 1.234, precision => -2) },
    qr/precision must be an integer/,
    "precision can't be negative"
);

done_testing;
