#ifndef ERR_H_INCLUDED
#define ERR_H_INCLUDED

#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h> /* toupper */
#include <string.h> /* strrchr */

#define ERR_NONE       "it's not broken"
#define ERR_NOMEM      "ran out of memory"

#ifdef NDEBUG
#define debugf(...) do{}while(0)
#else
//#define debugf(...) fprintf(stderr, __VA_ARGS__)
#define debugf(...) do{}while(0)
#endif

/* cast every strcspn result to type int (-Wfmt) */
#undef strcspn
#define strcspn (int)strcspn

#define DOCS_DEF "#define  "
#define DOCS_EXT "extern "
#define DOCS_SPACE "%-24s "
#define DOCS_SPACE_2 "%s_%-24s "
#define DOCS_SPACE_NUM "%-24s_%d "
static const char *int2str(int i)
{
	static char buf[16];
	sprintf(buf, "%d", i);
	return buf;
}
static const char *Canitize(const char *str, int caps)
{
	static char buf[1024];
	int i;
	for (i = 0; (i < (sizeof(buf) - 1)) && str[i]; ++i)
	{
		buf[i] = str[i];
		if (caps)
			buf[i] = toupper(buf[i]);
		if (!isalnum(buf[i]) && buf[i] != '_')
			buf[i] = '_';
	}
	buf[i] = '\0';
	return buf;
}
static const char *CanitizeProxy(const char *str, int caps)
{
	static char buf[1024];
	int i;
	if (!str)
	{/*FIXME*/
		str = "undefined";
	}
	for (i = 0; (i < (sizeof(buf) - 1)) && str[i]; ++i)
	{
		buf[i] = str[i];
		if (caps)
			buf[i] = toupper(buf[i]);
		if (!isalnum(buf[i]) && buf[i] != '_')
			buf[i] = '_';
	}
	buf[i] = '\0';
	strcat(buf, "_PROXY");
	return buf;
}
static const char *CanitizeNum(const char *str, int caps, int num)
{
	static char buf[1024];
	int i;
	for (i = 0; (i < (sizeof(buf) - 1)) && str[i]; ++i)
	{
		buf[i] = str[i];
		if (caps)
			buf[i] = toupper(buf[i]);
		if (!isalnum(buf[i]) && buf[i] != '_')
			buf[i] = '_';
	}
	buf[i] = '\0';
	sprintf(buf + strlen(buf), "_%d", num);
	return buf;
}
static const char *pathTail(const char *path)
{
	const char *ss;
	if (!(ss = strrchr(path, '/')))
		ss = strrchr(path, '\\');
	if (!ss)
		return path;
	return ss + 1;
}

static int UNUSED__;
static int *success = &UNUSED__;

static const char *error_reason = ERR_NONE;
static void *errmsg(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
#define errmsg0(ALWAYS_ZERO) (errmsg)(ALWAYS_ZERO)
static void *errmsg(const char *fmt, ...)
{
	static char *buf = 0;
	va_list args;
	
	if (!fmt)
		return 0;
	
	if (!buf && !(buf = malloc(4096)))
	{
		error_reason = ERR_NOMEM;
		return 0;
	}
	
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	
	error_reason = buf;
	return 0;
}

#endif /* ERR_H_INCLUDED */

