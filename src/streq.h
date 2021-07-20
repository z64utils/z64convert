

#define streq__u32(A) \
	((((A)[0]) << 24) \
	| (((A)[1]) << 16) \
	| (((A)[2]) <<  8) \
	| (((A)[3]) <<  0))
#define streq32(A, B) (((A)[0]) == ((B)[0]) && ((A)[1]) == ((B)[1]) && ((A)[2]) == ((B)[2]) && ((A)[3]) == ((B)[3]))

#define streq__u24(A) \
	(((A)[0]) << 16) \
	| (((A)[1]) <<  8) \
	| (((A)[2]) <<  0))
#define streq24(A, B) (((A)[0]) == ((B)[0]) && ((A)[1]) == ((B)[1]) && ((A)[2]) == ((B)[2]))

#define streq__u16(A) \
	(((A)[0]) <<  8) \
	| (((A)[1]) <<  0))
#define streq16(A, B) (((A)[0]) == ((B)[0]) && ((A)[1]) == ((B)[1]))

#define streq__u8(A) \
	(((A)[0]) <<  0))
#define streq8(A, B) (((A)[0]) == ((B)[0]))

#if 0 /* testing profiling */
#undef streq32
#undef streq24
#undef streq16
#undef streq8

#define streq32 streq
#define streq24 streq
#define streq16 streq
#define streq8  streq
#endif



