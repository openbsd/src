use ExtUtils::MakeMaker;
use ExtUtils::Constant 0.11 'WriteConstants';
use Config;
WriteMakefile(
    NAME	 => 'Socket',
    VERSION_FROM => 'Socket.pm',
   ($Config{libs} =~ /(-lsocks\S*)/ ? (LIBS => [ "$1" ] ) : ()),
    XSPROTOARG   => '-noprototypes', 		# XXX remove later?
    realclean => {FILES=> 'const-c.inc const-xs.inc'},
);
my @names = (qw(AF_802 AF_AAL AF_APPLETALK AF_CCITT AF_CHAOS AF_CTF
		AF_DATAKIT AF_DECnet AF_DLI AF_ECMA AF_GOSIP AF_HYLINK
		AF_IMPLINK AF_INET AF_INET6 AF_ISO AF_KEY
		AF_LAST AF_LAT AF_LINK AF_MAX AF_NBS AF_NIT AF_NS
		AF_OSI AF_OSINET AF_PUP AF_ROUTE AF_SNA
		AF_UNIX AF_UNSPEC AF_USER AF_WAN AF_X25
		IOV_MAX IP_OPTIONS IP_HDRINCL IP_TOS IP_TTL IP_RECVOPTS
	        IP_RECVRETOPTS IP_RETOPTS
		MSG_BCAST MSG_BTAG MSG_CTLFLAGS MSG_CTLIGNORE MSG_DONTWAIT
		MSG_EOF MSG_EOR MSG_ERRQUEUE MSG_ETAG MSG_FIN
		MSG_MAXIOVLEN MSG_MCAST MSG_NOSIGNAL MSG_RST MSG_SYN
		MSG_TRUNC MSG_URG MSG_WAITALL MSG_WIRE
		PF_802 PF_AAL PF_APPLETALK PF_CCITT PF_CHAOS PF_CTF
		PF_DATAKIT PF_DECnet PF_DLI PF_ECMA PF_GOSIP PF_HYLINK
		PF_IMPLINK PF_INET PF_INET6 PF_ISO PF_KEY
		PF_LAST PF_LAT PF_LINK PF_MAX PF_NBS PF_NIT PF_NS
		PF_OSI PF_OSINET PF_PUP PF_ROUTE PF_SNA
		PF_UNIX PF_UNSPEC PF_USER PF_WAN PF_X25
		SCM_CONNECT SCM_CREDENTIALS SCM_CREDS SCM_TIMESTAMP
		SOCK_DGRAM SOCK_RAW SOCK_RDM SOCK_SEQPACKET SOCK_STREAM
		SOL_SOCKET SOMAXCONN
		SO_ACCEPTCONN SO_ATTACH_FILTER SO_BACKLOG SO_BROADCAST
		SO_CHAMELEON SO_DEBUG SO_DETACH_FILTER SO_DGRAM_ERRIND
		SO_DONTLINGER SO_DONTROUTE SO_ERROR SO_FAMILY
		SO_KEEPALIVE SO_LINGER SO_OOBINLINE
		SO_PASSCRED SO_PASSIFNAME SO_PEERCRED
		SO_PROTOCOL SO_PROTOTYPE
		SO_RCVBUF SO_RCVLOWAT SO_RCVTIMEO
		SO_REUSEADDR SO_REUSEPORT
		SO_SECURITY_AUTHENTICATION
		SO_SECURITY_ENCRYPTION_NETWORK
		SO_SECURITY_ENCRYPTION_TRANSPORT
		SO_SNDBUF SO_SNDLOWAT SO_SNDTIMEO
		SO_STATE SO_TYPE SO_USELOOPBACK SO_XOPEN SO_XSE
		TCP_KEEPALIVE TCP_MAXRT TCP_MAXSEG TCP_NODELAY TCP_STDURG
		UIO_MAXIOV
		),
            {name=>"IPPROTO_IP", type=>"IV", default=>["IV", 0]},
            {name=>"IPPROTO_IPV6", type=>"IV", default=>["IV", 41]},
            {name=>"IPPROTO_RAW", type=>"IV", default=>["IV", 255]},
            {name=>"IPPROTO_ICMP", type=>"IV", default=>["IV", 1]},
            {name=>"IPPROTO_TCP",  type=>"IV", default=>["IV", 6]},
            {name=>"IPPROTO_UDP",  type=>"IV", default=>["IV", 17]},
            {name=>"SHUT_RD", type=>"IV", default=>["IV", "0"]},
            {name=>"SHUT_WR", type=>"IV", default=>["IV", "1"]},
            {name=>"SHUT_RDWR", type=>"IV", default=>["IV", "2"]},
);

push @names,
  {name=>$_, type=>"IV",
   macro=>["#if defined($_) || defined(HAS_$_) /* might be an enum */\n",
           "#endif\n"]}
foreach qw (MSG_CTRUNC MSG_DONTROUTE MSG_OOB MSG_PEEK MSG_PROXY SCM_RIGHTS);

push @names, 
{name => $_, type => "SV",
 pre=>"struct in_addr ip_address; ip_address.s_addr = htonl($_);",
 value => "newSVpvn_flags((char *)&ip_address,sizeof(ip_address), SVs_TEMP)",}
         foreach qw(INADDR_ANY INADDR_LOOPBACK INADDR_NONE INADDR_BROADCAST);

WriteConstants(
    ($] > 5.009002 ? (PROXYSUBS => 1) : ()),
    NAME => 'Socket',
    NAMES => \@names,
);
