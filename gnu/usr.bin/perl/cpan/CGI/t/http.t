#!./perl -w

# Fixes RT 12909

use lib qw(t/lib);

use Test::More tests => 7;
use CGI;

my $cgi = CGI->new();

{
    # http() without arguments should not cause warnings
    local $SIG{__WARN__} = sub { die @_ };
    ok eval { $cgi->http(); 1 },  "http() without arguments doesn't warn";
    ok eval { $cgi->https(); 1 }, "https() without arguments doesn't warn";
}

{
    # Capitalization and the use of hyphens versus underscores are not significant.
    local $ENV{'HTTP_HOST'}   = 'foo';
    is $cgi->http('Host'),      'foo', 'http("Host") returns $ENV{HTTP_HOST}';
    is $cgi->http('http-host'), 'foo', 'http("http-host") returns $ENV{HTTP_HOST}';
}

{
    # Called with no arguments returns the list of HTTP environment variables
    local $ENV{'HTTPS_FOO'} = 'bar';
    my @http = $cgi->http();
    is scalar( grep /^HTTPS/, @http), 0, "http() doesn't return HTTPS variables";
}

{
    # https()
    # The same as http(), but operates on the HTTPS environment variables present when the SSL protocol is in
    # effect.  Can be used to determine whether SSL is turned on.
    my @expect = grep /^HTTPS/, keys %ENV;
    push @expect, 'HTTPS'         if not exists $ENV{HTTPS};
    push @expect, 'HTTPS_KEYSIZE' if not exists $ENV{HTTPS_KEYSIZE};
    local $ENV{'HTTPS'} = 'ON';
    local $ENV{'HTTPS_KEYSIZE'} = 512;
    is $cgi->https(), 'ON', 'scalar context to check SSL is on';
    ok eq_set( [$cgi->https()], \@expect), 'list context returns https keys';
}
