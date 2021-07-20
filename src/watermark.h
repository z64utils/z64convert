#include <stdlib.h>
#include <ctype.h>
				
/* data list */
struct list
{
	void          *udata;
	void          *data;
	void          *next;
	char          *fn;
	int            w;
	int            h;
	unsigned int   sz;
};

struct valNum
{
	int  val;
	int  num;
	int  most;
};
							
static unsigned char *is_unique(unsigned char *b, int want)
{
	int i;
	int k;
	/* test 0 through 5/6 */
	for (i=0; i < want; ++i)
	{
		/* check 5/6 bytes against themselves */
		for (k = i+1; k < want; ++k)
		{
			if (b[i] == b[k])
				return 0;
		}
	}
	return b;
}


static int valNum_descend(const void *aa, const void *bb)
{
	const struct valNum *a = aa;
	const struct valNum *b = bb;
	
	return (a->num > b->num) ? -1 : (a->num < b->num) ? 1 : 0;
}

static int valNum_ascend(const void *aa, const void *bb)
{
	const struct valNum *a = aa;
	const struct valNum *b = bb;
	
	return (a->num > b->num) ? 1 : (a->num < b->num) ? -1 : 0;
}

static int valNum_ascend_val(const void *aa, const void *bb)
{
	const struct valNum *a = aa;
	const struct valNum *b = bb;
	
	return (a->val > b->val) ? 1 : (a->val < b->val) ? -1 : 0;
}

/* watermark ci texture data*/
static void watermark(void *pal, unsigned pal_bytes, unsigned pal_max, struct list *rme)
{
	/* ensure palette size ends with 0 or 8 */
	if (pal_bytes & 7)
		pal_bytes += 8 - (pal_bytes & 7);
	
	if ((pal_bytes/2) >= 'Z')
	{
		//fprintf(stderr, "attempt watermark\n");
		
		struct list *item;
		int max_unique = 0;
		unsigned char *unique = 0;
		
		/* step through every texture; we want one with *
		 * a pattern of 5 unique bytes in a row "Z64ME" *
		 * or 6 for "z64.me"                            */
		for (item = rme; item; item = item->next)
		{
			unsigned char *b;
			unsigned char *s = item->data;
			int want = 6;
			int want2 = 5; /* want - 1 */
			want = 5; /* go with just z64me */
			
			
			struct valNum vn[256] = {0};
			/* set index value of each */
			for (int k = 0; k < pal_max; ++k)
			{
				vn[k].val = k;
				vn[k].num = 0;
			}
			
			/* count number of each index */
			for (b = s; b - s < item->sz; ++b)
				vn[*b].num += 1;
			
			/* sort them */
			qsort(vn, 256, sizeof(vn[0]), valNum_descend);
			
			/* derive "most" */
			for (int k = 0; k < pal_max; ++k)
				vn[k].most = k;
			
			/* sort back */
			qsort(vn, 256, sizeof(vn[0]), valNum_ascend_val);
			
			/* cut off before end just to be safe */
			for (b = s; b - s < (item->sz - (want+2)); ++b)
			{
				/* found a good or better match */
				unsigned char *x = is_unique(b, want);
				
				/* skip if in top 8 most common */
				if (x && vn[*x].most > 8 && (x == b || *(b-1) != *b) && b[want] != b[want-1])
				{
				//	fprintf(stderr, "%d isn't many\n", vn[*x].most);
					max_unique = want;
					unique = x;
					/* found a perfect match */
					if (max_unique == want)
						break;
				}
			}
			
			/* found a perfect match */
			if (max_unique == want)
				break;
		}
		
		/* an acceptable match was found */
		if (unique)
		{
			//fprintf(stderr, "watermark successful\n");
			unsigned char lut[256];
			int i;
			
			/* the lut begins with standard order */
			for (i = 0; i < 256; ++i)
				lut[i] = i;
			
			/* determine how to rearrange indices */
			int lower_ok = 0;
			for (i = 0; i < max_unique; ++i)
			{
				int nidx;
				if (i == 0) nidx = 'Z';
				if (i == 1) nidx = '6';
				if (i == 2) nidx = '4';
				if (max_unique == 5)
				{
					if (i == 3) nidx = 'M';
					if (i == 4) nidx = 'E';
				}
				else /* 6 */
				{
					if (i == 3) nidx = '.';
					if (i == 4) nidx = 'M';
					if (i == 5) nidx = 'E';
				}
				
				/* use lowercase if available */
				/* applies to the whole string */
				if (i == 0 && tolower(nidx)*2 < pal_bytes)
					lower_ok = 1;
				if (lower_ok)
					nidx = tolower(nidx);
				int old = unique[i];
				lut[unique[i]] = nidx;
				lut[nidx] = old;
			}
			
			/* now rearrange every texture */
			for (item = rme; item; item = item->next)
			{
				unsigned int i;
				unsigned char *b = item->data;
				for (i = 0; i < item->sz; ++i)
					b[i] = lut[b[i]];
			}
			
			/* now rearrange palette colors */
			unsigned short *b = pal;
			unsigned short Nb[512];
			for (i = 0; i < pal_bytes / 2; ++i)
				Nb[i] = b[lut[i]];
			memcpy(b, Nb, pal_bytes);
		}
	}
}

