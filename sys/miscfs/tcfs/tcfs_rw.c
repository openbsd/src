#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <miscfs/tcfs/tcfs.h>
#include "tcfs_rw.h"
#include "tcfs_cipher.h"

tcfs_opinfo tcfs_get_opinfo(void *a)
{
	struct vop_read_args *arg;
	tcfs_opinfo r;
	tcfs_fileinfo *i;
	int iscr=0;


	arg=(struct vop_read_args*)a;

	r.i=tcfs_get_fileinfo(a);
	i=&(r.i);

	iscr=FI_CFLAG(i)||FI_GSHAR(i);

	if(!iscr)
		{
		 r.tcfs_op_desc=TCFS_NONE;
		 return r;
		}
		 

	r.off=arg->a_uio->uio_offset;
	r.req=arg->a_uio->uio_resid;

	if(arg->a_uio->uio_rw==UIO_READ)
	{
		if(r.off<FI_ENDOF(i)-FI_SPURE(i))
		{
	 	 r.tcfs_op_desc=TCFS_READ_C1;
	 	 r.out_boff=BOFF(&r);
	 	 r.out_foff=
		      (LAST(&r)>FI_ENDOF(i)-FI_SPURE(i)?FI_ENDOF(i):P_FOFF(&r));
		 r.in_boff=r.out_boff;
		 r.in_foff=r.out_foff;
	 	 return r;
		}
		else
		{
		 r.tcfs_op_desc=TCFS_READ_C2;
		 r.out_boff=0;
		 r.out_foff=0;
		 return r;
		}
	}
	if(arg->a_uio->uio_rw==UIO_WRITE)
	{
	 	if(arg->a_ioflag&IO_APPEND)
		{
			r.off=FI_ENDOF(i)-FI_SPURE(i);
			arg->a_ioflag&=~(IO_APPEND);
		}
		if(LAST(&r)<FI_ENDOF(i)-FI_SPURE(i))
			{
				if (FI_ENDOF(i)-FI_SPURE(i)-LAST(&r)>BLOCKSIZE)
				{
			  	 r.tcfs_op_desc=TCFS_WRITE_C1;
			  	 r.out_boff=BOFF(&r);
			  	 r.out_foff=FOFF(&r);
			 	 r.in_boff=r.out_boff;
			 	 r.in_foff=r.out_foff;
			  	 return r;
				}
				else
				{
				 r.tcfs_op_desc=TCFS_WRITE_C2;
				 r.out_boff=BOFF(&r);
				 r.out_foff=FI_ENDOF(i)-1;
			 	 r.in_boff=r.out_boff;
			 	 r.in_foff=r.out_foff;
				 return r;
				}
			}
		if(LAST(&r)>=FI_ENDOF(i)-FI_SPURE(i))
			{
			  	r.out_boff=BOFF(&r);
			  	r.out_foff=P_FOFF(&r);
			  	if (r.off<=FI_ENDOF(i)-FI_SPURE(i))
			  	{
			  	 r.tcfs_op_desc=TCFS_WRITE_C3;
		 		 r.in_boff=r.out_boff;
		 		 r.in_foff=r.out_foff;
			  	 return r;
			 	}
				else
			  	 if(D_BOFF(FI_ENDOF(i))==D_BOFF(r.off))
					{
					 r.tcfs_op_desc=TCFS_WRITE_C4;
		 			 r.in_boff=r.out_boff;
		 			 r.in_foff=r.out_foff;
					 return r;
					}
			  	 else
					{    
				 	 r.tcfs_op_desc=TCFS_WRITE_C5;
				  	 r.in_boff=D_BOFF(FI_ENDOF(i));
				 	 r.in_foff=D_FOFF(FI_ENDOF(i));
					 r.out_boff=BOFF(&r);
					 r.out_foff=P_FOFF(&r);
				 	 return r;
					}
			}
	}
	return r;
}

char *tcfs_new_uio_obs(struct uio *old, struct uio **new, int off, int bufsize)
{
	char *buffer;   
        struct uio *u;  
        struct iovec *n;

        u=malloc(sizeof(struct uio),M_FREE,M_NOWAIT);
	if(!u)
		return (char*)0;
        n=malloc(sizeof(struct iovec),M_FREE,M_NOWAIT);
	if(!n)
		{
		 free(u,M_FREE);
		 return (char*)0;
		}
        buffer=malloc(bufsize,M_FREE,M_NOWAIT);
	if(!buffer)
		{
		 free(u,M_FREE);
		 free(n,M_FREE);
		 return (char*)0;
		}

      
        u->uio_offset=off;
        u->uio_resid=bufsize;
        u->uio_iovcnt=1;
        u->uio_segflg=UIO_SYSSPACE;
        u->uio_rw=old->uio_rw;
        u->uio_procp=old->uio_procp;
        u->uio_iov=n;
        n->iov_base=buffer;
        n->iov_len=bufsize;

      
        *new=u;
        return buffer;
}

