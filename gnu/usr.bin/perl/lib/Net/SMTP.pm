# Net::SMTP.pm
#
# Copyright (c) 1995-1997 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Net::SMTP;

require 5.001;

use strict;
use vars qw($VERSION @ISA);
use Socket 1.3;
use Carp;
use IO::Socket;
use Net::Cmd;
use Net::Config;

$VERSION = "2.24"; # $Id: //depot/libnet/Net/SMTP.pm#25 $

@ISA = qw(Net::Cmd IO::Socket::INET);

sub new
{
 my $self = shift;
 my $type = ref($self) || $self;
 my $host = shift if @_ % 2;
 my %arg  = @_; 
 my $hosts = defined $host ? [ $host ] : $NetConfig{smtp_hosts};
 my $obj;

 my $h;
 foreach $h (@{$hosts})
  {
   $obj = $type->SUPER::new(PeerAddr => ($host = $h), 
			    PeerPort => $arg{Port} || 'smtp(25)',
			    LocalAddr => $arg{LocalAddr},
			    LocalPort => $arg{LocalPort},
			    Proto    => 'tcp',
			    Timeout  => defined $arg{Timeout}
						? $arg{Timeout}
						: 120
			   ) and last;
  }

 return undef
	unless defined $obj;

 $obj->autoflush(1);

 $obj->debug(exists $arg{Debug} ? $arg{Debug} : undef);

 unless ($obj->response() == CMD_OK)
  {
   $obj->close();
   return undef;
  }

 ${*$obj}{'net_smtp_host'} = $host;

 (${*$obj}{'net_smtp_banner'}) = $obj->message;
 (${*$obj}{'net_smtp_domain'}) = $obj->message =~ /\A\s*(\S+)/;

 unless($obj->hello($arg{Hello} || ""))
  {
   $obj->close();
   return undef;
  }

 $obj;
}

##
## User interface methods
##

sub banner
{
 my $me = shift;

 return ${*$me}{'net_smtp_banner'} || undef;
}

sub domain
{
 my $me = shift;

 return ${*$me}{'net_smtp_domain'} || undef;
}

sub etrn {
    my $self = shift;
    defined($self->supports('ETRN',500,["Command unknown: 'ETRN'"])) &&
	$self->_ETRN(@_);
}

sub auth {
    my ($self, $username, $password) = @_;

    require MIME::Base64;
    require Authen::SASL;

    my $mechanisms = $self->supports('AUTH',500,["Command unknown: 'AUTH'"]);
    return unless defined $mechanisms;

    my $sasl;

    if (ref($username) and UNIVERSAL::isa($username,'Authen::SASL')) {
      $sasl = $username;
      $sasl->mechanism($mechanisms);
    }
    else {
      die "auth(username, password)" if not length $username;
      $sasl = Authen::SASL->new(mechanism=> $mechanisms,
				callback => { user => $username,
                                              pass => $password,
					      authname => $username,
                                            });
    }

    # We should probably allow the user to pass the host, but I don't
    # currently know and SASL mechanisms that are used by smtp that need it
    my $client = $sasl->client_new('smtp',${*$self}{'net_smtp_host'},0);
    my $str    = $client->client_start;
    # We dont support sasl mechanisms that encrypt the socket traffic.
    # todo that we would really need to change the ISA hierarchy
    # so we dont inherit from IO::Socket, but instead hold it in an attribute

    my @cmd = ("AUTH", $client->mechanism, MIME::Base64::encode_base64($str,''));
    my $code;

    while (($code = $self->command(@cmd)->response()) == CMD_MORE) {
      @cmd = (MIME::Base64::encode_base64(
	$client->client_step(
	  MIME::Base64::decode_base64(
	    ($self->message)[0]
	  )
	), ''
      ));
    }

    $code == CMD_OK;
}

sub hello
{
 my $me = shift;
 my $domain = shift || "localhost.localdomain";
 my $ok = $me->_EHLO($domain);
 my @msg = $me->message;

 if($ok)
  {
   my $h = ${*$me}{'net_smtp_esmtp'} = {};
   my $ln;
   foreach $ln (@msg) {
     $h->{uc $1} = $2
	if $ln =~ /(\w+)\b[= \t]*([^\n]*)/;
    }
  }
 elsif($me->status == CMD_ERROR) 
  {
   @msg = $me->message
	if $ok = $me->_HELO($domain);
  }

 $ok && $msg[0] =~ /\A\s*(\S+)/
	? $1
	: undef;
}

