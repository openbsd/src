use Moped::Msg;
my ($MCC, $MNC, $LAC, $Cell) =
  Moped::Msg::get_gsm_network_info();
print "MCC  = $MCC\n";
print "MNC  = $MNC\n";
print "LAC  = $LAC\n";
print "Cell = $Cell\n";

