#include "doc.h"

typedef struct document_t {
	char* text[2];
	doctype_t    type;
	unsigned int offset;
	struct document_t* next;
} document_t;

document_t* sDocumentHead;
document_t* sDocumentCur;

void document_doc(const char* textA, const char* textB, unsigned int offset, doctype_t type) {
	if (sDocumentHead == NULL) {
		sDocumentCur = sDocumentHead = calloc(1, sizeof(document_t));
		assert(sDocumentCur != NULL);
	} else {
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
	
	if (textA && !textB) {
		sDocumentCur->type |= DOC_SPACE1;
	} else if (textA && textB) {
		sDocumentCur->type |= DOC_SPACE2;
	} else if (!textA && textB) {
		sDocumentCur->type |= DOC_INFO;
	}
}

void document_free() {
	document_t* doc;
	
	sDocumentCur = sDocumentHead;
	
	while ((doc = sDocumentCur) != NULL) {
		sDocumentCur = doc->next;
		if (doc->text[0])
			free(doc->text[0]);
		if (doc->text[1])
			free(doc->text[1]);
		free(doc);
	}
	
	sDocumentHead = NULL;
}

void document_define_header(FILE* file) {
	char* typeTable[] = {
		"WOW_",
		
		"MTL_",
		"DL_",
		"SKEL_",
		"TEX_",
		"PAL_",
		"ANIM_",
		"PROXY_",
		"COLL_",
	};
	document_t* doc = sDocumentHead;
	
	while (doc) {
		// Utilize buffer for alignment
		char buffer[512];
		
		if (doc->type & DOC_SPACE1) {
			sprintf(
				buffer,
				"%s%s",
				typeTable[doc->type & 0xF],
				Canitize(doc->text[0], 1)
			);
			
			fprintf(
				file,
				DOCS_DEF DOCS_SPACE " 0x%08X\n",
				buffer,
				doc->offset
			);
		}
		if (doc->type & DOC_SPACE2) {
			sprintf(
				buffer,
				"%s%s_%s",
				typeTable[doc->type & 0xF],
				Canitize(doc->text[0], 1),
				doc->text[1]
			);
			
			if (doc->type & DOC_INT) {
				fprintf(
					file,
					DOCS_DEF DOCS_SPACE " %d\n",
					buffer,
					doc->offset
				);
			} else {
				fprintf(
					file,
					DOCS_DEF DOCS_SPACE " 0x%08X\n",
					buffer,
					doc->offset
				);
			}
		}
		
		doc = doc->next;
	}
}