char *tcfs_new_uio_i(struct uio *old, struct uio **new,tcfs_opinfo *r)
{
        int bufsize;
        char *buffer=NULL;

	
	switch(r->tcfs_op_desc)
	{
		case TCFS_READ_C2:
			buffer=(char*)0;
			return buffer;
		default:
		case TCFS_READ_C1:
		case TCFS_WRITE_C1:
		case TCFS_WRITE_C2:
		case TCFS_WRITE_C3:
			bufsize=r->in_foff-r->in_boff+1;
		        break;
	}
	
	return tcfs_new_uio_obs(old,new,r->in_boff,bufsize);
}


void    tcfs_dispose_new_uio(struct uio *vec)
{
        void *p,*q;
        p=(void*)vec;
        q=(void*)vec->uio_iov;

        free(p,M_FREE);
        free(q,M_FREE);
}

int
tcfs_read(v)
        void *v;
{
        char    *buffer,        /* buffer per la read interna 		*/
                *buffp,         /* puntatore all'inizio dei dati utili 	*/
                *buffpf;        /* puntatore alla fine dei dati utili 	*/

	int	bufsize=0;	/* taglia del buffer allocato		*/

        struct uio
                *new,           /* puntatore all'uio rifatto    */
                *old;           /* puntatore all'uio originale  */

        int     terr,           /* val. ritorno read interna            */
                chdap,          /* caratteri da passare                 */
                chread,		/* caratteri letti dalla read int.      */
		r_off,		/* offset raggiunto			*/
		e_spure;	/* spure effettivamente letti		*/

        struct vop_read_args *arg;
        tcfs_opinfo f;
	tcfs_fileinfo *i;

	struct ucred *ucred;
	struct proc  *procp;

	struct tcfs_mount *mp;
	void *ks;


	f=tcfs_get_opinfo(v);

	if(f.tcfs_op_desc==TCFS_NONE)
		return tcfs_bypass(v);

	i=&(f.i);

        arg=(struct vop_read_args *)v;
	mp=MOUNTTOTCFSMOUNT(arg->a_vp->v_mount);
	ucred=arg->a_cred;
	procp=arg->a_uio->uio_procp;

	if(FI_GSHAR(i))
		{
		 ks=tcfs_getgkey(ucred,procp,arg->a_vp);
		 if(!ks)
			{
			return EACCES;
			}
		}
	else
		{
		 ks=tcfs_getpkey(ucred,procp,arg->a_vp);
		 if (!ks)
		 	 ks=tcfs_getukey(ucred,procp,arg->a_vp);
		 if (!ks)
		 	 return EACCES;
		}

        old=arg->a_uio;
        buffer=tcfs_new_uio_i(old,&new,&f);

	if (buffer==(char*)0)
		 return 0; 

        arg->a_uio=new;
	bufsize=f.out_foff-f.out_boff+1;

        terr=tcfs_bypass(arg);
        chread=bufsize-new->uio_resid;
	

	mkdecrypt(mp,buffer,chread,ks);


	r_off=f.out_boff+chread;

	if(r_off>=FI_ENDOF(i))
		e_spure=FI_SPURE(i);
	else
		if(r_off>FI_ENDOF(i)-FI_SPURE(i))
			e_spure=FI_SPURE(i)-FI_ENDOF(i)+r_off;
		else
			e_spure=0;

        buffp=buffer+ROFF(&f);
        if (chread>ROFF(&f))
        {
		chdap=MIN(f.req,chread-ROFF(&f)-e_spure);
                buffpf=buffp+chdap;

	  uiomove(buffp,chdap,old);
        }
        arg->a_uio=old;
        tcfs_dispose_new_uio(new);
	free(buffer,M_FREE);
	
        return terr;
}

