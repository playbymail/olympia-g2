
#include	<stdio.h>
#include	"z.h"



/*
 *  malloc safety checks:
 *
 *	Space for two extra ints is is allocated beyond what the client
 *	asks for.  The size of the malloc'd region is stored at the
 *	beginning, and a magic number is placed at the end.  realloc's
 *	and free's check the integrity of these markers.  This protects
 *	against overruns, makes sure that non-malloc'd memory isn't freed,
 *	and that memory isn't freed twice.
 */

void *
my_malloc(unsigned size)
{
	char *p;

	size += sizeof(int);

	p = malloc(size + sizeof(int));

	if (p == NULL)
	{
		fprintf(stderr, "my_malloc: out of memory (can't malloc "
				"%d bytes)\n", size);
		exit(1);
	}

	bzero(p, size);

	*((int *) p) = (int)size;	/* size header slot, value fits */
	*((int *) (p + size)) = 0xABCF;

	return p + sizeof(int);
}


void *
my_realloc(void *ptr, unsigned size)
{
	char *p = ptr;

	if (p == NULL)
		return my_malloc(size);

	size += sizeof(int);
	p -= sizeof(int);

	assert(*((int *) (p + *(int *) p)) == 0xABCF);

	p = realloc(p, size + sizeof(int));

	*((int *)p) = (int)size;	/* size header slot, value fits */
	*((int *) (p + size)) = 0xABCF;

	if (p == NULL)
	{
		fprintf(stderr, "my_realloc: out of memory (can't realloc "
				"%d bytes)\n", size);
		exit(1);
	}

	return p + sizeof(int);
}


void
my_free(void *ptr)
{
	char *p = ptr;

	p -= sizeof(int);

	assert(*((int *) (p + *(int *) p)) == 0xABCF);
	*((int *) (p + *(int *) p)) = 0;
	*((int *) p) = 0;

	free(p);
}


char *
str_save(char *s)
{
	char *p;

	p = my_malloc((unsigned) (strlen(s) + 1));	/* string len fits 32 bits */
	strcpy(p, s);

	return p;
}


void
asfail(char *file, int line, char *cond)
{
	fprintf(stderr, "assertion failure: %s (%d): %s\n",
						file, line, cond);
	abort();
	exit(1);
}


void
lcase(char *s)
{

	while (*s)
	{
		*s = tolower(*s);
		s++;
	}
}


/*
 *  Line reader with no size limits
 *  strips newline off end of line
 */

#define	GETLIN_ALLOC	255

char *
getlin(FILE *fp)
{
	static char *buf = NULL;
	static unsigned int size = 0;
	int len;
	int c;

	len = 0;

	while ((c = fgetc(fp)) != EOF)
	{
		if (len >= size)
		{
			size += GETLIN_ALLOC;
			buf = my_realloc(buf, size + 1);
		}

		if (c == '\n')
		{
			buf[len] = '\0';
			return buf;
		}

		buf[len++] = (char) c;
	}

	if (len == 0)
		return NULL;

	buf[len] = '\0';

	return buf;
}


/*
 *  Get line, remove leading and trailing whitespace
 */

char *
getlin_ew(FILE *fp)
{
	char *line;
	char *p;

	line = getlin(fp);

	if (line)
	{
		while (*line && iswhite(*line))
			line++;			/* eat leading whitespace */

		for (p = line; *p; p++)
			if (*p < 32 || *p == '\t')	/* remove ctrl chars */
				*p = ' ';
		p--;
		while (p >= line && iswhite(*p))
		{				/* eat trailing whitespace */
			*p = '\0';
			p--;
		}
	}

	return line;
}

#define MAX_BUF         8192

static char linebuf[MAX_BUF];
static ssize_t nread;	/* read() result; ssize_t avoids LP64 truncation */
static int line_fd = -1;
static char *point;


int
readfile(char *path)
{

	if (line_fd >= 0)
		close(line_fd);

	line_fd = open(path, 0);

	if (line_fd < 0)
	{
		fprintf(stderr, "can't open %s: ", path);
		perror("");
		return FALSE;
	}

	nread = read(line_fd, linebuf, MAX_BUF);
	point = linebuf;

	return TRUE;
}


