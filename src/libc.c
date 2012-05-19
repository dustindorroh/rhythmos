/*
 *      libc.c
 *      
 *      Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
 */

#include <kernel.h>

char *error_names[15] = {
	"Success",
	"",
	"Bad file descriptor",	/* EBADF */
	"Invalid argument",	/* EINVAL */
	"No such process",	/* ESRCH */
	"Operation not permitted",	/* EPERM */
	"No such file or directory",	/* ENOENT */
	"Too many open files",	/* EMFILE */
	"Is a directory",	/* EISDIR */
	"Not a directory",	/* ENOTDIR */
	"Function not implemented",	/* ENOSYS */
	"Not enough space",	/* ENOMEM */
	"Bad address",		/* EFAULT */
	"Resource unavailable, try again",	/* EAGAIN */
	"No child processes"	/* ECHILD */
};

char *strerror(int errnum)
{
	if ((0 > errnum) || (ERRNO_MAX < errnum))
		return "Unknown error";
	else
		return error_names[errnum];
}

void perror(const char *s)
{
	if ((NULL != s) && ('\0' != *s))
		printf("%s: %s\n", s, strerror(errno));
	else
		printf("%s\n", s, strerror(errno));
}

void *memset(void *b, int c, size_t len)
{
	char *dest = (char *)b;
	size_t i;
	for (i = 0; i < len; i++)
		dest[i] = c;
	return b;
}

void *memmove(void *dest, const void *src, size_t len)
{
	size_t pos;
	char *to = dest;
	const char *from = src;
	for (pos = 0; pos < len; pos++)
		*(to++) = *(from++);
	return dest;
}

/*
 * morecore() Requests a block of size n bytes (K&R uses multiples of the 
 * allocation header size) which might come from anywhere in the application's
 * address space
 */
//void *morecore(size_t n)

/*
 * sbrk Adjusts the size of the application's address space by d bytes and
 * returns a pointer to the start of the new region. Returns char* because
 * the earliest versions of C compilers had no void* type.
 */
//char *sbrk(size_t d)

/*
 * The  memcpy()  function  copies  n bytes from memory area src to memory area 
 * dest.  The memory areas should not overlap.  Use memmove(3) if the memory 
 * areas do overlap. The memcpy() function returns a pointer to dest.
 */
void *memcpy(void *dest, const void *src, size_t len)
{
	return memmove(dest, src, len);
}

/*
 * Standard C function: parse a string that represents a decimal integer.
 * Leading whitespace is allowed. Trailing gunk is allowed too. Doesn't
 * really report syntax errors in any useful way.
 */

int atoi(const char *s)
{
	static const char digits[] = "0123456789";	/* legal digits in order */
	unsigned val = 0;	/* value we're accumulating */
	int neg = 0;		/* set to true if we see a minus sign */

	/* skip whitespace */
	while (*s == ' ' || *s == '\t') {
		s++;
	}

	/* check for sign */
	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	/* process each digit */
	while (*s) {
		const char *where;
		unsigned digit;

		/* look for the digit in the list of digits */
		where = strchr(digits, *s);
		if (where == NULL) {
			/* not found; not a digit, so stop */
			break;
		}

		/* get the index into the digit list, which is the value */
		digit = (where - digits);

		/* could (should?) check for overflow here */

		/* shift the number over and add in the new digit */
		val = val * 10 + digit;

		/* look at the next character */
		s++;
	}

	/* handle negative numbers */
	if (neg) {
		return -val;
	}

	/* done */
	return val;
}

/*
 * The  strlen()  function  calculates  the  length  of  the string s, not
 * including the terminating '\0' character. The strlen() function returns
 * the number of characters in s.
 */
size_t strlen(const char *s)
{
	size_t len = 0;
	while (s[len])
		len++;
	return len;
}

/* 
 * The strchr() function returns a pointer to the first occurrence of the 
 * character c in the string s.
 */
//strchr(const char *s, int ch)
char *strchr(const char *s, int ch)
{
	/* scan from left to right */
	while (*s) {		/* if we hit it, return it */
		if (*s == ch)
			return (char *)s;
		s++;
	}

	/* if we were looking for the 0, return that */
	if (*s == ch) {
		return (char *)s;
	}

	/* didn't find it */
	return NULL;
}

/*
 * The strcmp() function compares the two strings s1 and s2.  It returns
 * an integer less than, equal to, or greater than zero if  s1  is  found, 
 * respectively, to be less than, to match, or be greater than s2.
 */
