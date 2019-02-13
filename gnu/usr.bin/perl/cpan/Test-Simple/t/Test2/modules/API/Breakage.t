use strict;
use warnings;

use Test2::IPC;
use Test2::Tools::Tiny;
use Test2::API::Breakage;
my $CLASS = 'Test2::API::Breakage';

for my $meth (qw/upgrade_suggested upgrade_required known_broken/) {
    my @list = $CLASS->$meth;
    ok(!(@list % 2), "Got even list ($meth)");
    ok(!(grep {!defined($_)} @list), "No undefined items ($meth)");
}

{
    no warnings 'redefine';
    local *Test2::API::Breakage::upgrade_suggested = sub {
        return ('T2Test::UG1' => '1.0', 'T2Test::UG2' => '0.5');
    };

    local *Test2::API::Breakage::upgrade_required = sub {
        return ('T2Test::UR1' => '1.0', 'T2Test::UR2' => '0.5');
    };

    local *Test2::API::Breakage::known_broken = sub {
        return ('T2Test::KB1' => '1.0', 'T2Test::KB2' => '0.5');
    };
    use warnings 'redefine';

    ok(!$CLASS->report, "Nothing to report");
    ok(!$CLASS->report(1), "Still nothing to report");

    {
        local %INC = (
            %INC,
            'T2Test/UG1.pm' => 1,
            'T2Test/UG2.pm' => 1,
            'T2Test/UR1.pm' => 1,
            'T2Test/UR2.pm' => 1,
            'T2Test/KB1.pm' => 1,
            'T2Test/KB2.pm' => 1,
        );
        local $T2Test::UG1::VERSION = '0.9';
        local $T2Test::UG2::VERSION = '0.9';
        local $T2Test::UR1::VERSION = '0.9';
        local $T2Test::UR2::VERSION = '0.9';
        local $T2Test::KB1::VERSION = '0.9';
        local $T2Test::KB2::VERSION = '0.9';

        my @report = $CLASS->report;

        is_deeply(
            [sort @report],
            [
                sort
                " * Module 'T2Test::UG1' is outdated, we recommed updating above 1.0.",
                " * Module 'T2Test::UR1' is outdated and known to be broken, please update to 1.0 or higher.",
                " * Module 'T2Test::KB1' is known to be broken in version 1.0 and below, newer versions have not been tested. You have: 0.9",
                " * Module 'T2Test::KB2' is known to be broken in version 0.5 and below, newer versions have not been tested. You have: 0.9",
            ],
            "Got expected report items"
        );
    }

    my %look;
    unshift @INC => sub {
        my ($this, $file) = @_;
        $look{$file}++ if $file =~ m{T2Test};
        return;
    };
    ok(!$CLASS->report, "Nothing to report");
    is_deeply(\%look, {}, "Did not try to load anything");

    ok(!$CLASS->report(1), "Nothing to report");
    is_deeply(
        \%look,
        {
            'T2Test/UG1.pm' => 1,
            'T2Test/UG2.pm' => 1,
            'T2Test/UR1.pm' => 1,
            'T2Test/UR2.pm' => 1,
            'T2Test/KB1.pm' => 1,
            'T2Test/KB2.pm' => 1,
        },
        "Tried to load modules"
    );
}

done_testing;
