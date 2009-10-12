# -*- Mode: cperl; coding: utf-8; cperl-indent-level: 4 -*-
# vim: ts=4 sts=4 sw=4:
package CPAN::LWP::UserAgent;
use strict;
use vars qw(@ISA $USER $PASSWD $SETUPDONE);
# we delay requiring LWP::UserAgent and setting up inheritance until we need it

$CPAN::LWP::UserAgent::VERSION = $CPAN::LWP::UserAgent::VERSION = "1.00";

sub config {
    return if $SETUPDONE;
    if ($CPAN::META->has_usable('LWP::UserAgent')) {
        require LWP::UserAgent;
        @ISA = qw(Exporter LWP::UserAgent); ## no critic
        $SETUPDONE++;
    } else {
        $CPAN::Frontend->mywarn("  LWP::UserAgent not available\n");
    }
}

sub get_basic_credentials {
    my($self, $realm, $uri, $proxy) = @_;
    if ($USER && $PASSWD) {
        return ($USER, $PASSWD);
    }
    if ( $proxy ) {
        ($USER,$PASSWD) = $self->get_proxy_credentials();
    } else {
        ($USER,$PASSWD) = $self->get_non_proxy_credentials();
    }
    return($USER,$PASSWD);
}

sub get_proxy_credentials {
    my $self = shift;
    my ($user, $password);
    if ( defined $CPAN::Config->{proxy_user} ) {
        $user = $CPAN::Config->{proxy_user};
        $password = $CPAN::Config->{proxy_pass} || "";
        return ($user, $password);
    }
    my $username_prompt = "\nProxy authentication needed!
 (Note: to permanently configure username and password run
   o conf proxy_user your_username
   o conf proxy_pass your_password
     )\nUsername:";
    ($user, $password) =
        _get_username_and_password_from_user($username_prompt);
    return ($user,$password);
}

sub get_non_proxy_credentials {
    my $self = shift;
    my ($user,$password);
    if ( defined $CPAN::Config->{username} ) {
        $user = $CPAN::Config->{username};
        $password = $CPAN::Config->{password} || "";
        return ($user, $password);
    }
    my $username_prompt = "\nAuthentication needed!
     (Note: to permanently configure username and password run
       o conf username your_username
       o conf password your_password
     )\nUsername:";

    ($user, $password) =
        _get_username_and_password_from_user($username_prompt);
    return ($user,$password);
}

sub _get_username_and_password_from_user {
    my $username_message = shift;
    my ($username,$password);

    ExtUtils::MakeMaker->import(qw(prompt));
    $username = prompt($username_message);
        if ($CPAN::META->has_inst("Term::ReadKey")) {
            Term::ReadKey::ReadMode("noecho");
        }
    else {
        $CPAN::Frontend->mywarn(
            "Warning: Term::ReadKey seems not to be available, your password will be echoed to the terminal!\n"
        );
    }
    $password = prompt("Password:");

        if ($CPAN::META->has_inst("Term::ReadKey")) {
            Term::ReadKey::ReadMode("restore");
        }
        $CPAN::Frontend->myprint("\n\n");
    return ($username,$password);
}

# mirror(): Its purpose is to deal with proxy authentication. When we
# call SUPER::mirror, we relly call the mirror method in
# LWP::UserAgent. LWP::UserAgent will then call
# $self->get_basic_credentials or some equivalent and this will be
# $self->dispatched to our own get_basic_credentials method.

# Our own get_basic_credentials sets $USER and $PASSWD, two globals.

# 407 stands for HTTP_PROXY_AUTHENTICATION_REQUIRED. Which means
# although we have gone through our get_basic_credentials, the proxy
# server refuses to connect. This could be a case where the username or
# password has changed in the meantime, so I'm trying once again without
# $USER and $PASSWD to give the get_basic_credentials routine another
# chance to set $USER and $PASSWD.

# mirror(): Its purpose is to deal with proxy authentication. When we
# call SUPER::mirror, we relly call the mirror method in
# LWP::UserAgent. LWP::UserAgent will then call
# $self->get_basic_credentials or some equivalent and this will be
# $self->dispatched to our own get_basic_credentials method.

# Our own get_basic_credentials sets $USER and $PASSWD, two globals.

# 407 stands for HTTP_PROXY_AUTHENTICATION_REQUIRED. Which means
# although we have gone through our get_basic_credentials, the proxy
# server refuses to connect. This could be a case where the username or
# password has changed in the meantime, so I'm trying once again without
# $USER and $PASSWD to give the get_basic_credentials routine another
# chance to set $USER and $PASSWD.

sub mirror {
    my($self,$url,$aslocal) = @_;
    my $result = $self->SUPER::mirror($url,$aslocal);
    if ($result->code == 407) {
        undef $USER;
        undef $PASSWD;
        $result = $self->SUPER::mirror($url,$aslocal);
    }
    $result;
}

1;
