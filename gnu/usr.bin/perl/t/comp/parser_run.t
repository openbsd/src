#!./perl

# Parser tests that want test.pl, eg to use runperl() for tests to show
# reads through invalid pointers.
# Note that this should still be runnable under miniperl.

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc( qw(. ../lib ) );
}

plan(5);

# [perl #130814] can reallocate lineptr while looking ahead for
# "Missing $ on loop variable" diagnostic.
my $result = fresh_perl(
    " foreach m0\n\$" . ("0" x 0x2000),
    { stderr => 1 },
);
is($result . "\n", <<EXPECT);
syntax error at - line 3, near "foreach m0
"
Identifier too long at - line 3.
EXPECT

fresh_perl_is(<<EOS, <<'EXPECT', {}, "linestart before bufptr");
\${ \xB6eeeeeeeeeeee
'x
EOS
Unrecognized character \xB6; marked by <-- HERE after ${ <-- HERE near column 4 at - line 1.
EXPECT

fresh_perl_is(<<'EOS', <<'EXPECTED', {}, "use after free (#131836)");
${sub#xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
EOS
Missing right curly or square bracket at - line 1, at end of line
syntax error at - line 1, at EOF
Execution of - aborted due to compilation errors.
EXPECTED

SKIP:
{
    # [perl #131949] use after free
    # detected by ASAN
    # Win32 cmd.exe can't handle newlines well
    skip("Need POSIXish", 1) if $^O eq "MSWin32";
    my $out = runperl(prog => "\@{ 0\n\n}", stderr => 1, non_portable => 1);
    is($out, "", "check for ASAN use after free");
}

fresh_perl_is('-C-', <<'EXPECTED', {}, "ambiguous unary operator check doesn't crash (#132433)");
Warning: Use of "-C-" without parentheses is ambiguous at - line 1.
syntax error at - line 1, at EOF
Execution of - aborted due to compilation errors.
EXPECTED

__END__
# ex: set ts=8 sts=4 sw=4 et:
