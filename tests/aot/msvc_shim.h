#define __INT8_TYPE__ signed char
#define __INT16_TYPE__ short
#define __INT32_TYPE__ int
#define __INT64_TYPE__ __int64
#define __UINT8_TYPE__ unsigned char
#define __UINT16_TYPE__ unsigned short
#define __UINT32_TYPE__ unsigned int
#define __UINT64_TYPE__ unsigned __int64
#ifdef _WIN64
#define __UINTPTR_TYPE__ unsigned __int64
#define __SIZE_TYPE__ unsigned __int64
#else
#define __UINTPTR_TYPE__ unsigned int
#define __SIZE_TYPE__ unsigned int
#endif
#define __builtin_expect(x,y) (x)
#define restrict __restrict
