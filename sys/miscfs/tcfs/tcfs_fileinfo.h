/*	Gestione informazioni sui files cifrati
*/


typedef struct {	
		 unsigned long flag;
		 unsigned int end_of_file;
		} tcfs_fileinfo;

#define		MBFLAG	0x00000010
#define		SPFLAG	0x000000e0
#define		GSFLAG	0x00000100

#define	FI_CFLAG(x)	(((x)->flag&MBFLAG)>>4)
#define	FI_SPURE(x)	(((x)->flag&SPFLAG)>>5)
#define	FI_GSHAR(x)	(((x)->flag&GSFLAG)>>8)
#define FI_ENDOF(x)	((x)->end_of_file)

#define	FI_SET_CF(x,y)	((x)->flag=\
			 ((x)->flag & (~MBFLAG))|((y<<4)&MBFLAG))

#define	FI_SET_SP(x,y)	((x)->flag=\
			 ((x)->flag & (~SPFLAG))|((y<<5)&SPFLAG))

#define	FI_SET_GS(x,y)	((x)->flag=\
			 ((x)->flag & (~GSFLAG))|((y<<8)&GSFLAG))

/*	prototipi	*/

tcfs_fileinfo 	tcfs_get_fileinfo(void *);
tcfs_fileinfo 	tcfs_xgetflags(struct vnode *,struct proc *,struct ucred*);
int 		tcfs_set_fileinfo(void *, tcfs_fileinfo *);
int 		tcfs_xsetflags(struct vnode *, struct proc *, struct ucred *, tcfs_fileinfo *);

