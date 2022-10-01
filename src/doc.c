#include "doc.h"
#include "streq.h"

typedef struct document_t {
    char*              text[2];
    doctype_t          type;
    unsigned int       offset;
    struct document_t* next;
} document_t;

static document_t* sDocumentHead;
static document_t* sDocumentCur;
static const char* sFileName;

void document_setFileName(const char* file) {
    sFileName = file;
}

void document_assign(const char* textA, const char* textB, unsigned int offset, doctype_t type) {
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
    
    if (type & DOC_ENUM)
        return;
    if (textA && !textB) {
        sDocumentCur->type |= DOC_SPACE1;
    } else if (textA && textB) {
        sDocumentCur->type |= DOC_SPACE2;
    } else if (!textA && textB) {
        sDocumentCur->type |= DOC_INFO;
    }
}

void document_free(void) {
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

static const char*document_basename() {
    static char buffer[1024 * 8];
    int point = 0;
    int slash = 0;
    int strSize = strlen(sFileName);
    
    for (int i = strSize; i > 0; i--) {
        if (point == 0 && sFileName[i] == '.') {
            point = i;
        }
        if (sFileName[i] == '/' || sFileName[i] == '\\') {
            slash = i;
            break;
        }
    }
    
    if (slash == 0)
        slash = -1;
    
    memset(buffer, 0, point - slash);
    memcpy(buffer, &sFileName[slash + 1], point - slash - 1);
    
    return buffer;
}

void document_mergeDefineHeader(FILE* file) {
    char* typeTable[] = {
        "WOW_",
        "MTL_",
        "DL_",
        "SKEL_",
        "TEX_",
        "PAL_",
        "ANIM_",
        "PROXY_",
        "COLL_"
    };
    document_t* doc = sDocumentHead;
    
    while (doc)	{
        // Utilize buffer for alignment
        char buffer[512];
        
        if (doc->type & DOC_SPACE1)	{
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
        if (doc->type & DOC_SPACE2)	{
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

static const char*document_externify(const char* txt) {
    static char buf[1024 * 8];
    int i = 0;
    
    while (*txt) {
        if (*txt == '.') {
            /* omit any file extensiosn that may be present */
            if (
                streq32(txt, ".png")
                || streq32(txt, ".tga")
                || streq32(txt, ".bmp")
                || streq32(txt, ".gif")
            ) {
                txt += 4;
                continue;
            }
        }
        
        if (!isalnum(*txt))	{
            ++txt;
            continue;
        }
        
        /* copy into buffer */
        buf[i] = *txt;
        if (i && !isalnum(txt[-1]))
            buf[i] = toupper(buf[i]);
        if (!i) /* first letter is always capitalized */
            buf[i] = toupper(buf[i]);
        ++i;
        ++txt;
    }
    
    buf[i] = '\0';
    
    return buf;
}

static char* UnCanitize(const char* txt) {
    static char buf[512];
    int w = 0;
    
    memset(buf, 0, 512);
    
    while (!isalpha(*txt)) txt++;
    
    for (int i = 0; i < strlen(txt); i++) {
        if (ispunct(txt[i]))
            continue;
        
        if (i == 0) {
            buf[w++] = toupper(txt[i]);
        } else {
            if (txt[i - 1] == '_')
                buf[w++] = toupper(txt[i]);
            else
                buf[w++] = txt[i];
        }
    }
    
    return buf;
}

void document_mergeExternHeader(FILE* header, FILE* linker, FILE* o) {
    char* typeTable[] = {
        "\0",
        /* MTL   */ "Gfx",
        /* DL    */ "Gfx",
        /* SKEL  */ "FlexSkeletonHeader",
        /* TEX   */ "u16",
        /* PAL   */ "\0",
        /* ANIM  */ "AnimationHeader",
        /* PROXY */ "\0",
        /* COLL  */ "CollisionHeader"
    };
    char* linkTable[] = {
        "\0",
        /* MTL   */ "Mtl",
        /* DL    */ "Dl",
        /* SKEL  */ "Skel",
        /* TEX   */ "Tex",
        /* PAL   */ "\0",
        /* ANIM  */ "Anim",
        /* PROXY */ "\0",
        /* COLL  */ "Coll"
    };
    document_t* doc = sDocumentHead;
    char* basename = document_basename();
    
    fprintf(
        header,
        "#ifndef __%s_H__\n"
        "#define __%s_H__\n\n",
        Canitize(basename, 1),
        Canitize(basename, 1)
    );
    
    while (doc)	{
        // Utilize buffer for alignment
        char buffer[512];
        
        if (doc->type & DOC_SPACE1 && (doc->type & 0xF) != T_PAL) {
            const char* txt = document_externify(doc->text[0]);
            snprintf(
                buffer,
                sizeof(buffer) / sizeof(*buffer),
                "%s g%s_%s%s",
                typeTable[doc->type & 0xF],
                UnCanitize(basename),
                linkTable[doc->type & 0xF],
                txt
            );
            
            if (header)
                fprintf(
                    header,
                    DOCS_EXT "%s" ";\n",
                    buffer
                );
            if (linker)
                fprintf(
                    linker,
                    "g%s_%s%s = 0x%08X;\n",
                    UnCanitize(basename),
                    linkTable[doc->type & 0xF],
                    txt,
                    doc->offset
                );
        } else if (doc->type & DOC_SPACE2) {
            if (doc->type & DOC_INT && (doc->type & 0xF) == T_SKEL)	{
                char* typeTable[] = {
                    "WOW_",
                    "MTL_",
                    "DL_",
                    "SKEL_",
                    "TEX_",
                    "PAL_",
                    "ANIM_",
                    "PROXY_",
                    "COLL_"
                };
                sprintf(
                    buffer,
                    "%s%s_%s",
                    typeTable[doc->type & 0xF],
                    Canitize(doc->text[0], 1),
                    doc->text[1]
                );
                fprintf(
                    header,
                    DOCS_DEF DOCS_SPACE " %d\n",
                    buffer,
                    doc->offset
                );
            }
        } else if (doc->type & DOC_ENUM) {
            if (doc->type & T_ENUM) {
                if (doc->offset == 0)
                    fprintf(header, "\ntypedef enum {\n");
                else
                    fprintf(header, "} Skel%sLimbs;\n\n", doc->text[0]);
                
            } else {
                char* skelname;
                
                skelname = strdup(Canitize(doc->text[0], 1));
                fprintf(header, "    %s_LIMB_%s,\n", skelname, Canitize(doc->text[1], 1));
                
                free(skelname);
            }
        }
        
        doc = doc->next;
    }
    
    fprintf(
        header,
        "\n#endif /* __%s_H__ */\n",
        Canitize(basename, 1)
    );
}