sub supports {
    my $self = shift;
    my $cmd = uc shift;
    return ${*$self}{'net_smtp_esmtp'}->{$cmd}
	if exists ${*$self}{'net_smtp_esmtp'}->{$cmd};
    $self->set_status(@_)
	if @_;
    return;
}

sub _addr {
  my $addr = shift;
  $addr = "" unless defined $addr;
  $addr =~ s/^\s*<?\s*|\s*>?\s*$//sg;
  "<$addr>";
}

sub mail
{
 my $me = shift;
 my $addr = _addr(shift);
 my $opts = "";

 if(@_)
  {
   my %opt = @_;
   my($k,$v);

   if(exists ${*$me}{'net_smtp_esmtp'})
    {
     my $esmtp = ${*$me}{'net_smtp_esmtp'};

     if(defined($v = delete $opt{Size}))
      {
       if(exists $esmtp->{SIZE})
        {
         $opts .= sprintf " SIZE=%d", $v + 0
        }
       else
        {
	 carp 'Net::SMTP::mail: SIZE option not supported by host';
        }
      }

     if(defined($v = delete $opt{Return}))
      {
       if(exists $esmtp->{DSN})
        {
	 $opts .= " RET=" . uc $v
        }
       else
        {
	 carp 'Net::SMTP::mail: DSN option not supported by host';
        }
      }

     if(defined($v = delete $opt{Bits}))
      {
       if(exists $esmtp->{'8BITMIME'})
        {
	 $opts .= $v == 8 ? " BODY=8BITMIME" : " BODY=7BIT"
        }
       else
        {
	 carp 'Net::SMTP::mail: 8BITMIME option not supported by host';
        }
      }

     if(defined($v = delete $opt{Transaction}))
      {
       if(exists $esmtp->{CHECKPOINT})
        {
	 $opts .= " TRANSID=" . _addr($v);
        }
       else
        {
	 carp 'Net::SMTP::mail: CHECKPOINT option not supported by host';
        }
      }

     if(defined($v = delete $opt{Envelope}))
      {
       if(exists $esmtp->{DSN})
        {
	 $v =~ s/([^\041-\176]|=|\+)/sprintf "+%02x", ord($1)/sge;
	 $opts .= " ENVID=$v"
        }
       else
        {
	 carp 'Net::SMTP::mail: DSN option not supported by host';
        }
      }

     carp 'Net::SMTP::recipient: unknown option(s) '
		. join(" ", keys %opt)
		. ' - ignored'
	if scalar keys %opt;
    }
   else
    {
     carp 'Net::SMTP::mail: ESMTP not supported by host - options discarded :-(';
    }
  }

 $me->_MAIL("FROM:".$addr.$opts);
}

sub send	  { shift->_SEND("FROM:" . _addr($_[0])) }
sub send_or_mail  { shift->_SOML("FROM:" . _addr($_[0])) }
sub send_and_mail { shift->_SAML("FROM:" . _addr($_[0])) }

sub reset
{
 my $me = shift;

 $me->dataend()
	if(exists ${*$me}{'net_smtp_lastch'});

 $me->_RSET();
}


sub recipient
{
 my $smtp = shift;
 my $opts = "";
 my $skip_bad = 0;

 if(@_ && ref($_[-1]))
  {
   my %opt = %{pop(@_)};
   my $v;

   $skip_bad = delete $opt{'SkipBad'};

   if(exists ${*$smtp}{'net_smtp_esmtp'})
    {
     my $esmtp = ${*$smtp}{'net_smtp_esmtp'};

     if(defined($v = delete $opt{Notify}))
      {
       if(exists $esmtp->{DSN})
        {
	 $opts .= " NOTIFY=" . join(",",map { uc $_ } @$v)
        }
       else
        {
	 carp 'Net::SMTP::recipient: DSN option not supported by host';
        }
      }

     carp 'Net::SMTP::recipient: unknown option(s) '
		. join(" ", keys %opt)
		. ' - ignored'
	if scalar keys %opt;
    }
   elsif(%opt)
    {
     carp 'Net::SMTP::recipient: ESMTP not supported by host - options discarded :-(';
    }
  }

 my @ok;
 my $addr;
 foreach $addr (@_) 
  {
    if($smtp->_RCPT("TO:" . _addr($addr) . $opts)) {
      push(@ok,$addr) if $skip_bad;
    }
    elsif(!$skip_bad) {
      return 0;
    }
  }

 return $skip_bad ? @ok : 1;
}

