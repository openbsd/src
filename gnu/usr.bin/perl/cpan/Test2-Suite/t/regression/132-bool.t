use Test2::Require::AuthorTesting;
use Test2::Require::Perl 'v5.20';
use Test2::V0;
use Test2::Plugin::BailOnFail;

opendir(my $dh, 'lib/Test2/Compare/') or die "Could not open compare lib dir: $!";

for my $file (readdir($dh)) {
    next unless $file =~ m/.pm$/;
    next if $file eq 'Delta.pm';

    require "Test2/Compare/$file";
    my $name = $file;
    $name =~ s/\.pm$//g;
    my $mod = "Test2::Compare::$name";

    my $test = "./t/modules/Compare/$name.t";
    next unless -f $test;

    eval <<"    EOT" or die $@;
        package $mod;

        require Test2::Tools::Basic;
        require Carp;

        use overload bool => sub { Carp::confess( 'illegal use of overloaded bool') } ;
        use overload '""' => sub { \$_[0] };

        my \$err;
        main::subtest($name => sub {
            package Test::$mod;
            local \$@;

            main::like(
                main::dies(sub { if(bless({}, "$mod")) { die "oops" }}),
                qr/illegal use of overloaded bool/,
                "Override for $mod is in place",
            );

            do "$test";
            \$err = \$@;
            1;
        });

        eval <<"        ETT" or die $@;
            no overload 'bool';
            no overload '""';
        1;
        ETT

        die \$err if \$err;

        1;
    EOT
}

done_testing;
