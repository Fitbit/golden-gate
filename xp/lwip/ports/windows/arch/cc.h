// disable some warnings
#pragma warning(disable: 4103) /* structure packing changed by including file */

// function options
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS

// default byte order
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

// printf constants
#define X8_F  "02x"
#define U16_F "hu"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"

#define S16_F "hd"
#define X16_F "hx"
#define SZT_F "lu"

// use bpstruct.h and epstruct.h
#define PACK_STRUCT_USE_INCLUDES