BEGIN {
  *to  = \&recipient;
  *cc  = \&recipient;
  *bcc = \&recipient;
}

sub data
{
 my $me = shift;

 my $ok = $me->_DATA() && $me->datasend(@_);

 $ok && @_ ? $me->dataend
	   : $ok;
}

sub datafh {
  my $me = shift;
  return unless $me->_DATA();
  return $me->tied_fh;
}

sub expand
{
 my $me = shift;

 $me->_EXPN(@_) ? ($me->message)
		: ();
}


sub verify { shift->_VRFY(@_) }

sub help
{
 my $me = shift;

 $me->_HELP(@_) ? scalar $me->message
	        : undef;
}

sub quit
{
 my $me = shift;

 $me->_QUIT;
 $me->close;
}

sub DESTROY
{
# ignore
}

##
## RFC821 commands
##

sub _EHLO { shift->command("EHLO", @_)->response()  == CMD_OK }   
sub _HELO { shift->command("HELO", @_)->response()  == CMD_OK }   
sub _MAIL { shift->command("MAIL", @_)->response()  == CMD_OK }   
sub _RCPT { shift->command("RCPT", @_)->response()  == CMD_OK }   
sub _SEND { shift->command("SEND", @_)->response()  == CMD_OK }   
sub _SAML { shift->command("SAML", @_)->response()  == CMD_OK }   
sub _SOML { shift->command("SOML", @_)->response()  == CMD_OK }   
sub _VRFY { shift->command("VRFY", @_)->response()  == CMD_OK }   
sub _EXPN { shift->command("EXPN", @_)->response()  == CMD_OK }   
sub _HELP { shift->command("HELP", @_)->response()  == CMD_OK }   
sub _RSET { shift->command("RSET")->response()	    == CMD_OK }   
sub _NOOP { shift->command("NOOP")->response()	    == CMD_OK }   
sub _QUIT { shift->command("QUIT")->response()	    == CMD_OK }   
sub _DATA { shift->command("DATA")->response()	    == CMD_MORE } 
sub _TURN { shift->unsupported(@_); } 			   	  
sub _ETRN { shift->command("ETRN", @_)->response()  == CMD_OK }
sub _AUTH { shift->command("AUTH", @_)->response()  == CMD_OK }   

1;

__END__

=head1 NAME

Net::SMTP - Simple Mail Transfer Protocol Client

=head1 SYNOPSIS

    use Net::SMTP;

    # Constructors
    $smtp = Net::SMTP->new('mailhost');
    $smtp = Net::SMTP->new('mailhost', Timeout => 60);

=head1 DESCRIPTION

This module implements a client interface to the SMTP and ESMTP
protocol, enabling a perl5 application to talk to SMTP servers. This
documentation assumes that you are familiar with the concepts of the
SMTP protocol described in RFC821.

A new Net::SMTP object must be created with the I<new> method. Once
this has been done, all SMTP commands are accessed through this object.

The Net::SMTP class is a subclass of Net::Cmd and IO::Socket::INET.

=head1 EXAMPLES

This example prints the mail domain name of the SMTP server known as mailhost:

    #!/usr/local/bin/perl -w

    use Net::SMTP;

    $smtp = Net::SMTP->new('mailhost');
    print $smtp->domain,"\n";
    $smtp->quit;

This example sends a small message to the postmaster at the SMTP server
known as mailhost:

    #!/usr/local/bin/perl -w

    use Net::SMTP;

    $smtp = Net::SMTP->new('mailhost');

    $smtp->mail($ENV{USER});
    $smtp->to('postmaster');

    $smtp->data();
    $smtp->datasend("To: postmaster\n");
    $smtp->datasend("\n");
    $smtp->datasend("A simple test message\n");
    $smtp->dataend();

    $smtp->quit;

=head1 CONSTRUCTOR

=over 4

=item new Net::SMTP [ HOST, ] [ OPTIONS ]

This is the constructor for a new Net::SMTP object. C<HOST> is the
name of the remote host to which an SMTP connection is required.

If C<HOST> is not given, then the C<SMTP_Host> specified in C<Net::Config>
will be used.

C<OPTIONS> are passed in a hash like fashion, using key and value pairs.
Possible options are:

B<Hello> - SMTP requires that you identify yourself. This option
specifies a string to pass as your mail domain. If not
given a guess will be taken.

B<LocalAddr> and B<LocalPort> - These parameters are passed directly
to IO::Socket to allow binding the socket to a local port.

