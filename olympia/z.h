/*
 *  BUGFIX (modernization): pull in the real libc prototypes BEFORE the
 *  engine's bzero/bcopy/abs shadow macros below, so every TU (z.h is the
 *  chokepoint included first by every engine .c, ahead of oly.h) sees real
 *  prototypes for the string/stdlib/io functions instead of implicit-int
 *  declarations -- the 64-bit pointer-truncation hazard (strchr/malloc/...).
 *  Including these before the macros keeps the macros (and oly.h's wait()
 *  macro, which collides with sys/wait.h via stdlib.h) intact by ordering.
 */
#include <string.h>
#include <strings.h>	/* bcopy, bzero */
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

/* BUGFIX (modernization): use varargs and forward declarations */
#include "legacy.h"
/* BUGFIX (modernization): update lists to use 64-bit pointers */
#include "../lib/lists.h"
/* BUGFIX (modernization): use an updated malloc/realloc/free */
#include "../lib/checked_alloc.h"

#define	TRUE	1
#define	FALSE	0

#define		LEN		2048	/* generic string max length */

/*
 *  BUGFIX (modernization): the dead `#ifdef SYSV` bzero/bcopy shadow macros
 *  were removed (issue #14). SYSV is never defined anywhere in the tree, so
 *  the macros never expanded; bzero/bcopy come from <strings.h> (included at
 *  the top of this header). This also makes the Phase-4 invariant explicit:
 *  the golden-critical MD5 in rnd.c uses the real libc bzero/bcopy, never a
 *  shadow macro.
 */

#define	abs(n)		((n) < 0 ? ((n) * -1) : (n))

#define	isalpha(c)	(((c)>='a' && (c)<='z') || ((c)>='A' && (c)<='Z'))
#define	isdigit(c)	((c) >= '0' && (c) <= '9')
#define	iswhite(c)	((c) == ' ' || (c) == '\t')

#if 1
#define	tolower(c)	(lower_array[c])
extern char lower_array[];
#else
#define	tolower(c)	(((c) >= 'A' && (c) <= 'Z') ? ((c) - 'A' + 'a') : (c))
#endif

#define	toupper(c)	(((c) >= 'a' && (c) <= 'z') ? ((c) - 'a' + 'A') : (c))

#if 0
#define	max(a,b)	((a) > (b) ? (a) : (b))
#define	min(a,b)	((a) < (b) ? (a) : (b))
#endif

extern char *str_save(char *);

extern char *getlin(FILE *);
extern char *getlin_ew(FILE *);
extern int i_strncmp(char *s, char *t, int n);
extern int i_strcmp(char *s, char *t);
extern int fuzzy_strcmp(char *, char *);
extern int rnd(int low, int high);

/*
 *  Assertion verifier
 */

extern void asfail(char *file, int line, char *cond);

#ifdef __STDC__
#define	assert(p)	if(!(p)) asfail(__FILE__, __LINE__, #p);
#else
#define	assert(p)	if(!(p)) asfail(__FILE__, __LINE__, "p");
#endif

extern int readfile(char *path);
extern char *readlin(void);
extern char *readlin_ew(void);
extern char *eat_leading_trailing_whitespace(char *s);

/*
 *  BUGFIX (modernization): prototypes for the functions defined in z.c and
 *  rnd.c.  Those files don't include oly.h, so they never see proto.h; z.h
 *  is the header they (and their engine-wide callers) do include, so the
 *  cross-file RNG/utility API is declared here instead.  (rnd and load_seed
 *  are already declared above / in legacy.h.)
 */
extern void lcase(char *s);
extern void copy_fp(FILE *a, FILE *b);
extern void init_lower(void);
extern void test_random(void);
extern void MD5(void *dest, void *orig, int len);
extern void save_seed(char *fnam);
extern int md5_int(int a, int b, int c, int d);
