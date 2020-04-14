#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS

#if defined(__cplusplus)
extern "C" {
#else
extern
#endif
void GG_LwipPlatformDiag(const char* fmt, ...);
#if defined(__cplusplus)
}
#endif
#define LWIP_PLATFORM_DIAG(x) do { GG_LwipPlatformDiag x;} while (0)
