use Test2::V0;
use Test2::API qw/test2_stack/;
use PerlIO;
# HARNESS-NO-FORMATTER

imported_ok qw{
    ok pass fail diag note todo skip
    plan skip_all done_testing bail_out

    gen_event

    intercept context

    cmp_ok

    subtest
    can_ok isa_ok DOES_ok
    set_encoding
    imported_ok not_imported_ok
    ref_ok ref_is ref_is_not
    mock mocked

    dies lives try_ok

    is like isnt unlike
    match mismatch validator
    hash array object meta number string bool check_isa
    in_set not_in_set check_set
    item field call call_list call_hash prop check all_items all_keys all_vals all_values
    etc end filter_items
    T F D DF E DNE FDNE U L
    event fail_events
    exact_ref

    is_refcount is_oneref refcount
};

ok(Test2::Plugin::ExitSummary->active, "Exit Summary is loaded");
ok(defined(Test2::Plugin::SRand->seed), "SRand is loaded");

subtest strictures => sub {
    local $^H;
    my $hbefore = $^H;
    Test2::V0->import;
    my $hafter = $^H;

    my $strict = do { local $^H; strict->import(); $^H };

    ok($strict,               'sanity, got $^H value for strict');
    ok(!($hbefore & $strict), "strict is not on before loading Test2::V0");
    ok(($hafter & $strict),   "strict is on after loading Test2::V0");
};

subtest warnings => sub {
    local ${^WARNING_BITS};
    my $wbefore = ${^WARNING_BITS} || '';
    Test2::V0->import;
    my $wafter = ${^WARNING_BITS} || '';

    my $warnings = do { local ${^WARNING_BITS}; 'warnings'->import(); ${^WARNING_BITS} || '' };

    ok($warnings, 'sanity, got ${^WARNING_BITS} value for warnings');
    ok($wbefore ne $warnings, "warnings are not on before loading Test2::V0") || diag($wbefore, "\n", $warnings);
    ok(($wafter & $warnings), "warnings are on after loading Test2::V0");
};

subtest utf8 => sub {
    ok(utf8::is_utf8("癸"), "utf8 pragma is on");

    # -2 cause the subtest adds to the stack
    my $format = test2_stack()->[-2]->format;
    my $handles = $format->handles or return;
    for my $hn (0 .. @$handles) {
        my $h = $handles->[$hn] || next;
        my $layers = { map {$_ => 1} PerlIO::get_layers($h) };
        ok($layers->{utf8}, "utf8 is on for formatter handle $hn");
    }
};

subtest "rename imports" => sub {
    package A::Consumer;
    use Test2::V0 ':DEFAULT', '!subtest', subtest => {-as => 'a_subtest'};
    imported_ok('a_subtest');
    not_imported_ok('subtest');
};

subtest "no meta" => sub {
    package B::Consumer;
    use Test2::V0 '!meta';
    imported_ok('meta_check');
    not_imported_ok('meta');
};

done_testing;

1;
