use Test2::Bundle::Extended;

my $check = hash {
  field first   => 42;
  field second  => undef;
  field third   => DNE();
  field fourth  => in_set(42, undef);
  field fifth   => in_set(42, undef);
  field sixth   => in_set(42, DNE());
  field seventh => in_set(42, DNE());
  field eighth  => not_in_set(DNE());
};

is(
  {
    first   => 42,
    second  => undef,
    # third DNE
    fourth  => 42,
    fifth   => undef,
    sixth   => 42,
    # seventh DNE
    eighth  => 42,
  },
  $check
);

done_testing;
