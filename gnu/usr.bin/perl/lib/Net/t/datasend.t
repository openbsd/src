#!./perl -w

BEGIN {
  package Foo;

  use IO::File;
  use Net::Cmd;
  @ISA = qw(Net::Cmd IO::File);

  sub timeout { 0 }

  sub new {
    my $fh = shift->new_tmpfile;
    binmode($fh);
    $fh;
  }

  sub output {
    my $self = shift;
    seek($self,0,0);
    local $/ = undef;
    scalar(<$self>);
  }

  sub response {
    return Net::Cmd::CMD_OK;
  }
}

(my $libnet_t = __FILE__) =~ s/datasend.t/libnet_t.pl/;
require $libnet_t or die;

print "1..51\n";

sub check {
  my $expect = pop;
  my $cmd = Foo->new;
  ok($cmd->datasend, 'datasend') unless @_;
  foreach my $line (@_) {
    ok($cmd->datasend($line), 'datasend');
  }
  ok($cmd->dataend, 'dataend');
  is(
    unpack("H*",$cmd->output),
    unpack("H*",$expect)
  );
}

my $cmd;

check(
  # nothing

  ".\015\012"
);

check(
  "a",

  "a\015\012.\015\012",
);

check(
  "a\r",

  "a\015\015\012.\015\012",
);

check(
  "a\rb",

  "a\015b\015\012.\015\012",
);

check(
  "a\rb\n",

  "a\015b\015\012.\015\012",
);

check(
  "a\rb\n\n",

  "a\015b\015\012\015\012.\015\012",
);

check(
  "a\r",
  "\nb",

  "a\015\012b\015\012.\015\012",
);

check(
  "a\r",
  "\nb\n",

  "a\015\012b\015\012.\015\012",
);

check(
  "a\r",
  "\nb\r\n",

  "a\015\012b\015\012.\015\012",
);

check(
  "a\r",
  "\nb\r\n\n",

  "a\015\012b\015\012\015\012.\015\012",
);

check(
  "a\n.b\n",

  "a\015\012..b\015\012.\015\012",
);

check(
  ".a\n.b\n",

  "..a\015\012..b\015\012.\015\012",
);

check(
  ".a\n",
  ".b\n",

  "..a\015\012..b\015\012.\015\012",
);

check(
  ".a",
  ".b\n",

  "..a.b\015\012.\015\012",
);

check(
  "a\n.",

  "a\015\012..\015\012.\015\012",
);

