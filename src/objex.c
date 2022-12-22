/* <z64.me> objex parsing */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h> /* SCNo8 */
#include <math.h> /* round */

#include "objex.h"
#include "err.h"
#include "texture.h"

#include "ksort.h"
#include "streq.h"

#define objex_f_pvt_lt(a, b) ((intptr_t)(a)._pvt < (intptr_t)(b)._pvt)
KSORT_INIT(fPvt, struct objex_f, objex_f_pvt_lt)

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define ERR_LOADFILE   "failed to load '%s'"
#define ERR_CHDIRFILE  "failed to enter directory of file '%s'", fn

/* private functions */
static int min_int(int a, int b)
{
	return a < b ? a : b;
}

/* returns true if two strings are equal, or if the beginning
 * of the longer one matches the entire length of the shorter
 */
static int streq(const char *a, const char *b)
{
	int i = min_int(strlen(a), strlen(b));
	return !memcmp(a, b, i);
}

/* test if a line should be skipped */
static int should_skip(const char *ss)
{
	return (isblank(*ss) || iscntrl(*ss));
}

static void free_if_udata(void *udata)
{
	struct udata
	{
		objex_udata_free  free;
	};
	
	if (udata)
		((struct udata*)(udata))->free(udata);
}

/* returns pointer to ndl if it exists in hay, or 0 if it doesn't
 * (nq means no-quotes; it skips over 'quotes' and "quotes")
 */
static const char *linecontainsnq(const char *hay, const char *ndl)
{
	if (!hay || !ndl)
		return 0;
	
	int quote = 0;
	while (*hay && !iscntrl(*hay))
	{
		if (*hay == '\'' || *hay == '"')
			quote = !quote;
		if (!quote && streq(hay, ndl))
			return hay;
		++hay;
	}
	
	return 0;
}
static const char *linecontainsnq32(const char *hay, const char *ndl)
{
	if (!hay || !ndl)
		return 0;
	
	int quote = 0;
	while (*hay && !iscntrl(*hay))
	{
		if (*hay == '\'' || *hay == '"')
			quote = !quote;
		if (!quote && streq32(hay, ndl))
			return hay;
		++hay;
	}
	
	return 0;
}

/* returns pointer to first non-whitespace character of next line,
 * or 0 once it reaches the end of the string (skips blank lines)
 */
static const char *nextline(const char *str)
{
	char *next;
	
	if (!str)
		return 0;
	
	if (!(next = strchr(str, '\n')))
		next = strchr(str, '\r');
	if (!next)
		return 0;
	str = next;
	
	while (*str && (iscntrl(*str) || isblank(*str)))
		++str;
	if (!*str)
		return 0;
	
	return str;
}

/* returns pointer to first non-whitespace character of next line,
 * or 0 once it reaches the end of the string (skips blank lines)
 */
static const char *nextlineNum(const char *str, int *lineNum)
{
	const char *next;
	
	if (!str)
		return 0;
	
	next = nextline(str);
	if (!next)
		return 0;
	
	while (str < next)
	{
		if (*str == '\n')
			*lineNum += 1;
		++str;
	}
	
	return str;
}

/* returns pointer to first non-whitespace character of next line,
 * or 0 once it reaches the end of the string (skips blank lines)
 */
static char *nextline_(char *str)
{
	char *next;
	
	if (!str)
		return 0;
	
	if (!(next = strchr(str, '\n')))
		next = strchr(str, '\r');
	if (!next)
		return 0;
	str = next;
	
	while (*str && (iscntrl(*str) || isblank(*str)))
		++str;
	if (!*str)
		return 0;
	
	return str;
}

/* strips obj-style comments, redundant whitespace, tabs->spaces */
static void sanitize(char *str)
{
	if (!str)
		return;
	/* handle comments */
	/*int comment = 0;
	for (char *ss = str; *ss; ++ss)
	{
		if (*ss == '#')
			comment = 1;
		if (comment)
		{
			if (*ss == '\n' || *ss == '\r')
				comment = 0;
			else
				*ss = ' ';
		}
		else if (*ss == '\t')
			*ss = ' ';
	}*/
	/* handle comments (right way) a line must start with # */
	for (char *ss = str; ss; ss = nextline_(ss))
	{
		if (*ss == '#')
			memset(ss, ' ', strcspn(ss, "\r\n"));
	}
	/* handle redundant whitespace */
	for (char *ss = str; *ss; ++ss)
	{
		if (*ss == ' ' || *ss == '\t')
		{
			char *ss1 = ss;
			while (*ss1 == ' ' || *ss1 == '\t')
				++ss1;
			if (ss1 > ss + 1)
				memmove(ss + 1, ss1, strlen(ss1)+1);
		}
	}
}

static void sanitize_slashes(char *str)
{
	if (!str)
		return;
	
	while (*str)
	{
		if (*str == '\\')
			*str = '/';
		
		++str;
	}
}

static int texture_count(struct objex_texture *head)
{
	int num = 0;
	while (head)
	{
		++num;
		head = head->next;
	}
	return num;
}

/*Function to swap the nodes */
static struct objex_texture *texture_swap(
	struct objex_texture *ptr1
	, struct objex_texture *ptr2
)
{
	struct objex_texture *tmp = ptr2->next;
	ptr2->next = ptr1;
	ptr1->next = tmp;
	return ptr2;
}
  
/* Function to sort the list */
static void texture_sortByPriority(struct objex_texture **head)
{
	if (!head || !*head)
		return;
	
	struct objex_texture **h;
	int i, j, swapped;
	
	int count = texture_count(*head);
  
	for (i = 0; i <= count; i++)
	{
		h = head;
		swapped = 0;
		
		for (j = 0; j < count - i - 1; j++)
		{
			struct objex_texture *p1 = *h;
			struct objex_texture *p2 = p1->next;
			
			if (p1->priority < p2->priority)
			{
				/* update the link after swapping */
				*h = texture_swap(p1, p2);
				swapped = 1;
			}
			
			h = &(*h)->next;
		}
		
		/* break if the loop ended without any swap */
		if (swapped == 0)
			break;
	}
}

static int group_count(struct objex_g *head)
{
	int num = 0;
	while (head)
	{
		++num;
		head = head->next;
	}
	return num;
}

/*Function to swap the nodes */
static struct objex_g *group_swap(
	struct objex_g *ptr1
	, struct objex_g *ptr2
)
{
	struct objex_g *tmp = ptr2->next;
	ptr2->next = ptr1;
	ptr1->next = tmp;
	return ptr2;
}
  
/* Function to sort the list */
static void group_sortByPriority(struct objex_g **head)
{
	if (!head || !*head)
		return;
	
	struct objex_g **h;
	int i, j, swapped;
	
	int count = group_count(*head);
  
	for (i = 0; i <= count; i++)
	{
		h = head;
		swapped = 0;
		
		for (j = 0; j < count - i - 1; j++)
		{
			struct objex_g *p1 = *h;
			struct objex_g *p2 = p1->next;
			
			if (p1->priority < p2->priority)
			{
				/* update the link after swapping */
				*h = group_swap(p1, p2);
				swapped = 1;
			}
			
			h = &(*h)->next;
		}
		
		/* break if the loop ended without any swap */
		if (swapped == 0)
			break;
	}
}

static int material_count(struct objex_material *head)
{
	return head->objex->mtlNum;
}

/*Function to swap the nodes */
static struct objex_material *material_swap(
	struct objex_material *ptr1
	, struct objex_material *ptr2
)
{
	struct objex_material *tmp = ptr2->next;
	ptr2->next = ptr1;
	ptr1->next = tmp;
	return ptr2;
}
  
/* Function to sort the list */
static void material_sortByPriority(struct objex_material **head)
{
	if (!head || !*head)
		return;
	
	struct objex_material **h;
	int i, j, swapped;
	
	int count = material_count(*head);
  
	for (i = 0; i <= count; i++)
	{
		h = head;
		swapped = 0;
		
		for (j = 0; j < count - i - 1; j++)
		{
			struct objex_material *p1 = *h;
			struct objex_material *p2 = p1->next;
			
			if (p1->priority < p2->priority)
			{
				/* update the link after swapping */
				*h = material_swap(p1, p2);
				swapped = 1;
			}
			
			h = &(*h)->next;
		}
		
		/* break if the loop ended without any swap */
		if (swapped == 0)
			break;
	}
	
	/* reindex */
	struct objex_material *mtl;
	for (i = 0, mtl = *head; mtl; mtl = mtl->next)
	{
		mtl->index = i;
	}
}

static void free_bones(struct objex_bone *b, void free(void *))
{
	if (!b)
		return;
	
	if (b->child)
		free_bones(b->child, free);
	
	if (b->next)
		free_bones(b->next, free);
	
	if (b->name)
		free(b->name);
	free_if_udata(b->udata);
	free(b);
}

static void *loadfile(
	FILE *fopen(const char *, const char *)
	, size_t fread(void *, size_t, size_t, FILE *)
	, void *malloc(size_t)
	, const char *fn
	, size_t *sz
	, int pad
)
{
	FILE *fp;
	unsigned char *bin;
	size_t sz1;
	if (!sz) sz = &sz1;
	if (
		!(fp = fopen(fn, "rb"))
		|| fseek(fp, 0, SEEK_END)
		|| !(*sz = ftell(fp))
		|| fseek(fp, 0, SEEK_SET)
		|| !(bin = malloc(*sz + pad))
		|| (fread(bin, 1, *sz, fp) != *sz)
		|| fclose(fp)
	)
		return 0;
	
	memset(bin + *sz, 0, pad);
	
	return bin;
}

static struct objex_skeleton *skeleton_find(
	struct objex *objex, const char *name
)
{
	struct objex_skeleton *sk;
	for (sk = objex->sk; sk; sk = sk->next)
		if (!strcmp(sk->name, name))
			return sk;
	return 0;
}

static struct objex_material *material_find(
	struct objex *objex, const char *name
)
{
	struct objex_material *m;
	for (m = objex->mtl; m; m = m->next)
		if (!strcmp(m->name, name))
			return m;
	return 0;
}

static struct objex_texture *texture_find(
	struct objex *objex, const char *name
)
{
	struct objex_texture *m;
	for (m = objex->tex; m; m = m->next)
		if (!strcmp(m->name, name))
			return m;
	return 0;
}

static struct objex_texture *texture_find_filename(
	struct objex *objex, const char *name
)
{
	struct objex_texture *m;
	for (m = objex->tex; m; m = m->next)
		if (m->filename && !strcmp(m->filename, name))
			return m;
	return 0;
}

static struct objex_bone *bone__find(
	struct objex_bone *bone, const char *name
)
{
	struct objex_bone *r;
	
	if (!bone)
		return 0;
		
	if (!strcmp(bone->name, name))
		return bone;
	
	if ((r = bone__find(bone->child, name)))
		return r;
	
	if ((r = bone__find(bone->next, name)))
		return r;
	
	return 0;
}

static struct objex_bone *bone_find(
	struct objex_skeleton *skeleton, const char *name
)
{
	return bone__find(skeleton->bone, name);
}

static const char *nexttok_p(const char *str, int ahead)
{
	const char *Ostr = str;
	if (!str)
		return 0;
	if (!ahead)
		return str;
	
	while (ahead)
	{
		int until = ' ';
		/* seek whitespace */
		if (*str == '\'' || *str == '"')
		{
			until = *str;
			++str;
		}
		while (*str && (*str == '\\' || *str != until))
			++str;
		if (until != ' ')
			++str;
		
		/* skip whitespace */
		while (*str && isspace(*str))
			++str;
		
		/* advance to next token */
		--ahead;
	}
	
	/* prevents fetching tokens across lines */
	while (Ostr < str)
	{
		if (*Ostr == '\n')
			return 0;
		++Ostr;
	}
	
	return str;
}