int strcmp(const char *s1, const char *s2)
{
	int pos = 0;
	for (; s1[pos] && s2[pos]; pos++) {
		if (s1[pos] != s2[pos])
			return (s1[pos] - s2[pos]);
	}
	if (s1[pos])
		return -1;
	else if (s2[pos])
		return 1;
	else
		return 0;
}

/*
 * The  strncmp()  function  is similar, except it only compares the first
 * (at most) n characters of s1 and s2. 
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
	int pos = 0;
	for (; (pos < n) && s1[pos] && s2[pos]; pos++) {
		if (s1[pos] != s2[pos])
			return (s1[pos] - s2[pos]);
	}
	if (pos < n) {
		if (s2[pos])
			return -1;
		else
			return 1;
	}
	return 0;
}

/*
 * The strcpy() function copies the string pointed to by src, including
 * the terminating null byte ('\0'), to the buffer pointed to by
 * dest. The strings may not overlap, and the destination string dest
 * must be large enough to receive the copy.
 */
char *strcpy(char *dest, const char *src)
{
	size_t i = 0;
	for (; src[i]; i++)
		dest[i] = src[i];

	dest[i] = '\0';

	return dest;
}

/*
 * The strncpy() function is similar, except that at most n bytes of src
 * are copied.  Warning: If there is no null byte among the first n
 * bytes of src, the string placed in dest will not be null terminated.
 *
 * If the length of src is less than n, strncpy() pads the remainder of
 * dest with null bytes.
 */
char *strncpy(char *dest, const char *src, size_t n)
{
	size_t i = 0;
	for (; (i < n) && src[i]; i++)
		dest[i] = src[i];

	for (; i < n; i++)
		dest[i] = '\0';

	return dest;
}

/* 
 * strcat() function concatenates a NULL-terminated string src onto
 * the end of dest, and return dest.
 */
char *strcat(char *dest, const char *src)
{
	while (*dest != 0) {
		*dest = *dest++;
	}

	do {
		*dest++ = *src++;
	}
	while (*src != 0);
	return dest;
}

/* 
 * The strncat() function is similar to strcat(), except that it will use at 
 * most n characters from src; and src does not need to be null terminated if
 * it contains n or more characters.
 */
char *strncat(char *dest, const char *src, size_t n)
{
	if (n != 0) {
		char *d = dest;
		const char *s = src;

		while (*d != 0)
			d++;
		do {
			if ((*d = *s++) == 0)
				break;
			d++;
		} while (--n != 0);
		*d = 0;
	}
	return dest;
}

/*
 * The  strpbrk() function locates the first occurrence in the string s of any 
 * of the characters in the string accept. The strpbrk() function returns a 
 * pointer to the character in s that matches one of the characters in accept, 
 * or NULL if no such character is found. 
 */
char *strpbrk(char *s, char *accept)
{
	while (*s != '0')
		if (strchr(accept, *s) == NULL)
			++s;
		else
			return (char *)s;

	return NULL;
}

static char *olds = NULL;

/* 
 * The strtok() function parses a string into a sequence of tokens. On the first
 *  call to strtok() the string to be parsed should be specified in str.  In 
 * each subsequent call that should parse the same string, str should be NULL. 
 */
char *strtok(char *s, char *delim)
{
	char *token;

	if (s == NULL) {
		if (olds == NULL) {
			/*errno = EINVAL;  Wonder where errno is defined.... */
			return NULL;
		} else
			s = olds;
	}

	/* Scan leading delimiters.  */
	s += strspn(s, delim);
	if (*s == '\0') {
		olds = NULL;
		return NULL;
	}

	/* Find the end of the token.  */
	token = s;
	s = strpbrk(token, delim);
	if (s == NULL)
		/* This token finishes the string.  */
		olds = NULL;
	else {
		/* Terminate the token and make OLDS point past it.  */
		*s = '\0';
		olds = s + 1;
	}
	return token;
}

/* 
 * The strspn() function calculates the length of the initial segment of s 
 * which consists entirely of characters in accept. The  strspn()  function 
 * returns the number of characters in the initial segment of s which consist
 * only of characters from accept.
 */
size_t strspn(char *s, char *accept)
{
	register char *p;
	register char *a;
	register size_t count = 0;

	for (p = s; *p != '\0'; ++p) {
		for (a = accept; *a != '\0'; ++a)
			if (*p == *a)
				break;
		if (*a == '\0')
			return count;
		else
			++count;
	}

	return count;
}