B<Timeout> - Maximum time, in seconds, to wait for a response from the
SMTP server (default: 120)

B<Debug> - Enable debugging information


Example:


    $smtp = Net::SMTP->new('mailhost',
			   Hello => 'my.mail.domain'
			   Timeout => 30,
                           Debug   => 1,
			  );

=back

=head1 METHODS

Unless otherwise stated all methods return either a I<true> or I<false>
value, with I<true> meaning that the operation was a success. When a method
states that it returns a value, failure will be returned as I<undef> or an
empty list.

=over 4

=item banner ()

Returns the banner message which the server replied with when the
initial connection was made.

=item domain ()

Returns the domain that the remote SMTP server identified itself as during
connection.

=item hello ( DOMAIN )

Tell the remote server the mail domain which you are in using the EHLO
command (or HELO if EHLO fails).  Since this method is invoked
automatically when the Net::SMTP object is constructed the user should
normally not have to call it manually.

=item etrn ( DOMAIN )

Request a queue run for the DOMAIN given.

=item auth ( USERNAME, PASSWORD )

Attempt SASL authentication.

=item mail ( ADDRESS [, OPTIONS] )

=item send ( ADDRESS )

=item send_or_mail ( ADDRESS )

=item send_and_mail ( ADDRESS )

Send the appropriate command to the server MAIL, SEND, SOML or SAML. C<ADDRESS>
is the address of the sender. This initiates the sending of a message. The
method C<recipient> should be called for each address that the message is to
be sent to.

The C<mail> method can some additional ESMTP OPTIONS which is passed
in hash like fashion, using key and value pairs.  Possible options are:

 Size        => <bytes>
 Return      => <???>
 Bits        => "7" | "8"
 Transaction => <ADDRESS>
 Envelope    => <ENVID>


=item reset ()

Reset the status of the server. This may be called after a message has been 
initiated, but before any data has been sent, to cancel the sending of the
message.

=item recipient ( ADDRESS [, ADDRESS [ ...]] [, OPTIONS ] )

Notify the server that the current message should be sent to all of the
addresses given. Each address is sent as a separate command to the server.
Should the sending of any address result in a failure then the
process is aborted and a I<false> value is returned. It is up to the
user to call C<reset> if they so desire.

The C<recipient> method can some additional OPTIONS which is passed
in hash like fashion, using key and value pairs.  Possible options are:

 Notify    =>
 SkipBad   => ignore bad addresses

If C<SkipBad> is true the C<recipient> will not return an error when a
bad address is encountered and it will return an array of addresses
that did succeed.

  $smtp->recipient($recipient1,$recipient2);  # Good
  $smtp->recipient($recipient1,$recipient2, { SkipBad => 1 });  # Good
  $smtp->recipient("$recipient,$recipient2"); # BAD   

=item to ( ADDRESS [, ADDRESS [...]] )

=item cc ( ADDRESS [, ADDRESS [...]] )

=item bcc ( ADDRESS [, ADDRESS [...]] )

Synonyms for C<recipient>.

=item data ( [ DATA ] )

Initiate the sending of the data from the current message. 

C<DATA> may be a reference to a list or a list. If specified the contents
of C<DATA> and a termination string C<".\r\n"> is sent to the server. And the
result will be true if the data was accepted.

If C<DATA> is not specified then the result will indicate that the server
wishes the data to be sent. The data must then be sent using the C<datasend>
and C<dataend> methods described in L<Net::Cmd>.

=item expand ( ADDRESS )

Request the server to expand the given address Returns an array
which contains the text read from the server.

=item verify ( ADDRESS )

Verify that C<ADDRESS> is a legitimate mailing address.

=item help ( [ $subject ] )

Request help text from the server. Returns the text or undef upon failure

=item quit ()

Send the QUIT command to the remote SMTP server and close the socket connection.

=back

=head1 ADDRESSES

All methods that accept addresses expect the address to be a valid rfc2821-quoted address, although
Net::SMTP will accept accept the address surrounded by angle brackets.

 funny user@domain      WRONG
 "funny user"@domain    RIGHT, recommended
 <"funny user"@domain>  OK

=head1 SEE ALSO

L<Net::Cmd>

=head1 AUTHOR

Graham Barr <gbarr@pobox.com>

=head1 COPYRIGHT

Copyright (c) 1995-1997 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=for html <hr>

I<$Id: //depot/libnet/Net/SMTP.pm#25 $>

=cut
