#include "doc.h"
#include "streq.h"

typedef struct document_t {
	char *text[2];
	doctype_t type;
	unsigned int offset;
	struct document_t *next;
} document_t;

static document_t *sDocumentHead;
static document_t *sDocumentCur;

void document_assign(const char *textA, const char *textB, unsigned int offset, doctype_t type)
{
	if (sDocumentHead == NULL)
	{
		sDocumentCur = sDocumentHead = calloc(1, sizeof(document_t));
		assert(sDocumentCur != NULL);
	}
	else
	{
		sDocumentCur->next = calloc(1, sizeof(document_t));
		assert(sDocumentCur->next != NULL);
		sDocumentCur = sDocumentCur->next;
	}
	
	sDocumentCur->offset = offset;
	sDocumentCur->type = type;
	
	if (textA)
		sDocumentCur->text[0] = strdup(textA);
	if (textB)
		sDocumentCur->text[1] = strdup(textB);
	
	if (textA && !textB)
	{
		sDocumentCur->type |= DOC_SPACE1;
	}
	else if (textA && textB)
	{
		sDocumentCur->type |= DOC_SPACE2;
	}
	else if (!textA && textB)
	{
		sDocumentCur->type |= DOC_INFO;
	}
}

void document_free(void)
{
	document_t *doc;
	
	sDocumentCur = sDocumentHead;
	
	while ((doc = sDocumentCur) != NULL)
	{
		sDocumentCur = doc->next;
		if (doc->text[0])
			free(doc->text[0]);
		if (doc->text[1])
			free(doc->text[1]);
		free(doc);
	}
	
	sDocumentHead = NULL;
}

void document_mergeDefineHeader(FILE *file)
{
	char *typeTable[] = {
		"WOW_"
		, "MTL_"
		, "DL_"
		, "SKEL_"
		, "TEX_"
		, "PAL_"
		, "ANIM_"
		, "PROXY_"
		, "COLL_"
	};
	document_t *doc = sDocumentHead;
	
	while (doc)
	{
		// Utilize buffer for alignment
		char buffer[512];
		
		if (doc->type & DOC_SPACE1)
		{
			sprintf(
				buffer
				, "%s%s"
				, typeTable[doc->type & 0xF]
				, Canitize(doc->text[0], 1)
			);
			
			fprintf(
				file
				, DOCS_DEF DOCS_SPACE " 0x%08X\n"
				, buffer
				, doc->offset
			);
		}
		if (doc->type & DOC_SPACE2)
		{
			sprintf(
				buffer
				, "%s%s_%s"
				, typeTable[doc->type & 0xF]
				, Canitize(doc->text[0], 1)
				, doc->text[1]
			);
			
			if (doc->type & DOC_INT)
			{
				fprintf(
					file
					, DOCS_DEF DOCS_SPACE " %d\n"
					, buffer
					, doc->offset
				);
			}
			else
			{
				fprintf(
					file
					, DOCS_DEF DOCS_SPACE " 0x%08X\n"
					, buffer
					, doc->offset
				);
			}
		}
		
		doc = doc->next;
	}
}

static const char *document_externify(const char *txt)
{
	static char buf[1024 * 8];
	int i = 0;
	
	while (*txt)
	{
		if (!isalnum(*txt))
		{
			++txt;
			continue;
		}
		
		/* omit any file extensiosn that may be present */
		if (streq32(txt, ".png")
			|| streq32(txt, ".tga")
			|| streq32(txt, ".bmp")
			|| streq32(txt, ".gif")
		)
		{
			txt += 4;
			continue;
		}
		
		/* copy into buffer */
		buf[i] = *txt;
		if (!i) /* first letter is always capitalized */
			buf[i] = toupper(buf[i]);
		++i;
		++txt;
	}
	
	buf[i] = '\0';
	return buf;
}

void document_mergeExternHeader(FILE *header, FILE *linker, FILE* o)
{
	char *typeTable[] = {
		"\0"
		, /* MTL   */ "Gfx* gMtl"
		, /* DL    */ "Gfx* gDL"
		, /* SKEL  */ "void* gSkel"
		, /* TEX   */ "void* gTex"
		, /* PAL   */ "\0"
		, /* ANIM  */ "void* gAnim"
		, /* PROXY */ "\0"
		, /* COLL  */ "void* gColl"
	};
	char *linkTable[] = {
		"\0"
		, /* MTL   */ "gMtl"
		, /* DL    */ "gDL"
		, /* SKEL  */ "gSkel"
		, /* TEX   */ "gTex"
		, /* PAL   */ "\0"
		, /* ANIM  */ "gAnim"
		, /* PROXY */ "\0"
		, /* COLL  */ "gColl"
	};
	document_t *doc = sDocumentHead;
	
	while (doc)
	{
		// Utilize buffer for alignment
		char buffer[512];
		
		if (doc->type & DOC_SPACE1 && (doc->type & 0xF) != T_PAL)
		{
			const char *txt = document_externify(doc->text[0]);
			snprintf(
				buffer
				, sizeof(buffer) / sizeof(*buffer)
				, "%s%s"
				, typeTable[doc->type & 0xF]
				, txt
			);
			
			if (header)
				fprintf(
					header
					, DOCS_EXT "%s" ";\n"
					, buffer
				);
			if (linker)
				fprintf(
					linker
					, "%s%s = 0x%08X;\n"
					, linkTable[doc->type & 0xF]
					, txt
					, doc->offset
				);
		}
		
		doc = doc->next;
	}
}
