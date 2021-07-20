
#define WOW_IMPLEMENTATION
#include <wow.h>

#define STBI_FAILURE_USERMSG 1
#define STBI_MALLOC    wow_malloc_die
#define STBI_REALLOC   wow_realloc_die
#define STBI_FREE      wow_free
#define STB_IMAGE_IMPLEMENTATION

#ifdef NDEBUG /* release versions support extra formats */
#define STBI_NO_JPEG
//        STBI_NO_PNG
//        STBI_NO_BMP
#define STBI_NO_PSD
//        STBI_NO_TGA
//        STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM //   (.ppm and .pgm)
#else
#	define STBI_ONLY_PNG /* test versions support only PNG, for build speed */
#endif

#include "stb_image.h"