int
tcfs_write(v)
	void *v;
{
        char    *buffer,        /* buffer per la read interna */
                *buffp,         /* puntatore all'inizio dei dati utili */
                *buffpf;        /* puntatore alla fine dei dati utili */

        struct uio
                *tmp,           /* uio per la dimensione di un blocco */
                *old;           /* puntatore all'uio originale della chiamata */


        int     bufsize,        /* num. caratteri req. read interna     */
                terr,           /* val. ritorno read interna            */
                chdap,          /* caratteri da passare                 */
                chread=0,       /* caratteri letti dalla read int.      */
		chwrote,e_chwrote,
		spure=0;		/* ho scritto fino a qui		*/

        tcfs_opinfo f;
	tcfs_fileinfo *i;

        struct vop_read_args *arg;
	struct ucred *ucred;
	struct proc  *procp;
	void *ks;
	struct tcfs_mount *mp;

        int e;


        arg=(struct vop_read_args *)v;
        ucred=arg->a_cred;
        procp=arg->a_uio->uio_procp;
	mp=MOUNTTOTCFSMOUNT(arg->a_vp->v_mount);

	
	f=tcfs_get_opinfo(arg);

	if (f.tcfs_op_desc==TCFS_NONE)
		return tcfs_bypass(v);

	i=&(f.i);

	if(FI_GSHAR(i))
	{
		ks=tcfs_getgkey(ucred,procp,arg->a_vp);
		if(!ks)
			return EACCES;
	}
	else
	{
        	ks=tcfs_getpkey(ucred,procp,arg->a_vp);
        	if (!ks)
       			ks=tcfs_getukey(ucred,procp,arg->a_vp);
        	if (!ks)
               	 	return EACCES;
	}

        old=arg->a_uio;

        buffer=(char*)tcfs_new_uio_i(old,&tmp,&f);
	if (buffer==(char*)0)
		return EFAULT;

        arg->a_uio=tmp;
        arg->a_desc=VDESC(vop_read);
        arg->a_uio->uio_rw=UIO_READ;

	bufsize=f.in_foff-f.in_boff+1;



        terr=tcfs_bypass(arg);

	switch(f.tcfs_op_desc)
	{
		case TCFS_WRITE_C1:
		case TCFS_WRITE_C2:
			if(tmp->uio_resid!=0)
				goto ret;

        		chread=bufsize;
			break;

		case TCFS_WRITE_C3:
		case TCFS_WRITE_C4:
			if(tmp->uio_resid!=(P_FOFF(&f)-FI_ENDOF(i)+1))
				goto ret;
			
			chread=bufsize;
			break;

		case TCFS_WRITE_C5:
			if(tmp->uio_resid!=(D_FOFF(FI_ENDOF(i))-FI_ENDOF(i)+1))
					 goto ret;
			chread=bufsize;
	}

        mkdecrypt(mp,buffer,chread,ks);


	if(f.tcfs_op_desc==TCFS_WRITE_C4)
	{

			for(e=FI_ENDOF(i)-FI_SPURE(i);e<f.off;e++)
				*(buffer+e-f.out_boff)='\0';

	}


	if(f.tcfs_op_desc!=TCFS_WRITE_C5) 
	{

       	 buffp=buffer+ROFF(&f);
       	 chdap=f.req;
       	 buffpf=buffp+chdap;
	
	uiomove(buffp,chdap,old);

	} /*if not TCFS_WRITE_C5 */
	else
	{
		for(e=FI_ENDOF(i)-FI_SPURE(i)-f.in_boff;e<bufsize;e++)
			 *(buffer+e)='\0';
	 }
		
       	 mkencrypt(mp,buffer,bufsize,ks);
	
        arg->a_desc=VDESC(vop_write);
        arg->a_uio->uio_rw=UIO_WRITE;
        arg->a_uio->uio_resid=bufsize;
        arg->a_uio->uio_offset=f.in_boff;
        arg->a_uio->uio_iov->iov_base=buffer;
        arg->a_uio->uio_iov->iov_len=bufsize;
        terr=tcfs_bypass(arg);

	if (f.tcfs_op_desc==TCFS_WRITE_C5)
	{

        	tcfs_dispose_new_uio(tmp);
		free(buffer,M_FREE);

		bufsize=f.out_foff-f.out_boff+1;
		buffer=tcfs_new_uio_obs(old,&tmp,f.out_boff,bufsize);
		arg->a_uio=tmp;
        	arg->a_desc=VDESC(vop_write);
        	arg->a_uio->uio_rw=UIO_WRITE;

                for(e=0;e<ROFF(&f);e++)
                         *(buffer+e)='\0';

         	buffp=buffer+ROFF(&f);
         	chdap=f.req;
         	buffpf=buffp+chdap;

		uiomove(buffp,chdap,old);
       	 	mkencrypt(mp,buffer,bufsize,ks);
		terr=tcfs_bypass(arg);

	}

	chwrote=bufsize-tmp->uio_resid;
	e_chwrote=MIN(f.req,chwrote-ROFF(&f));

        switch(f.tcfs_op_desc)
        {
         case TCFS_WRITE_C1:
         case TCFS_WRITE_C2:
                break;
         case TCFS_WRITE_C3:
		if ((f.in_boff+chwrote)>FI_ENDOF(i)-FI_SPURE(i))
		{


		if(chwrote>f.req){
			spure=D_SPURE(f.off+f.req);
		} else
		{
			spure=D_SPURE(f.out_boff+chwrote);
		}

			FI_SET_SP(i,spure);
			tcfs_set_fileinfo(v,i);
		}
		break;
	 	case TCFS_WRITE_C4:
	 	case TCFS_WRITE_C5:
			spure=D_SPURE(f.off+f.req);
                	FI_SET_SP(i,spure);
                	tcfs_set_fileinfo(v,i);
		break;

        }


        old->uio_resid=f.req-e_chwrote;

ret:
        arg->a_uio=old;
        tcfs_dispose_new_uio(tmp);
	free(buffer,M_FREE);
        return terr;
}

