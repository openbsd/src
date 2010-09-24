#!./perl 

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
}

BEGIN {
    use Config;
    if( !$Config{d_alarm} ) {
        skip_all("alarm() not implemented on this platform");
    }
}

plan tests => 5;
my $Perl = which_perl();

my $start_time = time;
eval {
    local $SIG{ALRM} = sub { die "ALARM!\n" };
    alarm 3;

    # perlfunc recommends against using sleep in combination with alarm.
    1 while (time - $start_time < 6);
};
alarm 0;
my $diff = time - $start_time;

# alarm time might be one second less than you said.
is( $@, "ALARM!\n",             'alarm w/$SIG{ALRM} vs inf loop' );
ok( abs($diff - 3) <= 1,   "   right time" );


my $start_time = time;
eval {
    local $SIG{ALRM} = sub { die "ALARM!\n" };
    alarm 3;
    system(qq{$Perl -e "sleep 6"});
};
alarm 0;
$diff = time - $start_time;

# alarm time might be one second less than you said.
is( $@, "ALARM!\n",             'alarm w/$SIG{ALRM} vs system()' );

{
    local $TODO = "Why does system() block alarm() on $^O?"
		if $^O eq 'VMS' || $^O eq 'dos';
    ok( abs($diff - 3) <= 1,   "   right time (waited $diff secs for 3-sec alarm)" );
}


{
    local $SIG{"ALRM"} = sub { die };
    eval { alarm(1); my $x = qx($Perl -e "sleep 3") };
    chomp (my $foo = "foo\n");
    ok($foo eq "foo", '[perl #33928] chomp() fails after alarm(), `sleep`');
}
