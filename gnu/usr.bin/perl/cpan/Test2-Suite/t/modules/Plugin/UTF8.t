use strict;
use warnings;
# HARNESS-NO-FORMATTER

# Store the default STDOUT and STDERR IO layers for later testing.
# This must happen before we load anything else.
use PerlIO ();
my %Layers;

sub get_layers {
    my $fh = shift;
    return { map {$_ => 1} PerlIO::get_layers($fh) };
}

BEGIN {
    $Layers{STDERR} = get_layers(*STDERR);
    $Layers{STDOUT} = get_layers(*STDOUT);
}

use Test2::Plugin::UTF8;
use Test2::Tools::Basic;
use Test2::Tools::Compare;
use Test2::API qw(test2_stack);

note "pragma"; {
    ok(utf8::is_utf8("癸"), "utf8 pragma is on");
}

note "io_layers"; {
    is get_layers(*STDOUT), $Layers{STDOUT}, "STDOUT encoding is untouched";
    is get_layers(*STDERR), $Layers{STDERR}, "STDERR encoding is untouched";
}

note "format_handles"; {
    my $format = test2_stack()->top->format;
    my $handles = $format->handles or last;
    for my $hn (0 .. @$handles) {
        my $h = $handles->[$hn] || next;
        my $layers = get_layers($h);
        ok($layers->{utf8}, "utf8 is on for formatter handle $hn");
    }
}

done_testing;
