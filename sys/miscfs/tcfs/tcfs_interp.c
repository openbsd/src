#include<sys/types.h>
#include<sys/malloc.h>
#include<sys/errno.h>
#include "tcfs_keytab.h"
#include "tcfs_cipher.h"

int interp(tcfs_grp_data *,unsigned char *);
void doinverse(void);

static int inverse[]={
      0,1,129,86,193,103,43,147,225,200,180,187,150,178,
      202,120,241,121,100,230,90,49,222,190,75,72,89,238,
      101,195,60,199,249,148,189,235,50,132,115,145,45,163,
      153,6,111,40,95,175,166,21,36,126,173,97,119,243,179,248,
      226,61,30,59,228,102,253,87,74,234,223,149,246,181,25,
      169,66,24,186,247,201,244,151,165,210,96,205,127,3,65,
      184,26,20,209,176,152,216,46,83,53,139,135,18,28,63,5,
      215,164,177,245,188,224,250,44,218,116,124,38,113,134,
      159,54,15,17,158,140,114,220,51,85,255,2,172,206,37,143,
      117,99,240,242,203,98,123,144,219,133,141,39,213,7,33,69,
      12,80,93,42,252,194,229,239,122,118,204,174,211,41,105,
      81,48,237,231,73,192,254,130,52,161,47,92,106,13,56,10,
      71,233,191,88,232,76,11,108,34,23,183,170,4,155,29,198,
      227,196,31,9,78,14,138,160,84,131,221,236,91,82,162,217,
      146,251,104,94,212,112,142,125,207,22,68,109,8,58,197,
      62,156,19,168,185,182,67,35,208,167,27,157,136,16,137,
      55,79,107,70,77,57,32,110,214,154,64,171,128,256
};

union bobbit { 
	unsigned char byte;
	struct {
		unsigned char b1:1;
		unsigned char b2:1;
		unsigned char b3:1;
		unsigned char b4:1;
		unsigned char b5:1;
		unsigned char b6:1;
		unsigned char b7:1;
		unsigned char b8:1;
	} bf;
};

#define mod(a)	((unsigned int)((a)%257))
/*
unsigned int mod(long a) 
{
	return (unsigned int) a%257;
}
*/

int interp(tcfs_grp_data *gd,unsigned char *gidkey)
{
	unsigned int tp,kkk;
	int i=0,j,l,idx;
	tcfs_grp_uinfo ktmp[MAXUSRPERGRP],*gui;
	unsigned int inv,ttt;
	union bobbit obits;
	int k;

	k=gd->gd_k;

	for (i=0;i<MAXUSRPERGRP && i<k;i++)
	{
		gui=&(gd->gd_part[i]);
		if(!IS_SET_GUI(*gui))
			continue;
		ktmp[i].gui_uid=gui->gui_uid;

		for(l=0;l<KEYSIZE/8;l++){
			obits.byte=gui->gui_tcfskey[9*l+8];
			ktmp[i].gui_tcfskey[8*l+0]=mod(obits.bf.b1<<8 | gui->gui_tcfskey[9*l+0]);
			ktmp[i].gui_tcfskey[8*l+1]=mod(obits.bf.b2<<8 | gui->gui_tcfskey[9*l+1]);
			ktmp[i].gui_tcfskey[8*l+2]=mod(obits.bf.b3<<8 | gui->gui_tcfskey[9*l+2]);
			ktmp[i].gui_tcfskey[8*l+3]=mod(obits.bf.b4<<8 | gui->gui_tcfskey[9*l+3]);
			ktmp[i].gui_tcfskey[8*l+4]=mod(obits.bf.b5<<8 | gui->gui_tcfskey[9*l+4]);
			ktmp[i].gui_tcfskey[8*l+5]=mod(obits.bf.b6<<8 | gui->gui_tcfskey[9*l+5]);
			ktmp[i].gui_tcfskey[8*l+6]=mod(obits.bf.b7<<8 | gui->gui_tcfskey[9*l+6]);
			ktmp[i].gui_tcfskey[8*l+7]=mod(obits.bf.b8<<8 | gui->gui_tcfskey[9*l+7]);
		}

		i++;
	}

	for (idx=0;idx<KEYSIZE;idx++) {
		kkk=0;
		for (i=0;i<k;i++) {
			tp=1;
			for (j=0;j<k;j++) {
				if (j!=i) {
					inv = inverse[mod(ktmp[i].gui_uid-ktmp[j].gui_uid)];
					ttt = mod(inv * mod(-ktmp[j].gui_uid));
					tp = mod(tp * ttt);
				}
			}
			tp *= mod(ktmp[i].gui_tcfskey[idx]);
			kkk=(tp+kkk);
		}
		gidkey[idx]=(unsigned char)mod(kkk);
	}
	return 0;
}

void doinverse(void)
{
	int i,j;
	for (i=0;i<257;i++) {
		for (j=0;j<257;j++) {
			if (mod((i*j))==1)
				inverse[i]=j;
		}
	}
}

int tcfs_interp(struct tcfs_mount *mp, tcfs_keytab_node* x) 
{
        void *ks;
        char key[KEYSIZE];

        interp(x->kn_data,key);
	ks=TCFS_INIT_KEY(mp,key);
	if (!ks)
		return ENOMEM;

        x->kn_key=ks;
        return TCFS_OK;
}

