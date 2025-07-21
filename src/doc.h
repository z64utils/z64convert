#ifndef __Z64DOC_H__
#define __Z64DOC_H__
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "err.h"
#include <assert.h>

typedef enum {
	DOC_SPACE1 = 1 << 4,
	DOC_SPACE2 = 1 << 5,
	DOC_INFO   = 1 << 6,
	DOC_INT    = 1 << 7,
	DOC_ENUM   = 1 << 8,
	T_NONE     = 0,
	T_MTL,
	T_DL,
	T_SKEL,
	T_TEX,
	T_PAL,
	T_ANIM,
	T_PROXY,
	T_COLL,
	T_JOINTCOUNT,
	T_ENUM,
} doctype_t;

void document_setFileName(const char* file);
void document_assign(const char* textA, const char* textB, unsigned int offset, doctype_t type);
void document_free();

void document_mergeDefineHeader(FILE* file);
void document_mergeExternHeader(FILE* header, FILE* linker, FILE* o, bool usePrefixes);

#endif
