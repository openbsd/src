#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

BEGIN {
  require Scalar::Util;

  if (grep { /set_prototype/ } @Scalar::Util::EXPORT_FAIL) {
    print "1..0\n";
    $skip=1;
  }
}

eval <<'EOT' unless $skip;
use Scalar::Util qw(set_prototype);

print "1..13\n";
$test = 0;

sub proto_is ($$) {
    $proto = prototype shift;
    $expected = shift;
    if (defined $expected) {
	print "# Got $proto, expected $expected\nnot " if $expected ne $proto;
    }
    else {
	print "# Got $proto, expected undef\nnot " if defined $proto;
    }
    print "ok ", ++$test, "\n";
}

sub f { }
proto_is 'f' => undef;
$r = set_prototype(\&f,'$');
proto_is 'f' => '$';
print "not " unless ref $r eq "CODE" and $r == \&f;
print "ok ", ++$test, " - return value\n";
set_prototype(\&f,undef);
proto_is 'f' => undef;
set_prototype(\&f,'');
proto_is 'f' => '';

sub g (@) { }
proto_is 'g' => '@';
set_prototype(\&g,undef);
proto_is 'g' => undef;

sub non_existent;
proto_is 'non_existent' => undef;
set_prototype(\&non_existent,'$$$');
proto_is 'non_existent' => '$$$';

sub forward_decl ($$$$);
proto_is 'forward_decl' => '$$$$';
set_prototype(\&forward_decl,'\%');
proto_is 'forward_decl' => '\%';

eval { &set_prototype( 'f', '' ); };
print "not " unless $@ =~ /^set_prototype: not a reference/;
print "ok ", ++$test, " - error msg\n";
eval { &set_prototype( \'f', '' ); };
print "not " unless $@ =~ /^set_prototype: not a subroutine reference/;
print "ok ", ++$test, " - error msg\n";
EOT
