
# Time-stamp: "2004-03-30 18:02:24 AST"
#sub Locale::Maketext::DEBUG () {10}
use Locale::Maketext;

use Test;
BEGIN { plan tests => 19 };

print "#\n# Testing non-tight insertion of super-ordinate language tags...\n#\n";

my @in = grep m/\S/, split /[\n\r]/, q{
 NIX => NIX
  sv => sv
  en => en
 hai => hai

          pt-br => pt-br pt
       pt-br fr => pt-br fr pt
    pt-br fr pt => pt-br fr pt
 pt-br fr pt de => pt-br fr pt de
 de pt-br fr pt => de pt-br fr pt
    de pt-br fr => de pt-br fr pt
   hai pt-br fr => hai pt-br fr  pt

# Now test multi-part complicateds:
   pt-br-janeiro fr => pt-br-janeiro fr pt-br pt 
pt-br-janeiro de fr => pt-br-janeiro de fr pt-br pt
pt-br-janeiro de pt fr => pt-br-janeiro de pt fr pt-br

ja    pt-br-janeiro fr => ja pt-br-janeiro fr pt-br pt 
ja pt-br-janeiro de fr => ja pt-br-janeiro de fr pt-br pt
ja pt-br-janeiro de pt fr => ja pt-br-janeiro de pt fr pt-br

pt-br-janeiro de pt-br fr => pt-br-janeiro de pt-br fr pt
 # an odd case, since we don't filter for uniqueness in this sub
 
};

$Locale::Maketext::MATCH_SUPERS_TIGHTLY = 0;

foreach my $in (@in) {
  $in =~ s/^\s+//s;
  $in =~ s/\s+$//s;
  $in =~ s/#.+//s;
  next unless $in =~ m/\S/;
  
  my(@in, @should);
  {
    die "What kind of line is <$in>?!"
     unless $in =~ m/^(.+)=>(.+)$/s;
  
    my($i,$s) = ($1, $2);
    @in     = ($i =~ m/(\S+)/g);
    @should = ($s =~ m/(\S+)/g);
    #print "{@in}{@should}\n";
  }
  my @out = Locale::Maketext->_add_supers(
    ("@in" eq 'NIX') ? () : @in
  );
  #print "O: ", join(' ', map "<$_>", @out), "\n";
  @out = 'NIX' unless @out;

  
  if( @out == @should
      and lc( join "\e", @out ) eq lc( join "\e", @should )
  ) {
    print "#     Happily got [@out] from [$in]\n";
    ok 1;
  } else {
    ok 0;
    print "#!!Got:         [@out]\n",
          "#!! but wanted: [@should]\n",
          "#!! from \"$in\"\n#\n";
  }
}

print "#\n#\n# Bye-bye!\n";
ok 1;

