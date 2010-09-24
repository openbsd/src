package autodie::test::missing;
use base qw(autodie);

sub exception_class {
    return "autodie::test::missing::exception";  # Doesn't exist!
}

1;
