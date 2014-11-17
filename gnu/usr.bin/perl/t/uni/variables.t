#!./perl

# Checks if the parser behaves correctly in edge case
# (including weird syntax errors)

BEGIN {
    require './test.pl';
}

use 5.016;
use utf8;
use open qw( :utf8 :std );
no warnings qw(misc reserved);

plan (tests => 65880);

# ${single:colon} should not be valid syntax
{
    no strict;

    local $@;
    eval "\${\x{30cd}single:\x{30cd}colon} = 1";
    like($@,
         qr/syntax error .* near "\x{30cd}single:/,
         '${\x{30cd}single:\x{30cd}colon} should not be valid syntax'
        );

    local $@;
    no utf8;
    evalbytes '${single:colon} = 1';
    like($@,
         qr/syntax error .* near "single:/,
         '...same with ${single:colon}'
        );
}

# ${yadda'etc} and ${yadda::etc} should both work under strict
{
    local $@;
    eval q<use strict; ${flark::fleem}>;
    is($@, '', q<${package::var} works>);

    local $@;
    eval q<use strict; ${fleem'flark}>;
    is($@, '', q<...as does ${package'var}>);
}

# The first character in ${...} should respect the rules
{
   local $@;
   use utf8;
   eval '${☭asd} = 1';
   like($@, qr/\QUnrecognized character/, q(the first character in ${...} isn't special))
}

# Checking that at least some of the special variables work
for my $v (qw( ^V ; < > ( ) {^GLOBAL_PHASE} ^W _ 1 4 0 [ ] ! @ / \ = )) {
  SKIP: {
    skip_if_miniperl('No $[ under miniperl', 2) if $v eq '[';
    local $@;
    evalbytes "\$$v;";
    is $@, '', "No syntax error for \$$v";

    local $@;
    eval "use utf8; \$$v;";
    is $@, '', "No syntax error for \$$v under use utf8";
  }
}

# Checking if the Latin-1 range behaves as expected, and that the behavior is the
# same whenever under strict or not.
for ( 0x80..0xff ) {
    no warnings 'closure';
    my $chr = chr;
    my $esc = sprintf("%X", ord $chr);
    utf8::downgrade($chr);
    if ($chr !~ /\p{XIDS}/u) {
        is evalbytes "no strict; \$$chr = 10",
            10,
            sprintf("\\x%02x, part of the latin-1 range, is legal as a length-1 variable", $_);

        utf8::upgrade($chr);
        local $@;
        eval "no strict; use utf8; \$$chr = 1";
        like $@,
            qr/\QUnrecognized character \x{\E\L$esc/,
            sprintf("..but is illegal as a length-1 variable under use utf8", $_);
    }
    else {
        {
            no utf8;
            local $@;
            evalbytes "no strict; \$$chr = 1";
            is($@, '', sprintf("\\x%02x, =~ \\p{XIDS}, latin-1, no utf8, no strict, is a valid length-1 variable", $_));

            local $@;
            evalbytes "use strict; \$$chr = 1";
            is($@,
                '',
                sprintf("\\x%02x under no utf8 does not have to be required under strict, even though it matches XIDS", $_)
            );

            local $@;
            evalbytes "\$a$chr = 1";
            like($@,
                qr/Unrecognized character /,
                sprintf("...but under no utf8, it's not allowed in two-or-more character variables")
            );

            local $@;
            evalbytes "\$a$chr = 1";
            like($@,
                qr/Unrecognized character /,
                sprintf("...but under no utf8, it's not allowed in two-or-more character variables")
            );
        }
        {
            use utf8;
            my $u = $chr;
            utf8::upgrade($u);
            local $@;
            eval "no strict; \$$u = 1";
            is($@, '', sprintf("\\x%02x, =~ \\p{XIDS}, UTF-8, use utf8, no strict, is a valid length-1 variable", $_));

            local $@;
            eval "use strict; \$$u = 1";
            like($@,
                qr/Global symbol "\$$u" requires explicit package name/,
                sprintf("\\x%02x under utf8 has to be required under strict", $_)
            );
        }
    }
}

{
    use utf8;
    my $ret = eval "my \$c\x{327} = 100; \$c\x{327}"; # c + cedilla
    is($@, '', "ASCII character + combining character works as a variable name");
    is($ret, 100, "...and returns the correct value");
}

# From Tom Christiansen's 'highly illegal variable names are now accidentally legal' mail
for my $chr (
      "\N{EM DASH}", "\x{F8FF}", "\N{POUND SIGN}", "\N{SOFT HYPHEN}",
      "\N{THIN SPACE}", "\x{11_1111}", "\x{DC00}", "\N{COMBINING DIAERESIS}",
      "\N{COMBINING ENCLOSING CIRCLE BACKSLASH}",
   )
{
   no warnings 'non_unicode';
   my $esc = sprintf("%x", ord $chr);
   local $@;
   eval "\$$chr = 1; \$$chr";
   like($@,
        qr/\QUnrecognized character \x{$esc};/,
        "\\x{$esc} is illegal for a length-one identifier"
       );
}

for my $i (0x100..0xffff) {
   my $chr = chr($i);
   my $esc = sprintf("%x", $i);
   local $@;
   eval "my \$$chr = q<test>; \$$chr;";
   if ( $chr =~ /^\p{_Perl_IDStart}$/ ) {
      is($@, '', sprintf("\\x{%04x} is XIDS, works as a length-1 variable", $i));
   }
   else {
      like($@,
           qr/\QUnrecognized character \x{$esc};/,
           "\\x{$esc} isn't XIDS, illegal as a length-1 variable",
          )
   }
}

{
    # Bleadperl v5.17.9-109-g3283393 breaks ZEFRAM/Module-Runtime-0.013.tar.gz
    # https://rt.perl.org/rt3/Public/Bug/Display.html?id=117101
    no strict;

    local $@;
    eval <<'EOP';
    q{$} =~ /(.)/;
    is($$1, $$, q{$$1 parses as ${$1}});

    $doof = "test";
    $test = "Got here";
    $::{+$$} = *doof;

    is( $$$$1, $test, q{$$$$1 parses as ${${${$1}}}} );
EOP
    is($@, '', q{$$1 parses correctly});

    for my $chr ( q{@}, "\N{U+FF10}", "\N{U+0300}" ) {
        my $esc = sprintf("\\x{%x}", ord $chr);
        local $@;
        eval <<"    EOP";
            \$$chr = q{\$};
            \$\$$chr;
    EOP

        like($@,
             qr/syntax error|Unrecognized character/,
             qq{\$\$$esc is a syntax error}
        );
    }
}

{    
    # bleadperl v5.17.9-109-g3283393 breaks JEREMY/File-Signature-1.009.tar.gz
    # https://rt.perl.org/rt3/Ticket/Display.html?id=117145
    local $@;
    my $var = 10;
    eval ' ${  var  }';

    is(
        $@,
        '',
        '${  var  } works under strict'
    );

    {
        no strict;
        # Silence the deprecation warning for literal controls
        no warnings 'deprecated';

        for my $var ( '$', "\7LOBAL_PHASE", "^GLOBAL_PHASE", "^V" ) {
            eval "\${ $var}";
            is($@, '', "\${ $var} works" );
            eval "\${$var }";
            is($@, '', "\${$var } works" );
            eval "\${ $var }";
            is($@, '', "\${ $var } works" );
        }
    }
}

{
    is(
        "".eval "*{\nOIN}",
        "*main::OIN",
        "Newlines at the start of an identifier should be skipped over"
    );
    
    
    is(
        "".eval "*{^JOIN}",
        "*main::\nOIN",
        "...but \$^J is still legal"
    );
    
    no warnings 'deprecated';
    my $ret = eval "\${\cT\n}";
    is($@, "", 'No errors from using ${\n\cT\n}');
    is($ret, $^T, "...and we got the right value");
}

{
    # Originally from t/base/lex.t, moved here since we can't
    # turn deprecation warnings off in that file.
    no strict;
    no warnings 'deprecated';
    
    my $CX  = "\cX";
    $ {$CX} = 17;
    
    # Does the syntax where we use the literal control character still work?
    is(
       eval "\$ {\cX}",
       17,
       "Literal control character variables work"
    );

    eval "\$\cQ = 24";                 # Literal control character
    is($@, "", "...and they can be assigned to without error");
    is(${"\cQ"}, 24, "...and the assignment works");
    is($^Q, 24, "...even if we access the variable through the caret name");
    is(\${"\cQ"}, \$^Q, '\${\cQ} == \$^Q');
}

{
    # Prior to 5.19.4, the following changed behavior depending
    # on the presence of the newline after '@{'.
    sub foo (&) { [1] }
    my %foo = (a=>2);
    my $ret = @{ foo { "a" } };
    is($ret, $foo{a}, '@{ foo { "a" } } is parsed as @foo{a}');
    
    $ret = @{
            foo { "a" }
        };
    is($ret, $foo{a}, '@{\nfoo { "a" } } is still parsed as @foo{a}');

}