static const char *nexttok_linerem(const char *str, int ahead)
{
	assert(ahead >= 0);
	
	static char buf[1024];
	int i;
	int until = '\n';
	
	str = nexttok_p(str, ahead);
	if (!str) return 0;
	if (!ahead) return str;
	
	/* ahead is how many tokens ahead (how many we skip) */
	buf[0] = '\0';
	
	/* if is open quote, end is close quote */
	if (*str == '\'' || *str == '"')
	{
		until = *str;
		++str;
	}
	
	/* copy into buf */
	for (i = 0; i < sizeof(buf); ++i, ++str)
	{
		if (*str == until || !*str || iscntrl(*str))
		{
			buf[i] = '\0';
			break;
		}
		buf[i] = *str;
	}
	
	if (!buf[0])
		return 0;
	
	return buf;
}

static const char *nexttok(const char *str, int ahead)
{
	assert(ahead >= 0);
	
	static char buf[1024];
	int i;
	int until = ' ';
	
	str = nexttok_p(str, ahead);
	if (!str) return 0;
	if (!ahead) return str;
	
	/* ahead is how many tokens ahead (how many we skip) */
	buf[0] = '\0';
	
	/* if is open quote, end is close quote */
	if (*str == '\'' || *str == '"')
	{
		until = *str;
		++str;
	}
	
	/* copy into buf */
	for (i = 0; i < sizeof(buf); ++i, ++str)
	{
		if (*str == until || !*str || iscntrl(*str))
		{
			buf[i] = '\0';
			break;
		}
		buf[i] = *str;
	}
	
	if (!buf[0])
		return 0;
	
	return buf;
}

/* allocate and link a new objex_g into an objex */
static struct objex_g *g_push(
	struct objex *objex
	, void *calloc(size_t, size_t)
)
{
	struct objex_g *g;
	
	assert(objex);
	assert(calloc);
	
	/* create group */
	if (!(g = calloc(1, sizeof(*g))))
		return errmsg(ERR_NOMEM);
	
	/* link into list */
	/* append */
	if (objex->g)
	{
		struct objex_g *w;
		for (w = objex->g; w->next; w = w->next)
			;
		w->next = g;
	}
	else
		objex->g = g;
	
	/* note index */
	g->objex = objex;
	g->index = objex->gNum;
	objex->gNum += 1;
	
	return g;
}

/* initialize an objex_g inside an objex, with lookahead */
static struct objex_g *g_push_lookahead(
	struct objex *objex
	, void *calloc(size_t, size_t)
	, const char *name
	, const char *work /* pointer to g or f in raw objex file */
)
{
	struct objex_g *g;
	
	assert(name);
	assert(work);
	assert(calloc);
	assert(objex);
	
	/* create group */
	if (!(g = g_push(objex, calloc)))
		return errmsg0(0);
	
	/* copy name */
	if (!(g->name = malloc(strlen(name)+1)))
		return errmsg(ERR_NOMEM);
	strcpy(g->name, name);
	
	/* pointer to f instead of g was passed, so count one extra */
	if (streq16(work, "f "))
		g->fNum++;

/* attrib */
	const char *attrib = 0;
	size_t attribLen = 0;
	/* count then alloc attrib string (if applicable) */
	for (attrib = work; (attrib = nextline(attrib)); attrib++)
	{
		if (streq32(attrib, "attrib "))
		{
			const char *next = nextline(attrib);
			if (!next)
				next = attrib + strlen(attrib);
			attribLen += strcspn(attrib, "\r\n")+1;
		}
		else if (streq16(attrib, "g ") || streq16(attrib, "o "))
			break;
	}
	
	/* zero-length string silences "missing attribs" errors */
	if (!attribLen)
		attribLen = 1;
	
	if (attribLen
		&& (++attribLen)
		&& !(g->attrib = malloc(attribLen))
	)
		return errmsg(ERR_NOMEM);
	if (g->attrib)
		g->attrib[0] = '\0';
/* /attrib */
	
	/* look-ahead to count faces */
	/* notice that current line is skipped before starting
	 * (we assume it's a g or o) */
	for (const char *i = work; (i = nextline(i)); ++i)
	{
		if (streq16(i, "f "))
			g->fNum++;
		else if (streq16(i, "g ") || streq16(i, "o "))
			break;
	}
	
	/* none found */
	if (!g->fNum)
		return errmsg("objex: group '%s' contains no faces", name);
	
	/* allocate array, reset counter */
	g->f = calloc(g->fNum, sizeof(*g->f));
	g->fNum = 0;
	g->fOwns = 1;
	
	return g;
}