char *
readlin(void)
{
	static char *buf = NULL;
	static unsigned int size = 0;
	int len;
	int c;

	len = 0;

	while (1)
	{
		if (point >= &linebuf[nread])
		{
			if (nread > 0)
				nread = read(line_fd, linebuf, MAX_BUF);

			if (nread < 1)
				break;

			point = linebuf;
		}

		c = *point++;

		if (len >= size)
		{
			size += GETLIN_ALLOC;
			buf = my_realloc(buf, size + 1);
		}

		if (c == '\n')
		{
			buf[len] = '\0';
			return buf;
		}

		buf[len++] = (char) c;
	}

	if (len == 0)
		return NULL;

	buf[len] = '\0';

	return buf;
}


char *
readlin_ew(void)
{
	char *line;
	char *p;

	line = readlin();

	if (line)
	{
		while (*line && iswhite(*line))
			line++;			/* eat leading whitespace */

		for (p = line; *p; p++)
			if (*p < 32 || *p == '\t')	/* remove ctrl chars */
				*p = ' ';
		p--;
		while (p >= line && iswhite(*p))
		{				/* eat trailing whitespace */
			*p = '\0';
			p--;
		}
	}

	return line;
}



#define	COPY_LEN	1024

void
copy_fp(FILE *a, FILE *b)
{
	char buf[COPY_LEN];

	while (fgets(buf, COPY_LEN, a) != NULL)
		fputs(buf, b);
}


char lower_array[256];


void
init_lower(void)
{
	int i;

	for (i = 0; i < 256; i++)
		lower_array[i] = i;

	for (i = 'A'; i <= 'Z'; i++)
		lower_array[i] = i - 'A' + 'a';
}


int
i_strcmp(char *s, char *t)
{
	char a, b;

	do {
		a = tolower(*s);
		b = tolower(*t);
		s++;
		t++;
		if (a != b)
			return a - b;
	} while (a);

	return 0;
}


int
i_strncmp(char *s, char *t, int n)
{
	char a, b;

	do {
		a = tolower(*s);
		b = tolower(*t);
		if (a != b)
			return a - b;
		s++;
		t++;
		n--;
	} while (a && n > 0);

	return 0;
}


static int
fuzzy_transpose(char *one, char *two, int l1, int l2)
{
	int i;
	char buf[LEN];
	char tmp;

	if (l1 != l2)
		return FALSE;

	strcpy(buf, two);

	for (i = 0; i < l2 - 1; i++)
	{
		tmp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = tmp;

		if (strcmp(one, buf) == 0)
			return TRUE;

		tmp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = tmp;
	}

	return FALSE;
}


static int
fuzzy_one_less(char *one, char *two, int l1, int l2)
{
	int count = 0;
	int i, j;

	if (l1 != l2 + 1)
		return FALSE;

	for (j = 0, i = 0; j < l2; i++, j++)
	{
		if (one[i] != two[j])
		{
			if (count++)
				return FALSE;
			j--;
		}
	}

	return TRUE;
}


static int
fuzzy_one_extra(char *one, char *two, int l1, int l2)
{
	int count = 0;
	int i, j;

	if (l1 != l2 - 1)
		return FALSE;

	for (j = 0, i = 0; i < l1; i++, j++)
	{
		if (one[i] != two[j])
		{
			if (count++)
				return FALSE;
			i--;
		}
	}

	return TRUE;
}


static int
fuzzy_one_bad(char *one, char *two, int l1, int l2)
{
	int count = 0;
	int i;

	if (l1 != l2)
		return FALSE;

	for (i = 0; i < l2; i++)
		if (one[i] != two[i] && count++)
			return FALSE;

	return TRUE;
}


int
fuzzy_strcmp(char *one, char *two)
{
	int l1 = (int) strlen(one);	/* name lengths fit 32 bits */
	int l2 = (int) strlen(two);

	if (l2 >= 4 && fuzzy_transpose(one, two, l1, l2))
		return TRUE;

	if (l2 >= 4 && fuzzy_one_less(one, two, l1, l2))
		return TRUE;

	if (l2 >= 5 && fuzzy_one_extra(one, two, l1, l2))
		return TRUE;

	if (l2 >= 5 && fuzzy_one_bad(one, two, l1, l2))
		return TRUE;

	return FALSE;
}
