use Test2::Bundle::Extended;

use Test2::Util::Times qw/render_bench/;

imported_ok qw{ render_bench };

sub TM() { 0.5 }

is(
    render_bench(0, 2.123456, TM, TM, TM, TM),
    "2.12346s on wallclock (0.50 usr 0.50 sys + 0.50 cusr 0.50 csys = 2.00 CPU)",
    "Got benchmark with < 10 second duration"
);

is(
    render_bench(0, 42.123456, TM, TM, TM, TM),
    "42.1235s on wallclock (0.50 usr 0.50 sys + 0.50 cusr 0.50 csys = 2.00 CPU)",
    "Got benchmark with < 1 minute duration"
);

is(
    render_bench(0, 422.123456, TM, TM, TM, TM),
    "07m:02.12s on wallclock (0.50 usr 0.50 sys + 0.50 cusr 0.50 csys = 2.00 CPU)",
    "Got benchmark with minute+ duration"
);

is(
    render_bench(0, 10422.123456, TM, TM, TM, TM),
    "02h:53m:42.12s on wallclock (0.50 usr 0.50 sys + 0.50 cusr 0.50 csys = 2.00 CPU)",
    "Got benchmark with hour+ duration"
);

is(
    render_bench(0, 501023.123456, TM, TM, TM, TM),
    "05d:19h:10m:23.12s on wallclock (0.50 usr 0.50 sys + 0.50 cusr 0.50 csys = 2.00 CPU)",
    "Got benchmark with day+ duration"
);

done_testing;
