/* This is a pointer table implementation essentially copied from the ptr_table
 * implementation in perl's sv.c, except that it has been modified to use memory
 * shared across threads. */

/* This header is designed to be included several times with different
 * definitions for PTABLE_NAME and PTABLE_VAL_FREE(). */

#undef pPTBLMS
#undef pPTBLMS_
#undef aPTBLMS
#undef aPTBLMS_

/* Context for PerlMemShared_* functions */

#ifdef PERL_IMPLICIT_SYS
# define pPTBLMS  pTHX
# define pPTBLMS_ pTHX_
# define aPTBLMS  aTHX
# define aPTBLMS_ aTHX_
#else
# define pPTBLMS
# define pPTBLMS_
# define aPTBLMS
# define aPTBLMS_
#endif

#ifndef pPTBL
# define pPTBL  pPTBLMS
#endif
#ifndef pPTBL_
# define pPTBL_ pPTBLMS_
#endif
#ifndef aPTBL
# define aPTBL  aPTBLMS
#endif
#ifndef aPTBL_
# define aPTBL_ aPTBLMS_
#endif

#ifndef PTABLE_NAME
# define PTABLE_NAME ptable
#endif

#ifndef PTABLE_VAL_FREE
# define PTABLE_VAL_FREE(V)
#endif

#ifndef PTABLE_JOIN
# define PTABLE_PASTE(A, B) A ## B
# define PTABLE_JOIN(A, B)  PTABLE_PASTE(A, B)
#endif

#ifndef PTABLE_PREFIX
# define PTABLE_PREFIX(X) PTABLE_JOIN(PTABLE_NAME, X)
#endif

#ifndef ptable_ent
typedef struct ptable_ent {
 struct ptable_ent *next;
 const void *       key;
 void *             val;
} ptable_ent;
#define ptable_ent ptable_ent
#endif /* !ptable_ent */

#ifndef ptable
typedef struct ptable {
 ptable_ent **ary;
 UV           max;
 UV           items;
} ptable;
#define ptable ptable
#endif /* !ptable */

#ifndef ptable_new
STATIC ptable *ptable_new(pPTBLMS) {
#define ptable_new() ptable_new(aPTBLMS)
  ptable *t = (ptable *)PerlMemShared_malloc(sizeof *t);
 t->max   = 63;
 t->items = 0;
 t->ary   = (ptable_ent **)PerlMemShared_calloc(t->max + 1, sizeof *t->ary);
 return t;
}
#endif /* !ptable_new */

#ifndef PTABLE_HASH
# define PTABLE_HASH(ptr) \
     ((PTR2UV(ptr) >> 3) ^ (PTR2UV(ptr) >> (3 + 7)) ^ (PTR2UV(ptr) >> (3 + 17)))
#endif

#ifndef ptable_find
STATIC ptable_ent *ptable_find(const ptable * const t, const void * const key) {
#define ptable_find ptable_find
 ptable_ent *ent;
 const UV hash = PTABLE_HASH(key);

 ent = t->ary[hash & t->max];
 for (; ent; ent = ent->next) {
  if (ent->key == key)
   return ent;
 }

 return NULL;
}
#endif /* !ptable_find */

#ifndef ptable_fetch
STATIC void *ptable_fetch(const ptable * const t, const void * const key) {
#define ptable_fetch ptable_fetch
 const ptable_ent *const ent = ptable_find(t, key);

 return ent ? ent->val : NULL;
}
#endif /* !ptable_fetch */

#ifndef ptable_split
STATIC void ptable_split(pPTBLMS_ ptable * const t) {
#define ptable_split(T) ptable_split(aPTBLMS_ (T))
 ptable_ent **ary = t->ary;
 const UV oldsize = t->max + 1;
 UV newsize = oldsize * 2;
 UV i;

 ary = (ptable_ent **)PerlMemShared_realloc(ary, newsize * sizeof(*ary));
 Zero(&ary[oldsize], newsize - oldsize, sizeof(*ary));
 t->max = --newsize;
 t->ary = ary;

 for (i = 0; i < oldsize; i++, ary++) {
  ptable_ent **curentp, **entp, *ent;
  if (!*ary)
   continue;
  curentp = ary + oldsize;
  for (entp = ary, ent = *ary; ent; ent = *entp) {
   if ((newsize & PTABLE_HASH(ent->key)) != i) {
    *entp     = ent->next;
    ent->next = *curentp;
    *curentp  = ent;
    continue;
   } else
    entp = &ent->next;
  }
 }
}
#endif /* !ptable_split */

STATIC void PTABLE_PREFIX(_store)(pPTBL_ ptable * const t, const void * const key, void * const val) {
 ptable_ent *ent = ptable_find(t, key);

 if (ent) {
  void *oldval = ent->val;
  PTABLE_VAL_FREE(oldval);
  ent->val = val;
 } else if (val) {
  const UV i = PTABLE_HASH(key) & t->max;
  ent = (ptable_ent *)PerlMemShared_malloc(sizeof *ent);
  ent->key  = key;
  ent->val  = val;
  ent->next = t->ary[i];
  t->ary[i] = ent;
  t->items++;
  if (ent->next && t->items > t->max)
   ptable_split(t);
 }
}

#ifndef ptable_walk
STATIC void ptable_walk(pTHX_ ptable * const t, void (*cb)(pTHX_ ptable_ent *ent, void *userdata), void *userdata) {
#define ptable_walk(T, CB, UD) ptable_walk(aTHX_ (T), (CB), (UD))
 if (t && t->items) {
  ptable_ent ** const array = t->ary;
  UV i = t->max;
  do {
   ptable_ent *entry;
   for (entry = array[i]; entry; entry = entry->next)
    cb(aTHX_ entry, userdata);
  } while (i--);
 }
}
#endif /* !ptable_walk */

STATIC void PTABLE_PREFIX(_clear)(pPTBL_ ptable * const t) {
 if (t && t->items) {
  ptable_ent ** const array = t->ary;
  UV i = t->max;

  do {
   ptable_ent *entry = array[i];
   while (entry) {
    ptable_ent * const oentry = entry;
    void *val = oentry->val;
    entry = entry->next;
    PTABLE_VAL_FREE(val);
    PerlMemShared_free(oentry);
   }
   array[i] = NULL;
  } while (i--);

  t->items = 0;
 }
}

STATIC void PTABLE_PREFIX(_free)(pPTBL_ ptable * const t) {
 if (!t)
  return;
 PTABLE_PREFIX(_clear)(aPTBL_ t);
 PerlMemShared_free(t->ary);
 PerlMemShared_free(t);
}

#undef pPTBL
#undef pPTBL_
#undef aPTBL
#undef aPTBL_

#undef PTABLE_NAME
#undef PTABLE_VAL_FREE
