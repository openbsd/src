#!./perl

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 4;

require_ok("B::Concise");

$out = runperl(switches => ["-MO=Concise"], prog => '$a', stderr => 1);

# If either of the next two tests fail, it probably means you need to
# fix the section labeled 'fragile kludge' in Concise.pm

($op_base) = ($out =~ /^(\d+)\s*<0>\s*enter/m);

is($op_base, 1, "Smallest OP sequence number");

($op_base_p1, $cop_base) = ($out =~ /^(\d+)\s*<;>\s*nextstate\(main (\d+) /m);

is($op_base_p1, 2, "Second-smallest OP sequence number");

is($cop_base, 1, "Smallest COP sequence number");
