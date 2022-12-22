/* binary-header.h: mostly-isolated binary header/footer system for generated assets */

#ifndef BINARY_HEADER_H_INCLUDED
#define BINARY_HEADER_H_INCLUDED 1

#include <stdio.h>
#include <stdlib.h>

#define BINARYHEADER_SIZE 0x30
#define BINARYHEADER_MAX 1024 // using an array for now

enum binaryHeaderFlags
{
	BINHEAD_NONE = 0
	, BINHEAD_DISPLAYLISTS = (1 << 0)
	, BINHEAD_COLLISIONS   = (1 << 1)
	, BINHEAD_SKELETONS    = (1 << 2)
	, BINHEAD_END
	, BINHEAD_NAMES        = (1 << 14) // alternative pointer format: include name strings
	, BINHEAD_FOOTER       = (1 << 15) // write footer instead of header
};

struct binaryHeaderData
{
	const char *name;
	unsigned addr;
	unsigned nameAddr;
	enum binaryHeaderFlags type;
};

static const char *binaryHeaderFlagsFromString(const char *s, enum binaryHeaderFlags *result)
{
	const struct binaryHeaderChar
	{
		const char c;
		const enum binaryHeaderFlags v;
	} arr[] = {
		{ 'd', BINHEAD_DISPLAYLISTS }
		, { 'c', BINHEAD_COLLISIONS }
		, { 's', BINHEAD_SKELETONS }
		, { 'n', BINHEAD_NAMES }
		, { 'f', BINHEAD_FOOTER }
	};
	const int arrNum = sizeof(arr) / sizeof(*arr);
	
	if (!s || !*s)
		return "binary header empty string";
	
	*result = 0;
	while (*s)
	{
		int i = 0;
		
		for (int i = 0; i < arrNum; ++i)
		{
			if (arr[i].c == *s)
			{
				*result |= arr[i].v;
				break;
			}
		}
		
		if (i == arrNum)
		{
			static char buf[256];
			
			snprintf(buf, sizeof(buf), "unrecognized binary header flag '%c'", *s);
			return buf;
		}
		
		++s;
	}
	
	return 0;
}

static void binaryHeaderWriteNames(VFILE *dst, unsigned int baseOfs, struct binaryHeaderData *data, int dataLen)
{
	for (int i = 0; i < dataLen; ++i)
	{
		data->nameAddr = baseOfs + vftell(dst);
		vfputstr0(dst, data->name);
		++data;
	}
}

static const void *binaryHeaderWrite(VFILE *dst, struct objex *obj, unsigned int baseOfs, enum binaryHeaderFlags flags)
{
	struct binaryHeaderData *data = calloc(BINARYHEADER_MAX, sizeof(*data));
	struct binaryHeaderData *dd = data;
	int dataLen = 0;
	unsigned int table[16] = {0}; // segment address to each table
	unsigned int tableCount[16] = {0}; // num items each table
	unsigned int tableAddress = 0; // segment address of main table
	int i;
	int k;
	
	// TODO throw error on failed allocation
	
	/* display lists */
	for (struct objex_g *g = obj->g; g; g = g->next)
	{
		struct groupUdata *ud = g->udata;
		
		if (!ud || !ud->hasWritten || g->isCollisionAddr)
			continue;
		
		dd->type = BINHEAD_DISPLAYLISTS;
		dd->name = g->name;
		dd->addr = baseOfs + ud->dlistOffset;
		++dd;
	}
	/* collision meshes */
	for (struct objex_g *g = obj->g; g; g = g->next)
	{
		if (!g->isCollisionAddr)
			continue;
		
		dd->type = BINHEAD_COLLISIONS;
		dd->name = g->name;
		dd->addr = g->isCollisionAddr;
		++dd;
	}
	
	/* derive length */
	dataLen = dd - data;
	
	/* write names for each */
	if (flags & BINHEAD_NAMES)
		binaryHeaderWriteNames(dst, baseOfs, data, dataLen);
	
	/* write table for each */
	vfalign(dst, 4);
	for (i = k = 1; i < BINHEAD_END; i <<= 1, ++k)
	{
		int j;
		
		for (j = 0; j < dataLen; ++j)
		{
			struct binaryHeaderData *item = data + j;
			
			if (item->type != i)
				continue;
			
			if (table[k] == 0)
				table[k] = baseOfs + vftell(dst);
			vfput32(dst, item->addr);
			if (item->nameAddr)
				vfput32(dst, item->nameAddr);
			tableCount[k] += 1;
		}
	}
	
	/* write main table */
	tableAddress = baseOfs + vftell(dst);
	for (i = k = 1; i < BINHEAD_END; i <<= 1, ++k)
		vfput32(dst, table[k]);
	
	/* write header (or footer) */
	if (!(flags & BINHEAD_FOOTER))
		vfseek(dst, 0, SEEK_SET);
	vfalign(dst, 0x10);
	vfputstr(dst, "z64convert");
	vfalign(dst, 0x02);
	vfput16(dst, flags);
	vfalign(dst, 0x04);
	vfput32(dst, tableAddress); // main table
	for (i = k = 1; i < BINHEAD_END; i <<= 1, ++k)
		vfput16(dst, tableCount[k]); // num items in each
	
	if (data)
		free(data);
	return 0;
}

#endif /* BINARY_HEADER_H_INCLUDED */

