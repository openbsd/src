
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

#include "form.priv.h"

/* this can't be readonly */
static FIELD default_field = {
  0,                       /* status */
  0,                       /* rows   */
  0,                       /* cols   */
  0,                       /* frow   */
  0,                       /* fcol   */
  0,                       /* drows  */
  0,                       /* dcols  */
  0,                       /* maxgrow*/
  0,                       /* nrow   */
  0,                       /* nbuf   */
  NO_JUSTIFICATION,        /* just   */
  0,                       /* page   */
  0,                       /* index  */
  (int)' ',                /* pad    */
  A_NORMAL,                /* fore   */
  A_NORMAL,                /* back   */
  ALL_FIELD_OPTS,          /* opts   */
  (FIELD *)0,              /* snext  */
  (FIELD *)0,              /* sprev  */
  (FIELD *)0,              /* link   */
  (FORM *)0,               /* form   */
  (FIELDTYPE *)0,          /* type   */
  (char *)0,               /* arg    */ 
  (char *)0,               /* buf    */
  (char *)0                /* usrptr */
};

FIELD *_nc_Default_Field = &default_field;

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static TypeArgument *Make_Argument(
|                              const FIELDTYPE *typ,
|                              va_list *ap,
|                              int *err )
|   
|   Description   :  Create an argument structure for the specified type.
|                    Use the type-dependant argument list to construct
|                    it.
|
|   Return Values :  Pointer to argument structure. Maybe NULL.
|                    In case of an error in *err an errorcounter is increased. 
+--------------------------------------------------------------------------*/
static TypeArgument* Make_Argument(const FIELDTYPE *typ, va_list *ap, int *err)
{
  TypeArgument *res = (TypeArgument *)0; 
  TypeArgument *p;

  if (typ && (typ->status & _HAS_ARGS))
    {
      assert(err && ap);
      if (typ->status & _LINKED_TYPE)
	{
	  p = (TypeArgument *)malloc(sizeof(TypeArgument));
	  if (p) 
	    {
	      p->left  = Make_Argument(typ->left ,ap,err);
	      p->right = Make_Argument(typ->right,ap,err);
	      return p;
	    }
	  else
	    *err += 1;
      } else 
	{
	  if ( !(res=(TypeArgument *)typ->makearg(ap)) ) 
	    *err += 1;
	}
    }
  return res;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static TypeArgument *Copy_Argument(
|                              const FIELDTYPE *typ,
|                              const TypeArgument *argp,
|                              int *err )
|   
|   Description   :  Create a copy of an argument structure for the specified 
|                    type.
|
|   Return Values :  Pointer to argument structure. Maybe NULL.
|                    In case of an error in *err an errorcounter is increased. 
+--------------------------------------------------------------------------*/
static TypeArgument *Copy_Argument(const FIELDTYPE *typ,
				   const TypeArgument *argp, int *err)
{
  TypeArgument *res = (TypeArgument *)0;
  TypeArgument *p;

  if ( typ && (typ->status & _HAS_ARGS) )
    {
      assert(err && argp);
      if (typ->status & _LINKED_TYPE)
	{
	  p = (TypeArgument *)malloc(sizeof(TypeArgument));
	  if (p)
	    {
	      p->left  = Copy_Argument(typ,argp->left ,err);
	      p->right = Copy_Argument(typ,argp->right,err);
	      return p;
	    }
	  *err += 1;
      } 
      else 
	{
	  if (!(res = (TypeArgument *)(typ->copyarg((void *)argp)))) 
	    *err += 1;
	}
    }
  return res;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void Free_Argument(
|                                  const FIELDTYPE *typ,
|                                  TypeArgument * argp )
|   
|   Description   :  Release memory associated with the argument structure
|                    for the given fieldtype.
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
static void Free_Argument(const FIELDTYPE * typ, TypeArgument * argp)
{
  if (!typ || !(typ->status & _HAS_ARGS)) 
    return;
  
  if (typ->status & _LINKED_TYPE)
    {
      assert(argp);
      Free_Argument(typ->left ,argp->left );
      Free_Argument(typ->right,argp->right);
      free(argp);
    } 
  else 
    {
      typ->freearg((void *)argp);
    }
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Copy_Type( FIELD *new, FIELD const *old )
|   
|   Description   :  Copy argument structure of field old to field new
|
|   Return Values :  TRUE       - copy worked
|                    FALSE      - error occured
+--------------------------------------------------------------------------*/
static bool Copy_Type(FIELD *new, FIELD const *old)
{
  int err = 0;

  assert(new && old);

  new->type = old->type;
  new->arg  = (void *)Copy_Argument(old->type,(TypeArgument *)(old->arg),&err);

  if (err)
    {
      Free_Argument(new->type,(TypeArgument *)(new->arg));
      new->type = (FIELDTYPE *)0;
      new->arg  = (void *)0;
      return FALSE;
    }
  else
    {
      if (new->type) 
	new->type->ref++;
      return TRUE;
    }
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void Free_Type( FIELD *field )
|   
|   Description   :  Release Argument structure for this field
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
INLINE static void Free_Type(FIELD *field)
{
  assert(field);
  if (field->type) 
    field->type->ref--;
  Free_Argument(field->type,(TypeArgument *)(field->arg));
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELD *new_field( int rows, int cols, 
|                                      int frow, int fcol,
|                                      int nrow, int nbuf )
|   
|   Description   :  Create a new field with this many 'rows' and 'cols',
|                    starting at 'frow/fcol' in the subwindow of the form.
|                    Allocate 'nrow' off-screen rows and 'nbuf' additional
|                    buffers. If an error occurs, errno is set to
|                    
|                    E_BAD_ARGUMENT - invalid argument
|                    E_SYSTEM_ERROR - system error
|
|   Return Values :  Pointer to the new field or NULL if failure.
+--------------------------------------------------------------------------*/
FIELD *new_field(int rows, int cols, int frow, int fcol, int nrow, int nbuf)
{
  FIELD *New_Field = (FIELD *)0;
  int err = E_BAD_ARGUMENT;

  if (rows>0  && 
      cols>0  && 
      frow>=0 && 
      fcol>=0 && 
      nrow>=0 && 
      nbuf>=0 &&
      ((err = E_SYSTEM_ERROR) != 0) && /* trick: this resets the default error */
      (New_Field=(FIELD *)malloc(sizeof(FIELD))) )
    {
      *New_Field       = default_field;
      New_Field->rows  = rows;
      New_Field->cols  = cols;
      New_Field->drows = rows + nrow;
      New_Field->dcols = cols;
      New_Field->frow  = frow;
      New_Field->fcol  = fcol;
      New_Field->nrow  = nrow;
      New_Field->nbuf  = nbuf;
      New_Field->link  = New_Field;

      if (Copy_Type(New_Field,&default_field))
	{
	  size_t len;

	  len = Total_Buffer_Size(New_Field);
	  if ((New_Field->buf = (char *)malloc(len)))
	    {
	      /* Prefill buffers with blanks and insert terminating zeroes
		 between buffers */
	      int i;

	      memset(New_Field->buf,' ',len);
	      for(i=0;i<=New_Field->nbuf;i++)
		{
		  New_Field->buf[(New_Field->drows*New_Field->cols+1)*(i+1)-1]
		    = '\0';
		}
	      return New_Field;
	    }
	}
    }

  if (New_Field) 
    free_field(New_Field);
  
  SET_ERROR( err );
  return (FIELD *)0;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELD *dup_field(FIELD *field, int frow, int fcol)
|   
|   Description   :  Duplicates the field at the specified position. All
|                    field attributes and the buffers are copied.
|                    If an error occurs, errno is set to
|                    
|                    E_BAD_ARGUMENT - invalid argument
|                    E_SYSTEM_ERROR - system error
|
|   Return Values :  Pointer to the new field or NULL if failure
+--------------------------------------------------------------------------*/
FIELD *dup_field(FIELD * field, int frow, int fcol)
{
  FIELD *New_Field = (FIELD *)0;
  int err = E_BAD_ARGUMENT;

  if (field && (frow>=0) && (fcol>=0) && 
      ((err=E_SYSTEM_ERROR) != 0) && /* trick : this resets the default error */
      (New_Field=(FIELD *)malloc(sizeof(FIELD))) )
    {
      *New_Field         = default_field;
      New_Field->frow    = frow;
      New_Field->fcol    = fcol;
      New_Field->link    = New_Field;
      New_Field->rows    = field->rows;
      New_Field->cols    = field->cols;
      New_Field->nrow    = field->nrow;
      New_Field->drows   = field->drows;
      New_Field->dcols   = field->dcols;
      New_Field->maxgrow = field->maxgrow;
      New_Field->nbuf    = field->nbuf;
      New_Field->just    = field->just;
      New_Field->fore    = field->fore;
      New_Field->back    = field->back;
      New_Field->pad     = field->pad;
      New_Field->opts    = field->opts;
      New_Field->usrptr  = field->usrptr;

      if (Copy_Type(New_Field,field))
	{
	  size_t len;

	  len = Total_Buffer_Size(New_Field);
	  if ( (New_Field->buf=(char *)malloc(len)) )
	    {
	      memcpy(New_Field->buf,field->buf,len);
	      return New_Field;
	    }
	}
    }

  if (New_Field) 
    free_field(New_Field);

  SET_ERROR(err);
  return (FIELD *)0;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELD *link_field(FIELD *field, int frow, int fcol)  
|   
|   Description   :  Duplicates the field at the specified position. The
|                    new field shares its buffers with the original one,
|                    the attributes are independent.
|                    If an error occurs, errno is set to
|                    
|                    E_BAD_ARGUMENT - invalid argument
|                    E_SYSTEM_ERROR - system error
|
|   Return Values :  Pointer to the new field or NULL if failure
+--------------------------------------------------------------------------*/
FIELD *link_field(FIELD * field, int frow, int fcol)
{
  FIELD *New_Field = (FIELD *)0;
  int err = E_BAD_ARGUMENT;

  if (field && (frow>=0) && (fcol>=0) &&
      ((err=E_SYSTEM_ERROR) != 0) && /* trick: this resets the default error */
      (New_Field = (FIELD *)malloc(sizeof(FIELD))) )
    {
      *New_Field        = default_field;
      New_Field->frow   = frow;
      New_Field->fcol   = fcol;
      New_Field->link   = field->link;
      field->link       = New_Field;
      New_Field->buf    = field->buf;
      New_Field->rows   = field->rows;
      New_Field->cols   = field->cols;
      New_Field->nrow   = field->nrow;
      New_Field->nbuf   = field->nbuf;
      New_Field->drows  = field->drows;
      New_Field->dcols  = field->dcols;
      New_Field->maxgrow= field->maxgrow;
      New_Field->just   = field->just;
      New_Field->fore   = field->fore;
      New_Field->back   = field->back;
      New_Field->pad    = field->pad;
      New_Field->opts   = field->opts;
      New_Field->usrptr = field->usrptr;
      if (Copy_Type(New_Field,field)) 
	return New_Field;
    }

  if (New_Field) 
    free_field(New_Field);

  SET_ERROR( err );
  return (FIELD *)0;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int free_field( FIELD *field )
|   
|   Description   :  Frees the storage allocated for the field.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid field pointer
|                    E_CONNECTED    - field is connected
+--------------------------------------------------------------------------*/
int free_field(FIELD * field)
{
  if (!field) 
    RETURN(E_BAD_ARGUMENT);

  if (field->form)
    RETURN(E_CONNECTED);
  
  if (field == field->link)
    {
      if (field->buf) 
	free(field->buf);
    }
  else 
    {
      FIELD *f;

      for(f=field;f->link != field;f = f->link) 
	{}
      f->link = field->link;
    }
  Free_Type(field);
  free(field);
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int field_info(const FIELD *field,
|                                   int *rows, int *cols,
|                                   int *frow, int *fcol,
|                                   int *nrow, int *nbuf)
|   
|   Description   :  Retrieve infos about the fields creation parameters.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid field pointer
+--------------------------------------------------------------------------*/
int field_info(const FIELD *field,
	       int *rows, int *cols, 
	       int *frow, int *fcol, 
	       int *nrow, int *nbuf)
{
  if (!field) 
    RETURN(E_BAD_ARGUMENT);

  if (rows) *rows = field->rows;
  if (cols) *cols = field->cols;
  if (frow) *frow = field->frow;
  if (fcol) *fcol = field->fcol;
  if (nrow) *nrow = field->nrow;
  if (nbuf) *nbuf = field->nbuf;
  RETURN(E_OK);
}
	
/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int move_field(FIELD *field,int frow, int fcol)
|   
|   Description   :  Moves the disconnected field to the new location in
|                    the forms subwindow.
|
|   Return Values :  E_OK            - success
|                    E_BAD_ARGUMENT  - invalid argument passed
|                    E_CONNECTED     - field is connected
+--------------------------------------------------------------------------*/
int move_field(FIELD *field, int frow, int fcol)
{
  if ( !field || (frow<0) || (fcol<0) ) 
    RETURN(E_BAD_ARGUMENT);

  if (field->form) 
    RETURN(E_CONNECTED);

  field->frow = frow;
  field->fcol = fcol;
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_type(FIELD *field, FIELDTYPE *type,...)
|   
|   Description   :  Associate the specified fieldtype with the field.
|                    Certain field types take additional arguments. Look
|                    at the spec of the field types !
|
|   Return Values :  E_OK           - success
|                    E_SYSTEM_ERROR - system error
+--------------------------------------------------------------------------*/
int set_field_type(FIELD *field,FIELDTYPE *type, ...)
{
  va_list ap;
  int res = E_SYSTEM_ERROR;
  int err = 0;

  va_start(ap,type);

  Normalize_Field(field);
  Free_Type(field);

  field->type = type;
  field->arg  = (void *)Make_Argument(field->type,&ap,&err);

  if (err)
    {
      Free_Argument(field->type,(TypeArgument *)(field->arg));
      field->type = (FIELDTYPE *)0;
      field->arg  = (void *)0;
    }
  else
    {
      res = E_OK;
      if (field->type) 
	field->type->ref++;
    }

  va_end(ap);
  RETURN(res);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELDTYPE *field_type(const FIELD *field)
|   
|   Description   :  Retrieve the associated fieldtype for this field.
|
|   Return Values :  Pointer to fieldtype of NULL if none is defined.
+--------------------------------------------------------------------------*/
FIELDTYPE *field_type(const FIELD * field)
{
  return Normalize_Field(field)->type;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  void *field_arg(const FIELD *field)
|   
|   Description   :  Retrieve pointer to the fields argument structure.
|
|   Return Values :  Pointer to structure or NULL if none is defined.
+--------------------------------------------------------------------------*/
void *field_arg(const FIELD * field)
{
  return Normalize_Field(field)->arg;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_max_field(FIELD *field, int maxgrow)
|   
|   Description   :  Set the maximum growth for a dynamic field. If maxgrow=0
|                    the field may grow to any possible size.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid argument
+--------------------------------------------------------------------------*/
int set_max_field(FIELD *field, int maxgrow)
{
  if (!field || (maxgrow<0))
    RETURN(E_BAD_ARGUMENT);
  else
    {
      bool single_line_field = Single_Line_Field(field);

      if (maxgrow>0)
	{
	  if (( single_line_field && (maxgrow < field->dcols)) ||
	      (!single_line_field && (maxgrow < field->drows)))
	    RETURN(E_BAD_ARGUMENT);
	}
      field->maxgrow = maxgrow;
      field->status &= ~_MAY_GROW;
      if (!(field->opts & O_STATIC))
	{
	  if ((maxgrow==0) ||
	      ( single_line_field && (field->dcols < maxgrow)) ||
	      (!single_line_field && (field->drows < maxgrow)))
	    field->status |= _MAY_GROW;
	}
    }
  RETURN(E_OK);
}
		  
/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int dynamic_field_info(const FIELD *field,
|                                           int *drows, int *dcols,
|                                           int *maxgrow)
|   
|   Description   :  Retrieve informations about a dynamic fields current
|                    dynamic parameters.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid argument
+--------------------------------------------------------------------------*/
int dynamic_field_info(const FIELD *field,
		       int *drows, int *dcols, int *maxgrow)
{
  if (!field)
    RETURN(E_BAD_ARGUMENT);

  if (drows)   *drows   = field->drows;
  if (dcols)   *dcols   = field->dcols;
  if (maxgrow) *maxgrow = field->maxgrow;

  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_new_page(FIELD *field, bool new_page_flag)
|   
|   Description   :  Marks the field as the beginning of a new page of 
|                    the form.
|
|   Return Values :  E_OK         - success
|                    E_CONNECTED  - field is connected
+--------------------------------------------------------------------------*/
int set_new_page(FIELD * field, bool new_page_flag)
{
  Normalize_Field(field);
  if (field->form) 
    RETURN(E_CONNECTED);

  if (new_page_flag) 
    field->status |= _NEWPAGE;
  else
    field->status &= ~_NEWPAGE;

  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  bool new_page(const FIELD *field)
|   
|   Description   :  Retrieve the info whether or not the field starts a
|                    new page on the form.
|
|   Return Values :  TRUE  - field starts a new page
|                    FALSE - field doesn't start a new page
+--------------------------------------------------------------------------*/
bool new_page(const FIELD * field)
{
  return (Normalize_Field(field)->status & _NEWPAGE)  ? TRUE : FALSE;
}

/* fld_def.c ends here */
