/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
/*
 * This header file is constructed to match the version described by
 * PCAP_VERSION_MAJ.
 *
 * The structure largely derives from libpcap which wouldn't include
 * nicely without bpf.
 */
typedef	struct	pcap_filehdr	{
	u_int	pc_id;
	u_short	pc_v_maj;
	u_short	pc_v_min;
	u_int	pc_zone;
	u_int	pc_sigfigs;
	u_int	pc_slen;
	u_int	pc_type;
} pcaphdr_t;

#define	PCAP_VERSION_MAJ	2

typedef	struct	pcap_pkthdr	{
	struct	timeval	ph_ts;
	u_int	ph_clen;
	u_int	ph_len;
} pcappkt_t;