/* returns 0 on failure, non-zero on success */
static void *skellib(
	struct objex *objex
	, void *calloc(size_t, size_t)
	, void free(void *)
	, const char *raw
	, const unsigned skelSeg
	, enum objex_flag flags
	, const char *exportid
)
{
#define errmsg(fmt, ...) (errmsg)("skellib(%d): " fmt, lineNum, ##__VA_ARGS__)
	assert(objex);
	assert(calloc);
	assert(free);
	assert(raw);
	
	struct objex_skeleton *sk = 0;
	struct objex_bone *bone = 0;
	int level = 0;
	int index = 0;
	int lineNum = 1;
	
	for (const char *ss = raw; ss; (ss = nextlineNum(ss, &lineNum)))
	{
		if (should_skip(ss))
			continue;
		else if (streq32(ss, "exportid "))
		{
			const char *name = nexttok_linerem(ss, 1);
			if (!name)
				return errmsg(
					"could not fetch string identifier from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (strcmp(name, exportid))
				return errmsg(
					"exportid mismatch (blender export issue)"
				);
			exportid = 0;
		}
		/* new skeleton */
		else if (streq32(ss, "newskel"))
		{
			const char *name;
			index = 0;
			
			/* previous skeleton is finished */
			if (sk && level)
				return errmsg(
					"skeleton '%s' bad push/pop structure"
					, sk->name
				);

// XXX unnecessary
//			/* previous skeleton is empty */
//			if (sk && !bone)
//				return errmsg("skeleton '%s' contains no bones", sk->name);
			
			if (!(sk = calloc(1, sizeof(*sk))))
				return errmsg(ERR_NOMEM);
			
			/* fetch name */
			if (!(name = nexttok(ss, 1)))
				return errmsg(
					"could not fetch skeleton name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* copy name */
			if (!(sk->name = calloc(1, strlen(name)+1)))
				return errmsg(ERR_NOMEM);
			strcpy(sk->name, name);
			
			/* optional extra field */
			if ((name = nexttok(ss, 2)))
			{
				if (!(sk->extra = calloc(1, strlen(name)+1)))
					return errmsg(ERR_NOMEM);
				strcpy(sk->extra, name);
			}
			
			/* copy default segment */
			sk->segment = skelSeg;
			
			/* link into list */
			/* append */
			if (objex->sk)
			{
				struct objex_skeleton *w;
				for (w = objex->sk; w->next; w = w->next)
					;
				w->next = sk;
			}
			else
				objex->sk = sk;
			
			bone = 0;
			sk->objex = objex;
		}
		else if (streq32(ss, "segment "))
		{
			const char *name;
			
			if (!sk)
				return errmsg("'segment' directive used without 'newskel'");
			
			if (!(name = nexttok(ss, 1)))
				return errmsg(
					"'segment' directive without specifier: '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (sscanf(name, "%X", &sk->segment) != 1)
				return errmsg(
					"'segment' invalid specifier: '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* segment is then followed by 'local' */
			if ((name = nexttok(ss, 2)) && streq32(name, "local"))
				sk->segmentIsLocal = 1;
		}
		else if (streq32(ss, "pbody"))
		{
			const char *n;
			
			if (!sk)
				return errmsg("'pbody' directive used without 'newskel'");
			
			sk->isPbody = 1;
			
			if ((n = nexttok(ss, 1)) && streq32(n, "parent"))
			{
				struct objex_skeleton *pSk;
				struct objex_bone *pBone;
				const char *name;
				
				if (!(name = nexttok(ss, 1 + 1)))
					return errmsg(
						"'parent' directive without skeleton name: '%.*s'"
						, strcspn(ss, "\r\n"), ss
					);
				
				if (!(pSk = skeleton_find(objex, name)))
					return errmsg("could not locate skeleton '%s'", name);
				
				if (pSk->parent)
					return errmsg(
						"cannot use skeleton '%s' as parent (it has a parent)"
						, pSk->parent->skeleton->name
					);
				
				if (!(name = nexttok(ss, 1 + 2)))
					return errmsg(
						"'parent' directive without bone name: '%.*s'"
						, strcspn(ss, "\r\n"), ss
					);
				
				if (!(pBone = bone_find(pSk, name)))
					return errmsg(
						"could not locate bone '%s' in skeleton '%s'"
						, name
						, pSk->name
					);
				
				sk->parent = pBone;
				sk->isPbody = 1;
			}
		}
		/* XXX moved to inside Pbody, left here for testing only */
		else if (streq32(ss, "parent "))
		{
			struct objex_skeleton *pSk;
			struct objex_bone *pBone;
			const char *name;
			
			if (!sk)
				return errmsg("'parent' directive used without 'newskel'");
			
			if (!(name = nexttok(ss, 1)))
				return errmsg(
					"'parent' directive without skeleton name: '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (!(pSk = skeleton_find(objex, name)))
				return errmsg("could not locate skeleton '%s'", name);
			
			if (pSk->parent)
				return errmsg(
					"cannot use skeleton '%s' as parent (it has a parent)"
					, pSk->parent->skeleton->name
				);
			
			if (!(name = nexttok(ss, 2)))
				return errmsg(
					"'parent' directive without bone name: '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (!(pBone = bone_find(pSk, name)))
				return errmsg(
					"could not locate bone '%s' in skeleton '%s'"
					, name
					, pSk->name
				);
			
			sk->parent = pBone;
			sk->isPbody = 1;
		}
		else if (streq8(ss, "-"))
		{
			if (!sk)
				return errmsg(
					"'-' directive used without 'newskel' directive"
				);
			--level;
			if (level < 0 || !bone)
				return errmsg(
					"skel '%s': too many pop directives"
					, sk->name
				);
			
			/* pop back to parent */
			bone = bone->parent;
		}
		else if (streq8(ss, "+"))
		{
			struct objex_bone *b;
			const char *name;
			
			if (!sk)
				return errmsg(
					"'+' directive used without 'newskel' directive"
				);
			
			if (!(b = calloc(1, sizeof(*b))))
				return errmsg(ERR_NOMEM);
			
			/* if skeleton has no bones, this is the root bone */
			if (!sk->bone)
				sk->bone = b;
			
			/* establish parent/child linkage */
			b->parent = bone;
			if (bone)
			{
				/* bone already has a child */
				if (bone->child)
				{
					bone = bone->child;
					
					/* walk bone's children until an empty one is found */
					while (bone->next)
						bone = bone->next;
					
					bone->next = b;
				}
				/* bone didn't have a child */
				else
					bone->child = b;
			}
			
			/* all operations that follow are done on new bone */
			bone = b;
			bone->skeleton = sk;
			bone->index = index;
			index += 1;
			
			/* fetch name */
			if (!(name = nexttok(ss, 1)))
				return errmsg(
					"could not fetch bone name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* copy name */
			if (!(bone->name = calloc(1, strlen(name)+1)))
				return errmsg(ERR_NOMEM);
			strcpy(bone->name, name);
			
//			if (bone->parent)
//				debugf("%s's parent is %s\n", name, bone->parent->name);
			
			/* fetch coordinates */
			/* older blender plug-in objex.py x = x, y = z, z = -y */
			if (
				!(name = nexttok_p(ss, 2))
				|| sscanf(name, "%f %f %f", &b->x, &b->y, &b->z) != 3
			)
				return errmsg(
					"could not fetch bone coordinates from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			++level;
			sk->boneNum++;
		}
		else if (flags & OBJEX_UNKNOWN_DIRECTIVES)
			return errmsg("unknown directive '%.*s'", strcspn(ss, " \n"), ss);
	}
	
	if (level)
		return errmsg(
			"skeleton '%s' bad push/pop structure"
			, sk->name
		);
	
	if (exportid)
		return errmsg("skellib contains no exportid");
	
	return objex;
#undef errmsg
}

/* returns 0 on failure, non-zero on success */
static void *animlib(
	struct objex *objex
	, void *calloc(size_t, size_t)
	, void free(void *)
	, const char *raw
	, enum objex_flag flags
	, const char *exportid
)
{
#define errmsg(fmt, ...) (errmsg)("animlib(%d): " fmt, lineNum, ##__VA_ARGS__)
	assert(objex);
	assert(calloc);
	assert(free);
	assert(raw);
	
	const struct objex_skeleton *sk = 0;
	struct objex_animation *anim = 0;
	struct objex_frame *frame = 0;
	struct objex_xyz *rot = 0;
	int lineNum = 1;
	
	for (const char *ss = raw; ss; (ss = nextlineNum(ss, &lineNum)))
	{
		if (should_skip(ss))
			continue;
		else if (streq32(ss, "exportid "))
		{
			const char *name = nexttok_linerem(ss, 1);
			if (!name)
				return errmsg(
					"animlib: could not fetch string identifier from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (strcmp(name, exportid))
				return errmsg(
					"animlib exportid mismatch (blender export issue)"
				);
			exportid = 0;
		}
		/* new skeleton */
		else if (streq32(ss, "newanim"))
		{
			const char *name;
			int frameNum;
			
			if (!(anim = calloc(1, sizeof(*anim))))
				return errmsg(ERR_NOMEM);
			
			/* fetch skeleton */
			if (!(name = nexttok(ss, 1)))
				return errmsg(
					"could not fetch skeleton name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (!(sk = anim->sk = skeleton_find(objex, name)))
				return errmsg("could not locate skeleton '%s'", name);
			
			/* fetch animation name */
			if (!(name = nexttok(ss, 2)))
				return errmsg(
					"could not fetch animation name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* copy animation name */
			if (!(anim->name = calloc(1, strlen(name)+1)))
				return errmsg(ERR_NOMEM);
			strcpy(anim->name, name);
			
			/* fetch frame count */
			if (
				!(name = nexttok(ss, 3))
				|| sscanf(name, "%d", &frameNum) != 1
			)
				return errmsg(
					"could not fetch frame count from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* allocate frames */
			if (!(anim->frame = calloc(frameNum, sizeof(*anim->frame))))
				return errmsg(ERR_NOMEM);
			
			/* link into list */
			/* append */
			if (objex->anim)
			{
				struct objex_animation *w;
				for (w = objex->anim; w->next; w = w->next)
					;
				w->next = anim;
			}
			else
				objex->anim = anim;
			
			frame = anim->frame - 1;
			anim->frameNum = frameNum;
			rot = 0;
		}
		else if (streq32(ss, "loc "))
		{
			if (!frame)
				return errmsg("loc directive used before newanim");
			
			++frame;
			if (frame - anim->frame >= anim->frameNum)
				return errmsg("unexpected loc directive (too many)");
			
			if (!(frame->rot = calloc(sk->boneNum, sizeof(*rot))))
				return errmsg(ERR_NOMEM);
			rot = frame->rot;
			
			if (sscanf(
					ss
					, "loc %f %f %f %d"
					, &frame->pos.x, &frame->pos.y, &frame->pos.z
					, &frame->ms
				) == 4
			)
				anim->is_keyed = 1;
			else if (sscanf(
					ss
					, "loc %f %f %f"
					, &frame->pos.x, &frame->pos.y, &frame->pos.z
				) != 3
			)
				return errmsg(
					"could not read coordinates '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
		}
		else if (streq32(ss, "rot "))
		{
			if (!rot)
				return errmsg("rot directive used before loc directive");
			
			if (rot - frame->rot >= sk->boneNum)
				return errmsg("unexpected rot directive (too many)");
			
			if (sscanf(ss, "rot %f %f %f", &rot->x, &rot->y, &rot->z) != 3)
				return errmsg(
					"could not read values from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			++rot;
		}
		else if (flags & OBJEX_UNKNOWN_DIRECTIVES)
			return errmsg("unknown directive '%.*s'", strcspn(ss, " \n"), ss);
	}
	
	if (exportid)
		return errmsg("animlib contains no exportid");
	
	return objex;
#undef errmsg
}

static struct objex_material *pushmat(
	struct objex *objex
	, void *calloc(size_t, size_t)
	, const char *name
)
{
	struct objex_material *mtl;
	
	if (!(mtl = calloc(1, sizeof(*mtl))))
		return errmsg(ERR_NOMEM);
	
	/* copy name */
	if (!(mtl->name = calloc(1, strlen(name)+1)))
		return errmsg(ERR_NOMEM);
	strcpy(mtl->name, name);
	
	/* link into list */
	/* append */
	if (objex->mtl)
	{
		struct objex_material *w;
		for (w = objex->mtl; w->next; w = w->next)
			;
		w->next = mtl;
	}
	else
		objex->mtl = mtl;
	
	/* note index */
	mtl->objex = objex;
	mtl->index = objex->mtlNum;
	objex->mtlNum += 1;
	
	/* FIXME forcing standalone only for testing purposes */
	if (!strstr(mtl->name, "empty."))
		mtl->isStandalone = 1;
	
	return mtl;
}

static struct objex_texture *pushtex(
	struct objex *objex
	, void *calloc(size_t, size_t)
	, const char *name
	, const int canAlreadyExist /* if name allowed to already exist */
)
{
	struct objex_texture *tex;
	
	/* it's possible that such a texture already exists */
	if ((tex = texture_find(objex, name)))
	{
		if (canAlreadyExist)
			return tex;
		return errmsg("duplicate texture '%s'", name);
	}
	
	if (!(tex = calloc(1, sizeof(*tex))))
		return errmsg(ERR_NOMEM);
	
	/* copy name */
	if (!(tex->name = calloc(1, strlen(name)+1)))
		return errmsg(ERR_NOMEM);
	strcpy(tex->name, name);
	
	/* link into list */
	/* append */
	if (objex->tex)
	{
		struct objex_texture *w;
		for (w = objex->tex; w->next; w = w->next)
			;
		w->next = tex;
	}
	else
		objex->tex = tex;
	
	tex->objex = objex;
	
	return tex;
}

/* returns 0 on failure, non-zero on success */
static void *mtllib(
	struct objex *objex
	, void *calloc(size_t, size_t)
	, void free(void *)
	, const char *raw
	, enum objex_flag flags
	, const char *exportid
)
{
#define errmsg(fmt, ...) (errmsg)("mtllib(%d): " fmt, lineNum, ##__VA_ARGS__)
	assert(objex);
	assert(calloc);
	assert(free);
	assert(raw);
	
	struct objex_material *mtl = 0;
	struct objex_texture *tex = 0;
	int lineNum = 1;
#define ASSERT_MTL \
	if (!mtl) \
		return errmsg("directive '%.*s' used without 'newmtl'" \
			, strcspn(ss, " \n"), ss \
		); \
	if (tex) \
		return errmsg("directive '%.*s' used beneath 'newtex'" \
			, strcspn(ss, " \n"), ss \
		);
#define ASSERT_TEX \
	if (!tex) \
		return errmsg("directive '%.*s' used without 'newtex'" \
			, strcspn(ss, " \n"), ss \
		); \
	if (mtl) \
		return errmsg("directive '%.*s' used beneath 'newmtl'" \
			, strcspn(ss, " \n"), ss \
		);
#define ASSERT_TEX_FILENAME \
if (tex && !tex->filename) \
	return errmsg( \
		"texture '%s' no filename specified" \
		, tex->name \
	);
	
	for (const char *ss = raw; ss; (ss = nextlineNum(ss, &lineNum)))
	{
		if (should_skip(ss))
			continue;
		else if (streq32(ss, "exportid "))
		{
			const char *name = nexttok_linerem(ss, 1);
			if (!name)
				return errmsg(
					"mtllib: could not fetch string identifier from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (strcmp(name, exportid))
				return errmsg(
					"mtllib exportid mismatch (blender export issue)"
				);
			exportid = 0;
		}
		/* new material */
		else if (streq32(ss, "newmtl "))
		{
			const char *attrib;
			const char *gbi;
			const char *name;
			
			/* ensure the previous texture has a filename */
			ASSERT_TEX_FILENAME
			tex = 0; /* intentionally cleared to 0 */
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch material name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (!(mtl = pushmat(objex, calloc, name)))
				return errmsg0(0);
			
			/* name begins with 'empty.' */
			if (streq32(name, "empty."))
				mtl->isEmpty = 1;
			
		/* gbi */
			/* count then alloc gbi string (if applicable) */
			for (gbi = ss; (gbi = nextline(gbi)); gbi++)
			{
				if (streq32(gbi, "gbi "))
				{
					const char *next = nextline(gbi);
					if (!next) next = gbi + strlen(gbi);
					const char *lt;
					mtl->gbiLen += strcspn(gbi, "\r\n")+1;
					/* allocate extra space for expanding these */
					if ((lt = strstr(gbi, "_loadtexels")) && lt < next)
						mtl->gbiLen += 2048;
				}
				else if (streq32(gbi, "newmtl "))
					break;
			}
			if (mtl->gbiLen
				&& (++mtl->gbiLen)
				&& !(mtl->gbi = malloc(mtl->gbiLen))
			)
				return errmsg(ERR_NOMEM);
			if (mtl->gbi)
				mtl->gbi[0] = '\0';
		/* /gbi */
			
		/* attrib */
			/* count then alloc attrib string (if applicable) */
			for (attrib = ss; (attrib = nextline(attrib)); attrib++)
			{
				if (streq32(attrib, "attrib "))
				{
					const char *next = nextline(attrib);
					if (!next) next = attrib + strlen(attrib);
					const char *lt;
					mtl->attribLen += strcspn(attrib, "\r\n")+1;
				}
				else if (streq32(attrib, "newmtl "))
					break;
			}
			if (mtl->attribLen
				&& (++mtl->attribLen)
				&& !(mtl->attrib = malloc(mtl->attribLen))
			)
				return errmsg(ERR_NOMEM);
			if (mtl->attrib)
				mtl->attrib[0] = '\0';
		/* /attrib */
			
			/* initialize all these to "0" */
			for (int i = 0; i < OBJEX_GBIVAR_NUM; ++i)
			{
				const char *dupMe = "0";
				if (i == OBJEX_GBIVAR_MASKS0) dupMe = "_texel0masks";
				else if (i == OBJEX_GBIVAR_MASKT0) dupMe = "_texel0maskt";
				else if (i == OBJEX_GBIVAR_MASKS1) dupMe = "_texel1masks";
				else if (i == OBJEX_GBIVAR_MASKT1) dupMe = "_texel1maskt";
				/*else if (i == OBJEX_GBIVAR_CMS0) dupMe = "G_TX_NOMIRROR";
				else if (i == OBJEX_GBIVAR_CMS1) dupMe = "G_TX_NOMIRROR";
				else if (i == OBJEX_GBIVAR_CMT0) dupMe = "G_TX_NOMIRROR";
				else if (i == OBJEX_GBIVAR_CMT1) dupMe = "G_TX_NOMIRROR";*/
				if (!(mtl->gbivar[i] = strdup(dupMe)))
					return errmsg(ERR_NOMEM);
			}
		}
		/* new texture */
		else if (streq32(ss, "newtex "))
		{
			const char *name;
			mtl = 0; /* intentionally cleared to 0 */
			
			/* ensure the previous texture has a filename */
			ASSERT_TEX_FILENAME
			
			if (tex && !tex->format)
			{
				/* if previous texture uses 'instead' and a format
				 * has not been specified, default to ci8 */
				if (tex->instead || tex->paletteSlot)
				{
					if (!(tex->format = strdup("ci8")))
						return errmsg(ERR_NOMEM);
				}
			}
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (!(tex = pushtex(objex, calloc, name, 0)))
				return errmsg0(0);
		}
	/* newtex subdirectives */
		else if (streq32(ss, "map "))
		{
			const char *name;
			ASSERT_TEX
			
			if (tex->filename)
				return errmsg(
					"texture '%s': 'map' used multiple times"
					, tex->name
				);
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture filename from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* sanitize slashes */
			char tmp[MAX_PATH];
			strcpy(tmp, name);
			sanitize_slashes(tmp);
			name = tmp;
			
			/* complain about duplicate if another
			 * texture uses this filename */
			if (texture_find_filename(objex, name))
			{
				return errmsg("duplicate texture '%s'", name);
			}
			
			/* filename is just name */
			if (!(tex->filename = calloc(1, strlen(name)+1)))
				return errmsg(ERR_NOMEM);
			strcpy(tex->filename, name);
		}
		/* XXX remove instead later, dropped from spec */
		else if (streq32(ss, "instead ") || streq32(ss, "texturebank "))
		{
			const char *name;
			ASSERT_TEX
			
			if (tex->instead)
				return errmsg(
					"texture '%s': 'texturebank' used multiple times"
					, tex->name
				);
				/*return errmsg(
					"texture '%s': 'instead' used multiple times"
					, tex->name
				);*/
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture filename from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* sanitize slashes */
			char tmp[MAX_PATH];
			strcpy(tmp, name);
			sanitize_slashes(tmp);
			name = tmp;
			
			/* filename is just name */
			if (!(tex->instead = calloc(1, strlen(name)+1)))
				return errmsg(ERR_NOMEM);
			strcpy(tex->instead, name);
		}
		else if (streq32(ss, "format "))
		{
			const char *name;
			ASSERT_TEX
			
			if (tex->format)
				return errmsg(
					"texture '%s': 'format' used multiple times"
					, tex->name
				);
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture format from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* filename is just name */
			if (!(tex->format = calloc(1, strlen(name)+1)))
				return errmsg(ERR_NOMEM);
			strcpy(tex->format, name);
		}
		else if (streq32(ss, "alphamode "))
		{
			const char *name;
			ASSERT_TEX
			
			if (tex->alphamode)
				return errmsg(
					"texture '%s': 'alphamode' used multiple times"
					, tex->name
				);
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch alphamode from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* filename is just name */
			if (!(tex->alphamode = calloc(1, strlen(name)+1)))
				return errmsg(ERR_NOMEM);
			strcpy(tex->alphamode, name);
		}
		/* checking tex first is important b/c this will be invoked
		 * for material priority as well otherwise
		 */
		else if (tex && streq32(ss, "priority "))
		{
			const char *name;
			ASSERT_TEX
			
			if (tex->priority)
				return errmsg(
					"texture '%s': 'priority' used multiple times"
					, tex->name
				);
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture priority from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (sscanf(name, "%d", &tex->priority) != 1)
				return errmsg(
					"could not parse texture priority number '%s'"
					, name
				);
		}
		else if (streq32(ss, "palette "))
		{
			const char *name;
			ASSERT_TEX
			
			if (tex->paletteSlot)
				return errmsg(
					"texture '%s': 'palette' used multiple times"
					, tex->name
				);
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture palette from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (sscanf(name, "%d", &tex->paletteSlot) != 1)
				return errmsg(
					"could not parse texture palette number '%s'"
					, name
				);
			
			if (tex->paletteSlot <= 0)
				return errmsg(
					"invalid palette number '%s' (must be > 0)"
					, name
				);
		}
		else if (streq32(ss, "pointer "))
		{
			const char *name;
			ASSERT_TEX
			
			if (tex->pointer)
				return errmsg(
					"texture '%s': 'pointer' used multiple times"
					, tex->name
				);
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch custom pointer from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (sscanf(name, "%X", &tex->pointer) != 1)
				return errmsg(
					"could not parse custom pointer value '%s'"
					, name
				);
			
			if (tex->pointer <= 0)
				return errmsg(
					"invalid pointer value '%s' (must be > 0)"
					, name
				);
		}
	/* newmtl subdirectives */
		else if (streq32(ss, "priority "))
		{
			const char *name;
			ASSERT_MTL
			
			if (mtl->priority)
				return errmsg(
					"material '%s': 'priority' used multiple times"
					, mtl->name
				);
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch material priority from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (sscanf(name, "%d", &mtl->priority) != 1)
				return errmsg(
					"could not parse material priority number '%s'"
					, name
				);
		}
		else if (streq32(ss, "vertexshading "))
		{
			ASSERT_MTL
			
			const char *name;
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch mode from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (streq32(name, "normal"))
				mtl->vertexshading = OBJEX_VTXSHADE_NORMAL;
			else if (streq32(name, "color"))
				mtl->vertexshading = OBJEX_VTXSHADE_COLOR;
			else if (streq32(name, "alpha"))
				mtl->vertexshading = OBJEX_VTXSHADE_ALPHA;
			else if (streq32(name, "dynamic"))
				mtl->vertexshading = OBJEX_VTXSHADE_DYNAMIC;
			else if (streq32(name, "none"))
				mtl->vertexshading = OBJEX_VTXSHADE_NONE;
			else
				return errmsg("unknown vertexshading mode '%s'", name);
		}
		else if (streq32(ss, "gbi "))
		{
			ASSERT_MTL
			ss += strlen("gbi ");
			strncat(mtl->gbi, ss, strcspn(ss, "\r\n")+1);
		}
		else if (streq32(ss, "attrib "))
		{
			ASSERT_MTL
			ss += strlen("attrib");
			strncat(mtl->attrib, ss, strcspn(ss, "\r\n")+1);
		}
		else if (streq32(ss, "gbivar "))
		{
			ASSERT_MTL
			const char *name;
			const char *varnames[] = {
				"cms0"
				, "cmt0"
				, "masks0"
				, "maskt0"
				, "shifts0"
				, "shiftt0"
				
				, "cms1"
				, "cmt1"
				, "masks1"
				, "maskt1"
				, "shifts1"
				, "shiftt1"
				
				, "" /* OBJEX_GBIVAR_NUM */
			};
			
			if (!(name = nexttok(ss, 1)))
				return errmsg(
					"could not fetch name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			int i;
			for (i = 0; i < OBJEX_GBIVAR_NUM; ++i)
				if (!strcasecmp(varnames[i], name))
					break;
			if (i == OBJEX_GBIVAR_NUM)
				return errmsg("unknown gbivar '%s'", name);
			
			if (!(name = nexttok(ss, 2)))
				return errmsg(
					"could not fetch value from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (mtl->gbivar[i])
				free(mtl->gbivar[i]);
			if (!(mtl->gbivar[i] = strdup(name)))
				return errmsg(ERR_NOMEM);
		}
		else if (streq32(ss, "standalone"))
		{
			ASSERT_MTL
			mtl->isStandalone = 1;
		}
		else if (streq32(ss, "empty"))
		{
			ASSERT_MTL
			mtl->isEmpty = 1;
		}
		else if (streq(ss, "forcewrite") || streq32(ss, "used"/*oldfiles*/))
		{
			if (tex)
			{
				tex->isUsed = 1;
				tex->alwaysUsed = 1;
			
				if (tex->alwaysUnused)
					return errmsg(
						"texture '%s' using both 'forcewrite' and 'forcenowrite' directives"
						" (these cannot be combined)"
						, tex->name
					);
			}
			else if (mtl)
			{
				mtl->isUsed = 1;
				mtl->alwaysUsed = 1;
			}
			else
				return errmsg(
					"directive '%.*s' not preceded by 'newtex'/'newmtl'"
					, strcspn(ss, " \n"), ss
				);
		}
		else if (streq(ss, "forcenowrite") || streq32(ss, "unused"/*oldfiles*/))
		{
			ASSERT_TEX
			
			if (tex->alwaysUsed)
				return errmsg(
					"texture '%s' using both 'forcewrite' and 'forcenowrite' directives"
					" (these cannot be combined)"
					, tex->name
				);
			
			tex->alwaysUnused = 1;
		}
		else if (streq(ss, "map_Kd "))
		{
			const char *name;
			ASSERT_MTL
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* map_Kd just propagates tex0 */
			if (!(mtl->tex0 = pushtex(objex, calloc, name, 1)))
				return errmsg0(0);
			
			/* filename doesn't already exist */
			if (!mtl->tex0->filename)
			{
				/* filename is just name */
				if (!(mtl->tex0->filename = calloc(1, strlen(name)+1)))
					return errmsg(ERR_NOMEM);
				strcpy(mtl->tex0->filename, name);
			}
		}
		else if (streq(ss, "texel0 "))
		{
			const char *name;
			ASSERT_MTL
			
			if (mtl->tex0)
				return errmsg("'texel0' used multiple times");
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (!(mtl->tex0 = texture_find(objex, name)))
				return errmsg("could not locate texture '%s'", name);
		}
		else if (streq(ss, "texel1 "))
		{
			const char *name;
			ASSERT_MTL
			
			if (mtl->tex1)
				return errmsg("'texel1' used multiple times");
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch texture name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (!(mtl->tex1 = texture_find(objex, name)))
				return errmsg("could not locate texture '%s'", name);
		}
		else if (flags & OBJEX_UNKNOWN_DIRECTIVES)
			return errmsg("unknown directive '%.*s'"
				, strcspn(ss, " \n"), ss
			);
	}
	
	if (exportid)
		return errmsg("mtllib contains no exportid");
	
	return objex;
#undef ASSERT_MTL
#undef ASSERT_TEX
#undef ASSERT_TEX_FILENAME
#undef errmsg
}

static void debug_print_bone(struct objex_bone *b)
{
	const char *indent_string = "  ";
	if (!b) return;
	static int indent = 0;
	for (int i = 0; i < indent; ++i)
		debugf(indent_string);
	debugf("+ '%s' %f %f %f\n", b->name, b->x, b->y, b->z);
	++indent;
	debug_print_bone(b->child);
	--indent;
	for (int i = 0; i < indent; ++i)
		debugf(indent_string);
	debugf("-\n");
	debug_print_bone(b->next);
}

/* dump information from objex structure */
static void debuginfo(struct objex *objex)
{
	for (struct objex_skeleton *sk = objex->sk; sk; sk = sk->next)
	{
		debugf("newskel '%s'\n", sk->name);
		debug_print_bone(sk->bone);
	}
	for (struct objex_animation *a = objex->anim; a; a = a->next)
	{
		debugf("newanim '%s' '%s' %d\n"
			, a->sk->name
			, a->name
			, a->frameNum
		);
		struct objex_frame *f;
		for (f = a->frame; f - a->frame < a->frameNum; ++f)
		{
			debugf("loc %f %f %f\n", f->pos.x, f->pos.y, f->pos.z);
			struct objex_xyz *r;
			for (r = f->rot; r - f->rot < a->sk->boneNum; ++r)
				debugf("rot %f %f %f\n", r->x, r->y, r->z);
		}
	}
}

static struct objex_v vert_xform(const struct objex_v *v)
{
	/* NOTE: vert_xform does not account for g->origin; this is
	 *       b/c vertices can be shared across multiple groups;
	 *       such vertex transformations thus must be done non-
	 *       destructively (e.g. while generating vertex buffer)
	 */
	struct objex_v nv = *v;
	if (!nv.weight)
		return nv;
	struct objex_bone *b = nv.weight->bone;
//	if (!strstr(b->name, "Hand"))
//		return nv;
	while (b)
	{
//		debugf("%s: %f %f %f\n", b->name, b->x, b->y, b->z);
		nv.x -= b->x;
		nv.y -= b->y;
		nv.z -= b->z;
		b = b->parent;
	}
	return nv;
}

/* write to .obj, for testing purposes */
static void write_obj(struct objex *objex)
{
//	FILE *fp = fopen("test-objex-out.obj", "wb");
//	if (!fp)
//		return;
	FILE *fp = stdout;
	for (struct objex_v *v = objex->v; v - objex->v < objex->vNum; ++v)
	{
		struct objex_v vT = vert_xform(v);
		vT = *v;
		fprintf(fp, "v %f %f %f\n", vT.x, vT.y, vT.z);
	}
	for (struct objex_vn *vn = objex->vn; vn - objex->vn < objex->vnNum; ++vn)
		fprintf(fp, "vn %f %f %f\n", vn->x, vn->y, vn->z);
	for (struct objex_vt *vt = objex->vt; vt - objex->vt < objex->vtNum; ++vt)
		fprintf(fp, "vt %f %f\n", vt->x, vt->y);
	for (struct objex_g *g = objex->g; g; g = g->next)
	{
		fprintf(fp, "g %s\n", g->name);
		for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
			fprintf(fp, "f %d/%d/%d %d/%d/%d %d/%d/%d\n"
				, f->v.x + 1, f->vt.x + 1, f->vn.x + 1
				, f->v.y + 1, f->vt.y + 1, f->vn.y + 1
				, f->v.z + 1, f->vt.z + 1, f->vn.z + 1
			);
	}
//	fclose(fp);
}

static int v_diff_bones(struct objex_v *v1, struct objex_v *v2)
{
	assert(v1);
	assert(v2);
	
	/* both are 0 */
	if (v1->weight == v2->weight)
		return 0;
	
	/* one is 0 */
	if (!v1->weight || !v2->weight)
		return 1;
	
	/* weight counts don't match */
	if (v1->weightNum != v2->weightNum)
		return 1;
	
	/* first weight doesn't match */
	if (v1->weight->bone != v2->weight->bone)
		return 1;
	
	/* vertex weights match */
	return 0;
}

static int f_diff_bones(
	struct objex *objex
	, struct objex_f *f
	, struct objex_f *f2
)
{
	/* first triangle contains verts for two or more bones */
	if (v_diff_bones(objex->v + f->v.x, objex->v + f->v.y) //x!=y
		|| v_diff_bones(objex->v + f->v.x, objex->v + f->v.z)//x!=z
		|| v_diff_bones(objex->v + f->v.y, objex->v + f->v.z)//y!=z
	)
		return 1;
	
	if (!f2)
		return 0;
	
	/* second triangle contains verts for two or more bones */
	if (v_diff_bones(objex->v + f2->v.x, objex->v + f2->v.y) //x!=y
		|| v_diff_bones(objex->v + f2->v.x, objex->v + f2->v.z)//x!=z
		|| v_diff_bones(objex->v + f2->v.y, objex->v + f2->v.z)//y!=z
	)
		return 1;
	
	/* at this point, f.x == f.y == f.z, and the same for f2,
	 * meaning for the final comparison, we need only compare x
	 */
	
	/* two individual triangles containing different weights
	 * (chunk models like Mario and Hylian Guard)
	 */
	if (v_diff_bones(objex->v + f->v.x, objex->v + f2->v.x))
		return 1;
	
	return 0;
}

static int v_diff_skel(struct objex_v *v1, struct objex_v *v2)
{
	assert(v1);
	assert(v2);
	
	/* both are 0 */
	if (v1->weight == v2->weight)
		return 0;
	
	/* one is 0 */
	if (!v1->weight || !v2->weight)
		return 1;
	
	/* skeletons don't match */
	if (v1->weight->bone->skeleton != v2->weight->bone->skeleton)
		return 1;
	
	/* skeletons match */
	return 0;
}

static int f_diff_skel(
	struct objex *objex
	, struct objex_f *f
	, struct objex_f *f2
)
{
	/* first triangle contains verts for two or more bones */
	if (v_diff_skel(objex->v + f->v.x, objex->v + f->v.y) //x!=y
		|| v_diff_skel(objex->v + f->v.x, objex->v + f->v.z)//x!=z
		|| v_diff_skel(objex->v + f->v.y, objex->v + f->v.z)//y!=z
	)
		return 1;
	
	if (!f2)
		return 0;
	
	/* second triangle contains verts for two or more bones */
	if (v_diff_skel(objex->v + f2->v.x, objex->v + f2->v.y) //x!=y
		|| v_diff_skel(objex->v + f2->v.x, objex->v + f2->v.z)//x!=z
		|| v_diff_skel(objex->v + f2->v.y, objex->v + f2->v.z)//y!=z
	)
		return 1;
	
	/* at this point, f.x == f.y == f.z, and the same for f2,
	 * meaning for the final comparison, we need only compare x
	 */
	
	/* two individual triangles containing different weights
	 * (chunk models like Mario and Hylian Guard)
	 */
	if (v_diff_skel(objex->v + f->v.x, objex->v + f2->v.x))
		return 1;
	
	return 0;
}
	
/* assigns a bone to each triangle by walking every face
 * for every bone to determine the best grouping strategy
 */
static void assign_triangle_bones(
	struct objex_v *v
	, struct objex_g *g
	, struct objex_bone *b
)
{
	assert(v);
	assert(g);
	assert(b);
	
	struct objex_f *f;
	
	/* every bone's group is zeroed by default */
	b->g = 0;
	
	if (b->child)
		assign_triangle_bones(v, g, b->child);
	
	/* walk every face in the group */
	for (f = g->f; f - g->f < g->fNum; ++f)
	{
		struct objex_bone *child;
		
		/* tri already assigned */
		if (f->_pvt)
			continue;
		
		/* tri doesn't contain vertices assigned to this bone */
		if (v[f->v.x].weight->bone != b
			&& v[f->v.y].weight->bone != b
			&& v[f->v.z].weight->bone != b
		)
			continue;
		
		/* walk this bone's immediate children */
		for (child = b->child; child; child = child->next)
		{
			if (v[f->v.x].weight->bone == child
				|| v[f->v.y].weight->bone == child
				|| v[f->v.z].weight->bone == child
			)
				continue;
		}
		
		/* tri contains vertices assigned to immediate child */
		if (child)
			continue;
		
		/* assign triangle to this bone */
		f->_pvt = b;
	}
	
	if (b->next)
		assign_triangle_bones(v, g, b->next);
}

/* sort objex_f by _pvt member */
static int f_sort_pvt(const void *a, const void *b)
{
	const struct objex_f *f0 = a, *f1 = b;
	
	if (f0->_pvt < f1->_pvt)
		return -1;
	else if (f0->_pvt > f1->_pvt)
		return 1;
	return 0;
	return ((intptr_t)f0->_pvt) - ((intptr_t)f1->_pvt);
}

/* sort objex_f by veretx index */
static int f_sort_vidx(const void *a, const void *b)
{
	const struct objex_f *f0 = a, *f1 = b;
	
	return f0->v.x - f1->v.x;
}

static void scale_bone(struct objex_bone *bone, float scale)
{
	if (bone->child) scale_bone(bone->child, scale);
	if (bone->next) scale_bone(bone->next, scale);
	
	bone->x *= scale;
	bone->y *= scale;
	bone->z *= scale;
}

void objex_scale(struct objex *objex, float scale)
{
	/* scale every skeleton in list */
	for (struct objex_skeleton *sk = objex->sk; sk; sk = sk->next)
		scale_bone(sk->bone, scale);
	
	/* scale every translation in animation data */
	for (struct objex_animation *a = objex->anim; a; a = a->next)
	{
		for (struct objex_frame *f = a->frame
			; f - a->frame < a->frameNum
			; ++f
		)
		{
			f->pos.x *= scale;
			f->pos.y *= scale;
			f->pos.z *= scale;
		}
	}
	
	/* scale every vertex in vertex array */
	if (objex->v)
	{
		/* free bone array in each */
		for (int i = 0; i < objex->vNum; ++i)
		{
			objex->v[i].x *= scale;
			objex->v[i].y *= scale;
			objex->v[i].z *= scale;
		}
	}
	
	/* scale every group position */
	if (objex->g)
	{
		for (struct objex_g *g = objex->g; g; g = g->next)
		{
			g->origin.x *= scale;
			g->origin.y *= scale;
			g->origin.z *= scale;
		}
	}
}

/* public functions */
struct objex *objex_load(
	FILE *fopen(const char *, const char *)
	, size_t fread(void *, size_t, size_t, FILE *)
	, void *calloc(size_t, size_t)
	, void *malloc(size_t)
	, void free(void *)
	, int chdir(const char *)
	, int chdirfile(const char *)
	, char *getcwd(void)
	, const char *fn
	, const unsigned skelSeg
	, const float scale
	, enum objex_flag flags
)
{
#define errmsg(fmt, ...) (errmsg)("objex(%d): " fmt, lineNum, ##__VA_ARGS__)
#undef fail
#undef fail__
#define fail__ {                       \
   if (cwd) { chdir(cwd); free(cwd); } \
   if (raw) free(raw);                 \
   if (exportid) free(exportid);       \
   objex_free(objex, free);            \
}
#define fail(fmt, ...) {               \
   fail__                              \
   errmsg(fmt, ## __VA_ARGS__);        \
   return 0;                           \
}
#define fail0(ALWAYS_ZERO) {           \
   fail__                              \
   errmsg0(ALWAYS_ZERO);               \
   return 0;                           \
}
#define ASSERT_EXPORTID \
	if (!exportid) \
		fail("%s missing exportid", fn);
	char *cwd = 0;
	char *raw = 0;
	char *exportid = 0;
	struct objex *objex;
	struct objex_skeleton *active_skeleton = 0;
	struct objex_g *g = 0;
	struct objex_material *mtl = 0;
	struct objex_f *fPrev = 0;
	struct objex_file *file = 0;
	error_reason = ERR_NONE;
	int weightless = 0;
	int weighted = 0;
	int vMax;
	int vnMax;
	int vtMax;
	int vcMax;
	int hasVersion = 0;
	int version = 0;
	int versionMajor = 0;
	int lineNum = 1;
	
	/* allocate objex structure */
	if (!(objex = calloc(1, sizeof(*objex))))
		return errmsg(ERR_NOMEM);
	
	/* back up cwd */
	if (!(cwd = getcwd()))
		fail("failed to retrieve working directory");
	
	/* load objex file and sanitize */
	if (!(raw = loadfile(fopen, fread, malloc, fn, 0, 1)))
		fail(ERR_LOADFILE, fn);
	sanitize(raw);
	
	/* enter its directory (all files ref'd within are relative) */
	if (chdirfile(fn))
		fail(ERR_CHDIRFILE);
	
	/* first pass: count v, vn, vt, vc */
	lineNum = 1;
	for (const char *ss = raw; ss; (ss = nextlineNum(ss, &lineNum)))
	{
		if (streq16(ss, "v "))
			objex->vNum++;
		else if (streq24(ss, "vn "))
			objex->vnNum++;
		else if (streq24(ss, "vt "))
			objex->vtNum++;
		else if (streq24(ss, "vc "))
			objex->vcNum++;
		else if (streq32(ss, "file "))
			objex->fileNum++;
		else if (streq32(ss, "version ") && (hasVersion = 1)
			&& sscanf(ss, "version %d.%d", &version, &versionMajor) != 2
		)
		{
			fail("malformed directive '%.*s'"
				, strcspn(ss, "\r\n"), ss
			);
		}
		else if (streq32(ss, "softinfo "))
		{
			const char *name = nexttok(ss, 1);
			const char *v;
			int pValFail = 0;
			if (!name)
				fail(
					"could not fetch property name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (streq(name, "animation_framerate"))
			{
				if (!(v = nexttok(ss, 2)))
					goto L_pValFail;
				if (!(objex->softinfo.animation_framerate = strdup(v)))
					fail(ERR_NOMEM);
			}
			if (pValFail)
			{
				L_pValFail:
				fail(
					"could not fetch property value from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			}
		}
	}
	
#ifdef NDEBUG
	if (!hasVersion)
		fail("no version number (please use new objex plug-in)");
	if (version != 2 || versionMajor != 0)
		fail("invalid version number %d.%d (upgrade zzconvert or plugin)"
			, version
			, versionMajor
		);
#endif
	
	/* store maxes for later */
	vMax = objex->vNum;
	vnMax = objex->vnNum;
	vtMax = objex->vtNum;
	vcMax = objex->vcNum;
	
	/* no vertices */
	if (!objex->vNum)
		fail("objex file contains no vertices, what's up with that?");
	
	/* if one element is missing, make sure it has at least one */
	objex->vNum += !objex->vNum;
	objex->vnNum += !objex->vnNum;
	objex->vtNum += !objex->vtNum;
	objex->vcNum += !objex->vcNum;
	objex->fileNum += !objex->fileNum;
	
	/* allocate v, vn, vt, vc, file */
	if (
		!(objex->v = calloc(objex->vNum, sizeof(*objex->v)))
		|| !(objex->vn = calloc(objex->vnNum, sizeof(*objex->vn)))
		|| !(objex->vt = calloc(objex->vtNum, sizeof(*objex->vt)))
		|| !(objex->vc = calloc(objex->vcNum, sizeof(*objex->vc)))
		|| !(file = objex->file = calloc(objex->fileNum, sizeof(*objex->file)))
	)
		fail(ERR_NOMEM);
	
	/* parse raw obj */
	objex->fileNum = objex->vNum = objex->vnNum = objex->vtNum = objex->vcNum = 0;
	lineNum = 1;
	for (const char *ss = raw; ss; (ss = nextlineNum(ss, &lineNum)))
	{
//		fprintf(stderr, "'%.*s'\n", (int)strcspn(ss, "\r\n"), ss);
		if (should_skip(ss))
			continue;
		else if (streq16(ss, "v "))
		{
			struct objex_v *v = objex->v + objex->vNum;
			const char *bs;
			const char *F;
			objex->vNum++;
			/*if (
				sscanf(
					ss, F="v %f %f %f " SCNo8 " " SCNo8 " " SCNo8 " " SCNo8
					//ss, "v %f %f %f %hho %hho %hho %hho"
					, &v->x, &v->y, &v->z, &v->r, &v->g, &v->b, &v->a
				) == 7
			)
				v->isColor = 1;
			else */if (sscanf(ss, "v %f %f %f", &v->x, &v->y, &v->z) != 3)
				fail(
					"could not fetch coordinates from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* first pass: num weights */
			for (bs = ss; (bs = linecontainsnq32(bs, "weight")); ++bs)
				v->weightNum++;
			
			/* weights specified but no useskel */
			if (v->weightNum && !active_skeleton)
				fail("v 'weight' directive but no useskel specified");
			
			/* allocate weights */
			if (
				v->weightNum
				&& !(v->weight=calloc(v->weightNum, sizeof(*v->weight)))
			)
				fail(ERR_NOMEM);
			
			/* read weights, influences */
			v->weightNum = 0;
			for (bs = ss; (bs = linecontainsnq32(bs, "weight")); ++bs)
			{
				const char *name = nexttok(bs, 1);
				if (!name)
					fail(
						"could not fetch bone name(s) from '%.*s'"
						, strcspn(bs, "\r\n"), bs
					);
				struct objex_weight *w = v->weight + v->weightNum;
				w->bone = bone_find(active_skeleton, name);
				if (!w->bone)
					fail(
						"could not find bone '%s' in skeleton '%s'"
						, name, active_skeleton->name
					);
				if (
					!(name = nexttok(bs, 2))
					|| sscanf(name, "%f", &w->influence) != 1
				)
					fail(
						"could not fetch bone weight(s) from '%.*s'"
						, strcspn(bs, "\r\n"), bs
					);
				v->weightNum++;
			}
		}
		else if (streq24(ss, "vt "))
		{
			struct objex_vt *vt = objex->vt + objex->vtNum;
			objex->vtNum++;
			if (sscanf(ss, "vt %f %f", &vt->x, &vt->y) != 2)
				fail(
					"could not fetch coordinates from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
		}
		else if (streq24(ss, "vn "))
		{
			struct objex_vn *vn = objex->vn + objex->vnNum;
			objex->vnNum++;
			if (sscanf(ss, "vn %f %f %f", &vn->x, &vn->y, &vn->z) != 3)
				fail(
					"could not fetch coordinates from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
		}
		else if (streq24(ss, "vc "))
		{
			struct objex_vc *vc = objex->vc + objex->vcNum;
			objex->vcNum++;
			float x, y, z, w;
			int r, g, b, a;
			if (sscanf(ss, "vc %f %f %f %f", &x, &y, &z, &w) != 4)
				fail(
					"could not fetch colors + alpha from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			r = round(x * 255.0);
			g = round(y * 255.0);
			b = round(z * 255.0);
			a = round(w * 255.0);
			if (r < 0 || r > 255
				|| g < 0 || g > 255
				|| b < 0 || b > 255
				|| a < 0 || a > 255
			)
				fail(
					"value not in range 0.0 <= n <= 1.0 '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			vc->r = r;
			vc->g = g;
			vc->b = b;
			vc->a = a;
		}
		else if (streq32(ss, "file "))
		{
			const char *tmp;
			const char *comm;
			int ssLen = strcspn(ss, "\r\n");
			file = objex->file + objex->fileNum;
			objex->fileNum++;
			
			/* fetch name */
			if (!(tmp = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch file name from '%.*s'"
					, ssLen, ss
				);
			if (!(file->name = strdup(tmp)))
				fail(ERR_NOMEM);
			
			/* address */
			if ((tmp = nexttok_linerem(ss, 2)) && !sscanf(tmp, "%x", &file->baseOfs))
				return errmsg(
					"could not fetch hexadecimal address from '%.*s'"
					, ssLen, ss
				);
			//fprintf(stderr, "baseOfs = %08x\n", file->baseOfs);
			
			/* common */
			/* TODO: fail if more than one file marked common? */
			if ((comm = strstr(ss, "common")) && comm < ss + ssLen)
				file->isCommon = 1;
		}
		else if (streq32(ss, "useskel "))
		{
			const char *name = nexttok_linerem(ss, 1);
			if (!name)
				fail(
					"could not fetch skeleton name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			active_skeleton = skeleton_find(objex, name);
			if (!active_skeleton)
				fail("could not find skeleton '%s'", name);
		}
		else if (streq32(ss, "exportid "))
		{
			const char *name = nexttok_linerem(ss, 1);
			if (exportid)
				fail("exportid specified multiple times in %s", fn);
			if (!name)
				fail(
					"could not fetch string identifier from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (!(exportid = strdup(name)))
				fail(ERR_NOMEM);
		}
		else if (streq32(ss, "skellib "))
		{
			const char *name = nexttok_linerem(ss, 1);
			char *file;
			
			ASSERT_EXPORTID
			
			if (!name)
				fail(
					"could not fetch filename from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* load skellib file and sanitize */
			if (!(file = loadfile(fopen, fread, malloc, name, 0, 1)))
				fail(ERR_LOADFILE, name);
			sanitize(file);
			
			if (!skellib(objex, calloc, free, file, skelSeg, flags, exportid))
			{
				free(file);
				fail0(0);
			}
			free(file);
		}
		else if (streq32(ss, "animlib "))
		{
			const char *name = nexttok_linerem(ss, 1);
			char *file;
			
			ASSERT_EXPORTID
			
			if (!name)
				fail(
					"could not fetch filename from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* load animlib file and sanitize */
			if (!(file = loadfile(fopen, fread, malloc, name, 0, 1)))
				fail(ERR_LOADFILE, name);
			sanitize(file);
			
			if (!animlib(objex, calloc, free, file, flags, exportid))
			{
				free(file);
				fail0(0);
			}
			free(file);
		}
		else if (streq32(ss, "mtllib "))
		{
			const char *name = nexttok_linerem(ss, 1);
			char *cwd1;
			char *file;
			
			ASSERT_EXPORTID
			
			if (!name)
				fail(
					"could not fetch filename from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (objex->mtl)
				fail("mtllib used multiple times");
			
			/* back up cwd */
			if (!(cwd1 = getcwd()))
				fail("failed to retrieve working directory");
			
			/* load mtllib file and sanitize */
			if (!(file = loadfile(fopen, fread, malloc, name, 0, 1)))
			{
				free(cwd1);
				fail(ERR_LOADFILE, name);
			}
			sanitize(file);
	
			/* enter its directory (files it refs are relative to it) */
			if (chdirfile(name))
			{
				/* failed */
				free(file);
				free(cwd1);
				fail(ERR_CHDIRFILE);
			}
			
			/* store mtllib directory (for loading texture files) */
			if (!(objex->mtllibDir = getcwd()))
				fail("failed to retrieve working directory");
			
			/* parse mtl */
			if (!mtllib(objex, calloc, free, file, flags, exportid))
			{
				/* failed */
				free(file);
				free(cwd1);
				fail0(0);
			}
	
			/* restore cwd, cleanup */
			free(file);
			if (chdir(cwd1))
			{
				free(cwd1);
				fail("failed to restore working directory");
			}
			free(cwd1);
		}
		else if (streq16(ss, "g ") || streq16(ss, "o "))
		{
			const char *name = nexttok_linerem(ss, 1);
			if (!name)
				fail(
					"could not fetch group name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (!(g = g_push_lookahead(objex, calloc, name, ss)))
				fail0(0);
			
			/* zero-initialize */
			g->file = file;
			weightless = 0;
			weighted = 0;
			fPrev = 0;
		}
		else if (streq32(ss, "priority "))
		{
			const char *name;
			
			if (!g)
				return errmsg("'priority' used before 'g'");
			
			if (g->priority)
				return errmsg(
					"group '%s': 'priority' used multiple times"
					, g->name
				);
			
			if (!(name = nexttok_linerem(ss, 1)))
				return errmsg(
					"could not fetch group priority from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			if (sscanf(name, "%d", &g->priority) != 1)
				return errmsg(
					"could not parse group priority number '%s'"
					, name
				);
		}
		else if (streq16(ss, "f "))
		{
			const char *noname = "unnamed";
			/* no group name specified, so make one last-minute */
			if (!g && !(g = g_push_lookahead(objex, calloc, noname, ss)))
				fail0(0);
			
			struct objex_f *f = g->f + g->fNum;
			g->fNum += 1;
			f->mtl = mtl;
			
			/* mtl can be 0 */
			if (mtl)
			{
				mtl->isUsed = 1;
				if (mtl->tex0)
					mtl->tex0->isUsed = 1;
				if (mtl->tex1)
					mtl->tex1->isUsed = 1;
			}
			
			void *generic_slashes(
				const char *str
				, int *v
				, int *vt
				, int *vn
				, int *vc
			)
			{
				int c = 0;
				
				if (!str)
					return 0;
				
				assert(str);
				assert(v);
				assert(vt);
				assert(vn);
				assert(vc);
				
				*v = 0;
				*vt = 0;
				*vn = 0;
				*vc = 0;
				
				for (c = 0; c < 4; ++c)
				{
					int *x = (c == 0) ? v : (c == 1) ? vt : (c == 2) ? vn : vc;
					
					if (sscanf(str, "%d", x) != 1)
						*x = 0;
					
					str = strchr(str, '/');
					if (!str)
						break;
					str += 1;
				}
				
				return success;
			}
			
			if (!generic_slashes(nexttok(ss, 1)
				, &f->v.x, &f->vt.x, &f->vn.x, &f->vc.x)
				|| !generic_slashes(nexttok(ss, 2)
				, &f->v.y, &f->vt.y, &f->vn.y, &f->vc.y)
				|| !generic_slashes(nexttok(ss, 3)
				, &f->v.z, &f->vt.z, &f->vn.z, &f->vc.z)
			)
				fail("malformed directive '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/*fprintf(stderr, "f %d/%d/%d  %d/%d/%d  %d/%d/%d\n"
				, f->v.x, f->vt.x, f->vt.z
				, f->v.y, f->vt.y, f->vt.y
				, f->v.z, f->vt.z, f->vt.z
			);*/
			
//			fprintf(stderr, "directive '%.*s'\n", strcspn(ss, "\r\n"), ss);
//			fprintf(stderr, "v %d %d %d\n", f->v.x, f->v.y, f->v.z);
			
			/* obj indices start at 1; make them start at 0 */
			/* (this also makes unset things -1) */
			f->v.x--; f->v.y--; f->v.z--;
			f->vt.x--; f->vt.y--; f->vt.z--;
			f->vn.x--; f->vn.y--; f->vn.z--;
			f->vc.x--; f->vc.y--; f->vc.z--;
			
			/* assert v range */
			if (f->v.x >= vMax || f->v.y >= vMax || f->v.z >= vMax)
				fail("invalid v (v >= vNum) from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (f->v.x < 0 || f->v.y < 0 || f->v.z < 0)
				fail("failed to fetch v from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			/* assert vt range */
			if (f->vt.x >= vtMax || f->vt.y >= vtMax || f->vt.z >= vtMax)
				fail("invalid vt (vt >= vtNum) from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (f->vt.x < 0 || f->vt.y < 0 || f->vt.z < 0)
				f->vt.x = f->vt.y = f->vt.z = 0;
			
			/* assert vn range */
			if (f->vn.x >= vnMax || f->vn.y >= vnMax || f->vn.z >= vnMax)
				fail("invalid vn (vn >= vnNum) from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			if (f->vn.x < 0 || f->vn.y < 0 || f->vn.z < 0)
				f->vn.x = f->vn.y = f->vn.z = 0;
			
			/* assert vc range */
			if (f->vc.x >= vcMax || f->vc.y >= vcMax || f->vc.z >= vcMax)
				fail("invalid vc (vc >= vcNum) from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			/* vc is allowed to be < 0 */
			
			/* has at least one weight that is 0 */
			if (!weightless && (
					!objex->v[f->v.x].weight
					|| !objex->v[f->v.y].weight
					|| !objex->v[f->v.z].weight
				)
			)
				weightless = 1;
			
			/* has at least one weight that is not 0 */
			if (!weighted && (
					objex->v[f->v.x].weight
					|| objex->v[f->v.y].weight
					|| objex->v[f->v.z].weight
				)
			)
			{
				struct objex_weight *w;
				weighted = 1;
				g->hasWeight = 1;
				
				/* grab skeleton */
				if ((w = objex->v[f->v.x].weight))
					g->skeleton = w->bone->skeleton;
				else if ((w = objex->v[f->v.y].weight))
					g->skeleton = w->bone->skeleton;
				else if ((w = objex->v[f->v.z].weight))
					g->skeleton = w->bone->skeleton;
			}
			
			/* if contains vertices for two or more bones, or this
			 * triangle and previous triangle contain different weights
			 */
			if (!g->hasDeforms && f_diff_bones(objex, f, fPrev))
				g->hasDeforms = 1;
				
			/* these things clash */
			if (weightless && (g->hasDeforms || weighted || g->hasWeight))
				fail("group '%s' contains assigned & unassigned vertices"
					, g->name
				);
			
			/* if contains vertices for two or more skeletons */
			if (f_diff_skel(objex, f, fPrev))
			// OLD TODO: above needs confirmed working before deleting this
			/*if (v_diff_skel(objex->v + f->v.x, objex->v + f->v.y) //x!=y
				|| v_diff_skel(objex->v + f->v.x, objex->v + f->v.z)//x!=z
				|| v_diff_skel(objex->v + f->v.y, objex->v + f->v.z)//y!=z
			)*/
				fail("group '%s' is rigged to multiple skeletons"
					, g->name
				);
			
			/* optional multi-assigned vertex detection;
			 * it would be more efficient to do this when loading the
			 * vertices, but doing it here offers better diagnostics */
			if (flags & OBJEXFLAG_NO_MULTIASSIGN)
			{
				if (objex->v[f->v.x].weightNum > 1
					|| objex->v[f->v.y].weightNum > 1
					|| objex->v[f->v.z].weightNum > 1
				)
					fail("group '%s' contains multi-assigned vertices"
						, g->name
					);
			}
			
			/* store bone */
			if (!fPrev && objex->v[f->v.x].weightNum)
			{
				struct objex_weight *w;
				if ((w = objex->v[f->v.x].weight))
					g->bone = w->bone;
			}
			
			/* for comparing triangle against last triangle */
			fPrev = f;
		}
		else if (streq32(ss, "clearmtl "))
		{
			mtl = 0;
		}
		else if (streq32(ss, "attrib "))
		{
			if (!g)
				fail("'attrib' directive used without 'g' directive");
			ss += strlen("attrib");
			strncat(g->attrib, ss, strcspn(ss, "\r\n")+1);
		}
		else if (streq32(ss, "origin "))
		{
			if (!g)
				fail("'origin' directive used without 'g' directive");
			if (sscanf(ss, "origin %f %f %f"
				, &g->origin.x, &g->origin.y, &g->origin.z
			) != 3)
				fail("malformed directive '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
		}
		else if (streq32(ss, "usemtl "))
		{
			const char *name = nexttok_linerem(ss, 1);
			if (!name)
				fail(
					"could not fetch material name from '%.*s'"
					, strcspn(ss, "\r\n"), ss
				);
			
			mtl = material_find(objex, name);
			if (!mtl)
				fail("could not find material '%s'", name);
			
			/* same material spans multiple files */
			if (mtl->file && mtl->file != file)
				mtl->isMultiFile = 1;
			mtl->file = file;
		}
		else if (flags & OBJEX_UNKNOWN_DIRECTIVES)
			fail("unknown directive '%.*s'", strcspn(ss, " \n"), ss);
	}
	
	/* restore cwd */
	if (chdir(cwd))
		fail("failed to restore working directory");
	
	/* apply scale */
	objex_scale(objex, scale);
	
	/* localize every vertex */
	objex_localize(objex);
	
	/* sort by texture priority */
	if (objex->tex)
		texture_sortByPriority(&objex->tex);
	
	/* sort by group priority */
	if (objex->tex)
		group_sortByPriority(&objex->g);
	
	/* sort by material priority */
	if (objex->mtl)
		material_sortByPriority(&objex->mtl);
	
	/* TODO sort animations by internal indices (order read) */
	
	/* NOTE z64dummy does not prevent group splitting;
	 *      that's what the Pbody attribute is for */
	
//	debuginfo(objex);
//	if (!objex_group_split(objex, objex->g, calloc))
//		fail0(0);
//	write_obj(objex);
	
	/* normal cleanup */
	free(cwd);
	free(raw);
	if (exportid)
		free(exportid);
	return objex;
#undef fail
#undef fail0
#undef fail__
#undef errmsg
}

void objex_localize(struct objex *objex)
{
	struct objex_v *v;
	for (v = objex->v; v - objex->v < objex->vNum; ++v)
		*v = vert_xform(v);
}

// TODO rework (omit?) the whole udata system
extern void *zobj_initObjex(struct objex *obj, const unsigned int baseOfs, FILE *docs);

static int qsortfunc_fileCommonFirst(const void *a_, const void *b_)
{
	const struct objex_file *a = a_, *b = b_;
	
	if (a->isCommon)
		return -1;
	
	return 1;
}

void *objex_divide(struct objex *objex, FILE *docs)
{
	struct objex_file *common = 0;
	
	if (objex->fileNum <= 1)
		return success;
	
	/* divide group list across files */
	for (int i = 0; i < objex->fileNum; ++i)
	{
		struct objex_file *file = objex->file + i;
		struct objex_g **prev = &objex->g;
		struct objex_g *next = 0;
		struct objex *newObj;
		
		/* allocate objex structure */
		if (!(newObj = file->objex = calloc(1, sizeof(*file->objex))))
			return errmsg(ERR_NOMEM);
		
		zobj_initObjex(newObj, file->baseOfs ? file->baseOfs : objex->baseOfs, docs);
		
		if (file->isCommon)
		{
			if (common)
				return errmsg(
					"multiple common files specified: "
					"'%s' and '%s'"
					, common->name, file->name
				);
			common = file;
		}
		
		for (struct objex_g *g = objex->g; g; g = next)
		{
			next = g->next;
			
			if (g->file == file)
			{
				g->objex = newObj;
				
				/* unlink from old list */
				if (next)
				{
					*prev = next;
					prev = &next->next;
				}
				
				/* link into new list */
				g->next = file->head;
				file->head = g;
				newObj->gNum += 1;
				continue;
			}
			
			prev = &g->next;
		}
		
		newObj->g = file->head;
	}
	objex->gNum = 0;
	objex->g = 0;
	
	/* XXX could add logic if no common file, but then the data gets
	 *     duplicated, defeating the purpose of storing multiple files
	 *     in a single objex (aka such a feature has no real use-case)
	 */
	if (!common)
		return errmsg("no common file specified");
	
	/* materials */
	{
		struct objex_material *next = 0;
		
		for (struct objex_material *m = objex->mtl; m; m = next)
		{
			struct objex_file *into = (m->isMultiFile || !m->file) ? common : m->file;
			struct objex *dst = into->objex;
			
			next = m->next;
			
			if (m->tex0)
			{
				/* same texture spans multiple files */
				if (m->tex0->file && m->tex0->file != into)
					m->tex0->isMultiFile = 1;
				m->tex0->file = into;
			}
			
			if (m->tex1)
			{
				/* same texture spans multiple files */
				if (m->tex1->file && m->tex1->file != into)
					m->tex1->isMultiFile = 1;
				m->tex1->file = into;
			}
			
			m->objex = dst;
			m->next = dst->mtl;
			dst->mtl = m;
			dst->mtlNum += 1;
		}
		
		objex->mtl = 0;
		objex->mtlNum = 0;
	}
	
	/* textures and palettes */
	{
		struct objex_texture *next = 0;
		
		for (struct objex_texture *tex = objex->tex; tex; tex = next)
		{
			struct objex_file *into = (tex->isMultiFile || !tex->file) ? common : tex->file;
			struct objex *dst = into->objex;
			
			next = tex->next;
			
			tex->objex = dst;
			tex->next = dst->tex;
			dst->tex = tex;
			
			if (tex->palette)
			{
				tex->palette->objex = dst;
				tex->palette->next = dst->pal;
				dst->pal = tex->palette;
			}
		}
		
		objex->pal = 0;
		objex->tex = 0;
	}
	
	#if 0
	for (int i = 0; i < objex->fileNum; ++i)
	{
		struct objex_file *file = objex->file + i;
		struct objex *objex = file->objex;
		
		fprintf(stderr, "%p %s\n", objex->g, objex->g->name);
	}
	fprintf(stderr, "main: %p\n", objex->g);
	return errmsg("hey");
	#endif
	
	qsort(objex->file, objex->fileNum, sizeof(*objex->file), qsortfunc_fileCommonFirst);
	
	return success;
}

const char *objex_errmsg(void)
{
	return error_reason;
}

/* changes group's skeleton so that every bone that is used
 * for matrices points to the group (0 = fail, non-0 = success)
 */
void *objex_group_matrixBones(
	struct objex *objex
	, struct objex_g *g
)
{
	/* we can't split any of these */
	if (!objex || !g || !g->f || !g->hasDeforms)
		return 0;
	
	struct objex_f *f;
	struct objex_v *v = objex->v;
	struct objex_skeleton *sk = v[g->f->v.x].weight->bone->skeleton;
	
	/* clear private */
	for (f = g->f; f - g->f < g->fNum; ++f)
		f->_pvt = 0;
	
	/* assign every triangle to a bone (also zero every bone's group) */
	assign_triangle_bones(v, g, sk->bone);
	
	/* check for missed triangles */
	for (f = g->f; f - g->f < g->fNum; ++f)
	{
		struct objex_bone *b = f->_pvt;
		
		if (!b)
			return errmsg("missed triangle in group '%s'", g->name);
		
		g->bone = b;
		b->g = g;
	}
	
	/* sort subgroup so triangles sharing vertices are nearer */
	ks_mergesort(fPvt, g->fNum, g->f, 0);
	
	return success;
}

void objex_g_sortByMaterialPriority(struct objex_g *g)
{
	struct objex_f *f;
	int allSamePriority = 1;
	int lastPriority;
	
	if (!g || !g->f || !g->f->mtl)
		return;
	
	/* determine whether they all have the same priority */
	lastPriority = g->f->mtl->priority;
	for (f = g->f; f < g->f + g->fNum; ++f)
	{
		/* _pvt = priority for each */
		f->_pvt = (void*)(intptr_t)-f->mtl->priority;
		if (f->mtl->priority != lastPriority)
			allSamePriority = 0;
	}
	
	/* they do, so no sorting necessary */
	if (allSamePriority)
		return;
	
	/* sort by udata ascending */
	ks_mergesort(fPvt, g->fNum, g->f, 0);
}

static void blankDLs(struct objex_skeleton *sk)
{
	assert(sk);
	
#if 0
	struct
	{
		char isRefd;
		struct objex_bone *b;
	} bone[256];
#endif
	
	struct objex *objex = sk->objex;
	struct objex_skeleton *item;
	
	static unsigned char x_udata[64] = {0};
	static struct objex_g emptyGroup = {
		.udata = x_udata
	};
	if (!x_udata[0])
		memset(x_udata, -1, sizeof(x_udata));
#if 0
	int i;
	
	memset(bone, 0, sizeof(bone));
	
	void boneFunc(struct objex_bone *b)
	{
		bone[i].b = b;
		if (b->g)
			bone[i].isRefd = 1;
		++i;
		
		if (b->child)
			boneFunc(b->child);
		
		if (b->next)
			boneFunc(b->next);
	}
	
	/* get bones into indices */
	i = 0;
	boneFunc(sk->bone);
#endif
	
	/* for every skeleton */
	for (item = objex->sk; item; item = item->next)
	{
		if (!item->parent /* has no parent bone */
			|| item->parent->g /* parent bone already has mesh */
			|| item->parent->skeleton != sk
		)
			continue;
		
		/* new method */
		item->parent->g = &emptyGroup;
	}
	
	/* for every vertex */
	for (struct objex_v *v = objex->v; v - objex->v < objex->vNum; ++v)
	{
		struct objex_bone *b;
		
		if (!v->weight || !(b = v->weight->bone) || b->g)
			continue;
		
#if 0
		bone[b->index].isRefd = 1;
#endif
		/* new method */
		b->g = &emptyGroup;
	}
	
#if 0
	/* make empty-but-referenced bones point to empty mesh */
	for (i = 0; i < sk->boneNum; ++i)
	{
		/* bone is referenced, but group is blank */
		if (bone[i].isRefd && !bone[i].b->g)
		{
			static unsigned char x_udata[64] = {0};
			static struct objex_g x = {
				.udata = x_udata
			};
			if (!x_udata[0])
				memset(x_udata, -1, sizeof(x_udata));
			bone[i].b->g = &x;
//			fprintf(stderr, "uh-oh lol\n");
		}
	}
#endif
}

/* splits a group into multiple groups
 * returns 0 on failure, pointer to modified skeleton on success
 */
struct objex_skeleton *objex_group_split(
	struct objex *objex
	, struct objex_g *g
	, void *calloc(size_t, size_t)
)
{
	/* we can't split any of these */
	if (!objex || !g || !g->f || !g->hasWeight/*!g->hasDeforms*/)
		return 0;
	
	assert(calloc);
	struct objex_f *f;
	struct objex_v *v = objex->v;
	struct objex_skeleton *sk = v[g->f->v.x].weight->bone->skeleton;
	struct objex_g *ng = 0;
	void *last_pvt = 0;
	int mtlNum = objex->mtlNum;
	
	if (sk->boneNum > 256)
		return errmsg("too many bones in skeleton '%s'", sk->name);
	
	/* clear private */
	for (f = g->f; f - g->f < g->fNum; ++f)
		f->_pvt = 0;
	
	/* assign every triangle to a bone (also zero every bone's group) */
	assign_triangle_bones(v, g, sk->bone);
	
	/* check for missed triangles */
	for (f = g->f; f - g->f < g->fNum; ++f)
	{
		if (!f->_pvt)
			return errmsg("missed triangle in group '%s'", g->name);
	}
	
	/* sort based on bone pointers */
	ks_mergesort(fPvt, g->fNum, g->f, 0);
	
	/* make a new group for each bone */
	for (f = g->f; f - g->f < g->fNum; ++f)
	{
		/* group is different from last, so create a new one */
		if (f == g->f || f->_pvt != last_pvt)
		{
			struct objex_bone *b = f->_pvt;
			char *ss;
			
			/* sort subgroup so triangles sharing vertices are nearer */
			if (ng)
			{
				ks_mergesort(fPvt, ng->fNum, ng->f, 0);
			}
			
			/* create new group */
			if (!(ng = g_push(objex, calloc)))
				return errmsg0(0);
			ng->f = f;
			
			/* inherit attributes */
			if (g->attrib && !(ng->attrib = strdup(g->attrib)))
				return errmsg(ERR_NOMEM);
			
			/* do not inherit PROXY attribute */
			if ((ss = strstr(ng->attrib, "PROXY")))
				memset(ss, ' ', strlen("PROXY"));
			
			if (!(ng->name = calloc(1, strlen(g->name)+strlen(b->name)+2)))
				return errmsg(ERR_NOMEM);
			sprintf(ng->name, "%s.%s", g->name, b->name);
			
			b->g = ng;
			ng->bone = b;
			g->hasSplit = 1;
		}
		
		last_pvt = f->_pvt;
		ng->fNum += 1;
		
		/* improved sorting */
		int weights = 0;
		int mtlIndex = (f->mtl) ? f->mtl->index + 1 : 0;
		/* triangles using empty material shoved to end */
		if (f->mtl && f->mtl->isEmpty)
			mtlIndex = mtlNum + 1;
		struct objex_bone *b0 = v[f->v.x].weight->bone;
		struct objex_bone *b1 = v[f->v.y].weight->bone;
		struct objex_bone *b2 = v[f->v.z].weight->bone;
		weights += b0 != b1;
		weights += b1 != b2;
		if (!weights)
		{
			intptr_t index = b0->index;
			f->_pvt = (void*)(mtlIndex * mtlNum + index * 3);
		}
		else
		{
			intptr_t minIndex;
			minIndex = min_int(min_int(b0->index, b1->index), b2->index);
			f->_pvt = (void*)(mtlIndex * mtlNum + minIndex * 3 + weights);
		}
	}
	
	/* sort subgroup so triangles sharing vertices are nearer */
	if (ng)
	{
		ks_mergesort(fPvt, ng->fNum, ng->f, 0);
	}
	
	/* check for bones that vertices reference yet are blank */
	blankDLs(sk);
	
	/* XXX you could get away with sorting by texture, too;
	 *     if (mtl) sort by mtl->tex0; but what produces the
	 *     smallest binary?
	 */
	
	sk->g = g;
	g->bone = sk->bone;
	return sk;
}

void *objex_assert_vertex_boundaries(
	struct objex *objex, long long int min, long long int max
)
{
	if (!objex)
		return success;
	
	for (struct objex_v *v = objex->v; v - objex->v < objex->vNum; ++v)
	{
		if (v->x < min || v->x > max
			|| v->y < min || v->y > max
			|| v->z < min || v->z > max
		) return errmsg(
			"vertices and/or bones out of bounds "
			"(try a smaller scale)"
		);
//		return errmsg("%f %f %f vs %d %d\n", v->x, v->y, v->z, min, max);
	}
	
	return success;
}

/* find a group by name, returning 0 if none is found */
struct objex_g *objex_g_find(struct objex *objex, const char *name)
{
	struct objex_g *g;
	for (g = objex->g; g; g = g->next)
		if (!strcmp(g->name, name))
			return g;
	return 0;
}

/* find a group by index, returning 0 if none is found */
struct objex_g *objex_g_index(struct objex *objex, const int index)
{
	struct objex_g *g;
	for (g = objex->g; g; g = g->next)
		if (g->index == index)
			return g;
	return 0;
}

void objex_resolve_common(struct objex *dst, struct objex *needle, struct objex *haystack)
{
	/* ignore self search */
	if (needle == dst || needle == haystack)
		return;
	
	/* for every texture within needle which also exsists
	 * within haystack and doesn't already exist within dst,
	 * copy into dst and redirect needle and haystack to dst
	 * (if haystack == dst, don't search dst or do the copy)
	 */
	
	struct objex_texture *next = 0;
	for (struct objex_texture *tex = needle->tex; tex; tex = next)
	{
		struct objex_texture *matchHay = texture_findMatch(tex, haystack->tex);
		struct objex_texture *matchDst = (haystack == dst)
			? matchHay
			: texture_findMatch(tex, dst->tex)
		;
		
		next = tex->next;
		
		/* no match found */
		if (matchHay == 0 && matchDst == 0)
			continue;
		
		/* haystack match doesn't already exist in dst, so copy into it */
		if (matchHay != matchDst)
		{
			struct objex_texture *dup = texture_copy(matchHay);
			
			//fprintf(stderr, "found common texture: %s\n", dup->name);
			
			matchDst = dup;
			dup->next = dst->tex;
			dst->tex = dup;
			dup->objex = dst;
		}
		
		/* redirect matches into dst */
		tex->commonRef = matchDst;
		if (matchHay)
			matchHay->commonRef = matchDst;
	}
}

void objex_resolve_common_array(struct objex *dst, struct objex *src[], int srcNum)
{
	assert(dst);
	assert(src);
	assert(srcNum > 0);
	
	for (int i = 0; i < srcNum; ++i)
	{
		/* compare against dst once */
		objex_resolve_common(dst, src[i], dst);
		
		/* compare every index against every other index */
		for (int k = 0; k < srcNum; ++k)
			objex_resolve_common(dst, src[i], src[k]);
	}
}

void objex_free(struct objex *objex, void free(void *))
{
#define free_if(X) if (X) { free(X); X = 0; }
	void *next;
	
	if (!objex)
		return;
	
	/* free skeleton list */
	for (struct objex_skeleton *sk = objex->sk; sk; sk = next)
	{
		next = sk->next;
		if (sk->name) free(sk->name);
		if (sk->extra) free(sk->extra);
		free_bones(sk->bone, free);
		free(sk);
	}
	
	/* free animation list */
	for (struct objex_animation *anim = objex->anim; anim; anim = next)
	{
		next = anim->next;
		if (anim->name) free(anim->name);
		for (int i = 0; i < anim->frameNum; ++i)
			free(anim->frame[i].rot);
		free(anim->frame);
		free_if_udata(anim->udata);
		free(anim);
	}
	
	/* free material list */
	for (struct objex_material *mtl = objex->mtl; mtl; mtl = next)
	{
		next = mtl->next;
		if (mtl->name) free(mtl->name);
		if (mtl->gbi) free(mtl->gbi);
		if (mtl->attrib) free(mtl->attrib);
		for (int i = 0; i < OBJEX_GBIVAR_NUM; ++i)
			if (mtl->gbivar[i])
				free(mtl->gbivar[i]);
		free_if_udata(mtl->udata);
		free(mtl);
	}
	
	/* free texture list */
	for (struct objex_texture *tex = objex->tex; tex; tex = next)
	{
		next = tex->next;
		if (tex->copyOf)
		{
			free(tex);
			continue;
		}
		if (tex->name) free(tex->name);
		if (tex->instead) free(tex->instead);
		if (tex->filename) free(tex->filename);
		if (tex->format) free(tex->format);
		if (tex->pix) free(tex->pix);
		if (tex->alphamode) free(tex->alphamode);
		free_if_udata(tex->udata);
		free(tex);
	}
	
	/* free palette list */
	for (struct objex_palette *pal = objex->pal; pal; pal = next)
	{
		next = pal->next;
		if (pal->colors) free(pal->colors);
		free_if_udata(pal->udata);
		free(pal);
	}
	
	/* free group list */
	for (struct objex_g *g = objex->g; g; g = next)
	{
		next = g->next;
		if (g->name) free(g->name);
		if (g->f && g->fOwns) free(g->f);
		if (g->attrib) free(g->attrib);
		free_if_udata(g->udata);
		free(g);
	}
	
	/* free vertex arrays */
	if (objex->v)
	{
		/* free bone array in each */
		for (int i = 0; i < objex->vNum; ++i)
			if (objex->v[i].weight)
				free(objex->v[i].weight);
		free(objex->v);
	}
	if (objex->vn) free(objex->vn);
	if (objex->vt) free(objex->vt);
	if (objex->vc) free(objex->vc);
	if (objex->file)
	{
		for (int i = 0; i < objex->fileNum; ++i)
		{
			struct objex_file *file = objex->file + i;
			if (file->name)
				free(file->name);
			if (file->objex)
				objex_free(file->objex, free);
		}
		free(objex->file);
	}
	
	/* free softinfo */
	free_if(objex->softinfo.animation_framerate);
	
	if (objex->mtllibDir)
		free(objex->mtllibDir);
	
	free_if_udata(objex->udata);
	free(objex);
}