int tcfs_ed(struct vnode *v,struct proc *p,struct ucred *c, tcfs_fileinfo *i)
{
	struct vop_read_args ra;
	struct vop_write_args wa;
	struct uio *u;
	struct iovec *n;
	char *buff;
	unsigned long  csize,resid,w_resid;
	unsigned long  size,w_size;
	int bufsize,e;
	int retval,sp;
	void *ks;
	int encr=0;
	struct tcfs_mount *mp;

	mp=MOUNTTOTCFSMOUNT(v->v_mount);
	encr=FI_CFLAG(i)||FI_GSHAR(i);
	

	if(v->v_type!=VREG)
		return 0;

	if(FI_GSHAR(i))
	{
		ks=tcfs_getgkey(c,p,v);
		if(!ks)
			return EACCES;
	}	
	else
	{
	
        	ks=tcfs_getpkey(c,p,v);
        	if (!ks)
        		ks=tcfs_getukey(c,p,v);
        	if (!ks)
			{
                	return EACCES;
			}

	}

	u=malloc(sizeof(struct uio),M_FREE,M_NOWAIT);
	n=malloc(sizeof(struct iovec),M_FREE,M_NOWAIT);
	

	size=FI_ENDOF(i);

	if(encr)
		{
		 w_size=D_PFOFF(size);
		 sp=D_SPURE(size);
		}
	else
		{
		 w_size=size-FI_SPURE(i);
		 sp=-FI_SPURE(i);
		}

	csize=0;
	resid=size;
	w_resid=w_size;

	bufsize=BLOCKSIZE;

	buff=malloc(BLOCKSIZE,M_FREE,M_NOWAIT);

	u->uio_offset=0;
	u->uio_resid=bufsize;
	u->uio_iovcnt=1;
	u->uio_segflg=UIO_SYSSPACE;
	u->uio_procp=p;
	u->uio_iov=n;
	
	wa.a_desc=VDESC(vop_write);
	ra.a_desc=VDESC(vop_read);
	wa.a_vp=ra.a_vp=v;
	wa.a_cred=ra.a_cred=c;
	wa.a_ioflag=ra.a_ioflag=0;
	wa.a_uio=ra.a_uio=u;

	
	for(e=0;e<=D_NOBLK(size);e++)
	{

		int x,y; 

		u->uio_offset=csize;
		u->uio_rw=UIO_READ;
		n->iov_base=buff;
		n->iov_len=u->uio_resid=x=MIN(bufsize,resid+1);
		
 		retval=tcfs_bypass((void*)&ra);


		u->uio_offset=csize;
		u->uio_rw=UIO_WRITE;
		n->iov_base=buff;
		n->iov_len=u->uio_resid=y=MIN(bufsize,w_resid+1);

		if(!encr)
			 mkdecrypt(mp,buff,x,ks);
		else
			mkencrypt(mp,buff,y,ks);
		
		retval=tcfs_bypass((void*)&wa);

		resid-=x; csize+=x;
		w_resid-=y;

		/* I should call the scheduler here */
	}

	tcfs_dispose_new_uio(u);
	return sp;
}

