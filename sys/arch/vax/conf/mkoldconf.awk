#!/usr/bin/awk -f
#
# $NetBSD: mkoldconf.awk,v 1.4 1995/12/16 15:05:03 ragge Exp $
#

/tmscd/{
	tmsplats[ntms]=$2;
	tmsaddr[ntms]=$5;
	ntms++;
}

/tscd/{
	tsplats[nts]=$2;
	tsaddr[nts]=$5;
	nts++;
}

/racd/{
	raplats[nra]=$2;
	raaddr[nra]=$5;
	nra++;
}

{
	if(savenext==1){
		l=sprintf("%d",$2)
		udanummer[l-1]=nuda-1
		savenext=0;
	}
}


{
	if(tmssavenext==1){
		l=sprintf("%d",$2)
		tmsnummer[l-1]=ntmscp-1
		tmssavenext=0;
	}
	if(tssavenext==1){
		l=sprintf("%d",$2)
		tsnummer[l-1]=nts-1
		tssavenext=0;
	}
}

/tmscpcd/{
	tmscpplats[ntmscp]=$2;
	tmscpaddr[ntmscp]=$5;
	ntmscp++;
	tmssavenext=1;
}
		
/udacd/{
	udaplats[nuda]=$2;
	udaddr[nuda]=$5;
	nuda++;
	savenext=1;
}
		

/};/{
	k=0;
	m=0;
}

{
	if (k==1){
		for(i=1;i<NF+1;i++){
			loc[loccnt+i]=$i;
		}
		loccnt+=NF;
	}
}

/static int loc/{
	k=1;
	loccnt=0;
}

{
	if(m==1){
		for(i=1;i<NF+1;i++){
			pv[i]=$i;
		}
	}
}

/static short pv/{
	m=1;
}

END{

printf "#include \"sys/param.h\"\n"
printf "#include \"machine/pte.h\"\n"
printf "#include \"sys/buf.h\"\n"
printf "#include \"sys/map.h\"\n"

printf "#include \"vax/uba/ubavar.h\"\n"

printf "int antal_ra=%d;\n",nra-1
printf "int antal_uda=%d;\n",nuda-1
printf "int antal_ts=%d;\n",nts-1
printf "int antal_tms=%d;\n",ntms-1
printf "int antal_tmscp=%d;\n",ntmscp-1

printf "extern struct uba_driver udadriver;\n"
if(nts) printf "extern struct uba_driver tsdriver;\n"
if(nts) printf "void tsintr();\n"
if(ntms) printf "extern struct uba_driver tmscpdriver;\n"
if(ntms) printf "void tmscpintr();\n"
printf "void udaintr();\n"
printf "int racd=0, tmscd=0;\n"
printf "#define C (caddr_t)\n"

printf "struct uba_ctlr ubminit[]={\n"
for(i=1;i<nuda;i++){
	k=sprintf("%d",udaddr[i])
	printf "	{ &udadriver, %d,0,0,udaintr,C %s},\n",
		udaplats[i],loc[k+1]
}
for(i=1;i<nts;i++){
	k=sprintf("%d",tsaddr[i])
if(nts)printf "        { &tsdriver, %d,'?',0,tsintr,C %s},\n",
	tsplats[i],loc[k+1]
}
for(i=1;i<ntmscp;i++){
	k=sprintf("%d",tmscpaddr[i])
if(ntms)printf "        { &tmscpdriver, %d,'?',0,tmscpintr,C %s},\n",
	tmscpplats[i],loc[k+1]
}
printf "0};\n"

printf "struct uba_device ubdinit[]={\n"
for(i=1;i<nra;i++){
	k=sprintf("%d",raaddr[i])
	printf "	{ &udadriver,%d,%d,0,%d,0,0,1,0},\n",raplats[i],
		rr++/4,loc[k+1]
}
for(i=1;i<nts;i++){
	k=sprintf("%d",tsaddr[i])
	printf "	{&tsdriver,%d,0,'?',0,0,C 0,1,0},\n",tsplats[i]
}
for(i=1;i<ntms;i++){
	k=sprintf("%d",tmsaddr[i])
	printf "	{&tmscpdriver,%d,0,'?',0,0,C 0,1,0},\n",tmsplats[i]
}
printf "0};\n"

}

