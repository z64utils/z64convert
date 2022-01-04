#ifndef __Z64DOC_H__
#define __Z64DOC_H__
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "err.h"
#include <assert.h>

typedef enum {
	DOC_SPACE1 = 1 << 4,
	DOC_SPACE2 = 1 << 5,
	DOC_INFO   = 1 << 6,
	DOC_INT    = 1 << 7,
	
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
} doctype_t;

void document_assign(const char* textA, const char* textB, unsigned int offset, doctype_t type);
void document_free();

void document_mergeDefineHeader(FILE* file);

#endif

