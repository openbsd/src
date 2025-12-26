use Test2::Bundle::Extended -target => 'Test2::Util::Table::Cell';

subtest sanitization => sub {
    my $unsanitary = <<"    EOT";
This string
has vertical space
including          　‌﻿\N{U+000B}unicode stuff
and some non-whitespace ones: 婧 ʶ ๖
    EOT
    my $sanitary = 'This string\nhas vertical space\nincluding\N{U+A0}\N{U+1680}\N{U+2000}\N{U+2001}\N{U+2002}\N{U+2003}\N{U+2004}\N{U+2008}\N{U+2028}\N{U+2029}\N{U+3000}\N{U+200C}\N{U+FEFF}\N{U+B}unicode stuff\nand some non-whitespace ones: 婧 ʶ ๖\n';
    $sanitary =~ s/\\n/\\n\n/g;

    local *show_char = sub { Test2::Util::Table::Cell->show_char(@_) };

    # Common control characters
    is(show_char("\a"), '\a',    "translated bell");
    is(show_char("\b"), '\b',    "translated backspace");
    is(show_char("\e"), '\e',    "translated escape");
    is(show_char("\f"), '\f',    "translated formfeed");
    is(show_char("\n"), "\\n\n", "translated newline");
    is(show_char("\r"), '\r',    "translated return");
    is(show_char("\t"), '\t',    "translated tab");
    is(show_char(" "),  ' ',     "plain space is not translated");

    # unicodes
    is(show_char("婧"), '\N{U+5A67}', "translated unicode 婧 (U+5A67)");
    is(show_char("ʶ"),  '\N{U+2B6}',  "translated unicode ʶ (U+2B6)");
    is(show_char("߃"),  '\N{U+7C3}',  "translated unicode ߃ (U+7C3)");
    is(show_char("๖"),  '\N{U+E56}',  "translated unicode ๖ (U+E56)");

    my $cell = CLASS->new(value => $unsanitary);
    $cell->sanitize;

    is($cell->value, $sanitary, "Sanitized string");
};

done_testing;
