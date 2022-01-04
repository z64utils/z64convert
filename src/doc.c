#include "doc.h"

typedef struct document_t {
	char *text[2];
	doctype_t type;
	unsigned int offset;
	struct document_t *next;
} document_t;

document_t *sDocumentHead;
document_t *sDocumentCur;

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

void document_free()
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

static char *document_externify(char *txt)
{
	char* buf = calloc(1, strlen(txt) + 8);
	int i = 0, j = 0;
	
	strcpy(buf, txt);
	
	i = 0;
	
	while (txt[i] != '\0')
	{
		
		if (j == 0)
		{
			buf[j++] = toupper(txt[i++]);
			continue;
		}
		
		if (txt[i] == '.')
			if (txt[i + 1] == 'p')
				if (txt[i + 2] == 'n')
					if (txt[i + 3] == 'g')
					{
						i = i + 4;
					}
		if (txt[i] == 'Z')
			if (txt[i + 1] == 'Z')
				if (txt[i + 2] == 'P')
					if (txt[i + 3] == 'A')
					{
						i = i + 5;
					}
		
		if (txt[i] < 'A' || txt[i] > 'Z')
		{
			if (txt[i] < 'a' || txt[i] > 'z')
			{
				if (txt[i] < '0' || txt[i] > '9')
				{
					i++;
					txt[i] = toupper(txt[i]);
					
					continue;
				}
			}
		}

		buf[j] = txt[i];
		buf[j + 1] = '\0';
		
		i++;
		j++;
	}
	
	return buf;
}

void document_mergeExternHeader(FILE *file)
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
	document_t *doc = sDocumentHead;
	
	while (doc)
	{
		// Utilize buffer for alignment
		char buffer[512];
		
		if (doc->type & DOC_SPACE1 && (doc->type & 0xF) != T_PAL)
		{
			char* txt = document_externify(doc->text[0]);
			sprintf(
				buffer
				, "%s%s"
				, typeTable[doc->type & 0xF]
				, txt
			);
			
			if (txt)
				free(txt);
			fprintf(
				file
				, DOCS_EXT "%s" ";\n"
				, buffer
			);
		}
		
		doc = doc->next;
	}
}