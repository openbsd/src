#!/usr/bin/awk -f
#
# $NetBSD: mkoldconf.awk,v 1.3 1995/03/29 22:50:51 ragge Exp $
#

/tmscd/{
	tmsplats[ntms]=$2;
	tmsaddr[ntms]=$5;
	ntms++;
}

/racd/{
	raplats[nra]=$2;
	raaddr[nra]=$5;
	nra++;
}

/decd/{
	deplats[nde]=$2;
	deaddr[nde]=$5;
	nde++;
}

/qecd/{
	qeplats[nqe]=$2;
	qeaddr[nqe]=$5;
	nqe++;
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
printf "int antal_de=%d;\n",nde-1
printf "int antal_uda=%d;\n",nuda-1
printf "int antal_tms=%d;\n",ntms-1
printf "int antal_tmscp=%d;\n",ntmscp-1

printf "extern struct uba_driver udadriver;\n"
printf "extern struct uba_driver dedriver;\n"
printf "extern struct uba_driver qedriver;\n"
if(ntms) printf "extern struct uba_driver tmscpdriver;\n"
if(ntms) printf "int tmscpintr();\n"
printf "int deintr();\n"
printf "int qeintr();\n"
printf "int udaintr();\n"
printf "int udacd=0, racd=0, tmscpcd=0, tmscd=0;\n"
printf "#define C (caddr_t)\n"

printf "struct uba_ctlr ubminit[]={\n"
for(i=1;i<nuda;i++){
	k=sprintf("%d",udaddr[i])
	printf "	{ &udadriver, %d,0,0,udaintr,C %s},\n",
		udaplats[i],loc[k+1]
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
for(i=1;i<nde;i++){
	k=sprintf("%d",deaddr[i])
	printf "	{&dedriver,%d,-1,0,-1,deintr,C %s0,0},\n",deplats[i],
		loc[k+1]
}
for(i=1;i<nqe;i++){
	k=sprintf("%d",qeaddr[i])
	printf "	{&qedriver,%d,-1,0,-1,qeintr,C %s0,0},\n",qeplats[i],
		loc[k+1]
}
for(i=1;i<ntms;i++){
	k=sprintf("%d",tmsaddr[i])
	printf "	{&tmscpdriver,%d,0,'?',0,0,C 0,1,0},\n",tmsplats[i]
}
printf "0};\n"

}

