#!perl

use strict;
use warnings;

use File::Basename;
use Test::More 0.96;
use lib 't';
use Util qw[tmpfile rewind slurp monkey_patch dir_list parse_case
  hashify connect_args clear_socket_source set_socket_source sort_headers
  $CRLF $LF];

use HTTP::Tiny;
BEGIN { monkey_patch() }
use SimpleCookieJar;

# XXX: this test is broken. SimpleCookieJar doesn't implement enough to behave
# correctly with the responses in the corpus data. And the requests in the
# corpus match that incorrect implementation, so if used against a real cookie
# jar, it will break. It still tests interactions with the jar object, so it
# is still a valuable test even in its partly broken state.

for my $class (
  'SimpleCookieJar',
  #'HTTP::CookieJar', # this test doesn't actually work with a real cookie jar
  #'HTTP::Cookies',   # would be nice to support eventually
) {
    (my $module = $class . ".pm") =~ s{::}{/}g;

    subtest $class => sub {
        eval { require $module; 1 } or do {
            die $@
                if $ENV{RELEASE_TESTING};
            plan skip_all => "Needs $class";
        };

        for my $file ( dir_list("corpus", qr/^cookies/ ) ) {
            my $label = basename($file);
            my $data = do { local (@ARGV,$/) = $file; <> };
            my @cases = split /--+\n/, $data;

            my $jar = $class->new();
            my $http = undef;
            my $case_n = 0;
            while (@cases) {
                my ($params, $expect_req, $give_res) = splice( @cases, 0, 3 );
                $case_n++;

                my $case = parse_case($params);

                my $url = $case->{url}[0];
                my $method = $case->{method}[0] || 'GET';
                my %headers = hashify( $case->{headers} );
                my %new_args = hashify( $case->{new_args} );

                if( exists $headers{Cookie} ) {
                    my $cookies = delete $headers{Cookie};
                    $jar->add( $url, $cookies );
                }

                if( exists $headers{'No-Cookie-Jar'} ) {
                    delete $headers{'No-Cookie-Jar'};
                    $jar = undef;
                }

                my %options;
                $options{headers} = \%headers if %headers;

                my $version = HTTP::Tiny->VERSION || 0;
                my $agent = $new_args{agent} || "HTTP-Tiny/$version";

                $new_args{cookie_jar} = $jar;

                # cleanup source data
                $expect_req =~ s{HTTP-Tiny/VERSION}{$agent};
                s{\n}{$CRLF}g for ($expect_req, $give_res);

                # setup mocking and test
                my $res_fh = tmpfile($give_res);
                my $req_fh = tmpfile();

                $http = HTTP::Tiny->new(keep_alive => 0, %new_args) if !defined $http;
                clear_socket_source();
                set_socket_source($req_fh, $res_fh);

                my @call_args = %options ? ($url, \%options) : ($url);
                my $response  = $http->get(@call_args);

                my $got_req = slurp($req_fh);
                is( sort_headers($got_req), sort_headers($expect_req), "$label case $case_n request data");
            }
        }
    };
}

done_testing;