#define addchar(_pos,_c) { if ((_pos)+1 < size)   \
                             str[(_pos)] = (_c);  \
                           (_pos)++; }
void
print_num(char *str, size_t size, size_t * pos, unsigned int val,
	  unsigned int base)
{
	char *hexdigits = "0123456789abcdef";
	unsigned int digits = 1;
	unsigned int mult = 1;
	unsigned int x = val;
	for (; x >= base; x /= base, mult *= base)
		digits++;
	for (x = val; x >= base; x /= base, mult /= base)
		addchar(*pos, hexdigits[(val / mult) % base]);
	addchar(*pos, hexdigits[val % base]);
}

void
print_field(char *str, size_t size, size_t * pos, const char *c,
	    va_list * apptr)
{
	switch (*c) {
	case 's':
		{
			char *s;
			for (s = va_arg(*apptr, char *); *s; s++)
				addchar(*pos, *s);
			break;
		}
	case 'c':
		{
			char val = (char)va_arg(*apptr, int);
			addchar(*pos, val);
			break;
		}
	case 'd':
		{
			int val = va_arg(*apptr, int);
			if (val < 0) {
				val = -val;
				addchar(*pos, '-');
			}
			print_num(str, size, pos, val, 10);
			break;
		}
	case 'u':
		print_num(str, size, pos, va_arg(*apptr, unsigned int), 10);
		break;
	case 'p':
		addchar(*pos, '0');
		addchar(*pos, 'x');
		/*
		 * fall through 
		 */
	case 'x':
		print_num(str, size, pos, va_arg(*apptr, unsigned int), 16);
		break;
	default:
		addchar(*pos, '%');
		break;
	}
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	size_t pos = 0;
	const char *c = format;
	for (; *c; c++) {
		if (*c == '%') {
			c++;

			int right_aligned = 1;
			int pad = 0;
			va_list start_ap = ap;
			size_t start_pos = pos;

			if ('-' == *c) {
				right_aligned = 0;
				c++;
			}

			for (; ('0' <= *c) && ('9' >= *c); c++)
				pad = pad * 10 + *c - '0';

			print_field(str, size, &pos, c, &ap);	/* Print first time */
			pad -= (pos - start_pos);

			if (right_aligned) {
				pos = start_pos;
				ap = start_ap;
				for (; 0 < pad; pad--)	/* Add padding on right */
					addchar(pos, ' ');
				print_field(str, size, &pos, c, &ap);	/* Print again */
			} else {
				for (; 0 < pad; pad--)	/* Add padding on left */
					addchar(pos, ' ');
			}
		} else {
			addchar(pos, *c);
		}
	}
	if (0 < size) {
		if (pos + 1 < size)
			str[pos] = '\0';
		else
			str[size - 1] = '\0';
	}
	return pos;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int r = vsnprintf(str, size, format, ap);
	va_end(ap);
	return r;
}

int printf(const char *format, ...)
{
	if (!in_user_mode())
		assert
		    (!"printf cannot be called from kernel mode; use kprintf instead!");
	char buf[4096];
	va_list ap;
	va_start(ap, format);
	int len = vsnprintf(buf, 4096, format, ap);
	va_end(ap);
	write(STDOUT_FILENO, buf, len);
	return len;
}

/* 
 * puts - writes the string s and a trailing newline to stdout. 
 */
int puts(const char *s)
{
	int len = strlen(s);
	write(STDOUT_FILENO, s, len);
	return len;
}

int kprintf(const char *format, ...)
{
	if (in_user_mode())
		assert
		    (!"kprintf cannot be called from kernel mode; use printf instead!");
	char buf[4096];		/* @TODO Make Macro for 4096 */
	va_list ap;
	va_start(ap, format);
	int len = vsnprintf(buf, 4096, format, ap);
	va_end(ap);
	write_to_screen(buf, len);
	return len;
}

void user_mode_assert(const char *str, const char *function)
{
	printf("Assertion failure in %s: %s\n", function, str);
	exit(1);
}

DIR *opendir(const char *filename)
{
	int fd;
	if (0 > (fd = open(filename, OPEN_DIRECTORY)))
		return NULL;
	DIR *dir = (DIR *) malloc(sizeof(DIR));
	dir->fd = fd;
	return dir;
}

struct dirent *readdir(DIR * dirp)
{
	if (1 != getdent(dirp->fd, &dirp->ent))
		return NULL;
	return &dirp->ent;
}

int closedir(DIR * dirp)
{
	close(dirp->fd);
	free(dirp);
	return 0;
}
