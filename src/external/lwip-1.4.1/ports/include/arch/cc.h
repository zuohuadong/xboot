#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Include some files for defining library routines
 */
#include <xboot.h>

/*
 * Define generic types used in lwIP
 */
typedef size_t	mem_ptr_t;

/*
 * Define (sn)printf formatters for these lwIP types
 */
#define X8_F	"02x"
#define U16_F	"hu"
#define S16_F	"hd"
#define X16_F	"hx"
#define U32_F	"u"
#define S32_F	"d"
#define X32_F	"x"

/*
 * Compiler hints for packing structures
 */
#define PACK_STRUCT_FIELD(x)	x
#define PACK_STRUCT_STRUCT		__attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/*
 * Plaform specific diagnostic output
 */
#define LWIP_PLATFORM_DIAG(x)	do { logger_print x; } while(0)
#define LWIP_PLATFORM_ASSERT(x)	do { logger_print("Assertion '%s' failed at line %d in %s\n", x, __LINE__, __FILE__); } while(0)

#define LWIP_RAND()				((u32_t)rand())
//#define LWIP_TIMEVAL_PRIVATE	0

#ifdef __cplusplus
}
#endif

#endif /* __ARCH_CC_H__ */
