#if !defined(ZCONF_H)
#define ZCONF_H
#if defined(Z_PREFIX)
#define deflateInit_ z_deflateInit_
#define deflate z_deflate
#define deflateEnd z_deflateEnd
#define inflateInit_ z_inflateInit_
#define inflate z_inflate
#define inflateEnd z_inflateEnd
#define deflateInit2_ z_deflateInit2_
#define deflateSetDictionary z_deflateSetDictionary
#define deflateCopy z_deflateCopy
#define deflateReset z_deflateReset
#define deflateParams z_deflateParams
#define deflateBound z_deflateBound
#define deflatePrime z_deflatePrime
#define inflateInit2_ z_inflateInit2_
#define inflateSetDictionary z_inflateSetDictionary
#define inflateSync z_inflateSync
#define inflateSyncPoint z_inflateSyncPoint
#define inflateCopy z_inflateCopy
#define inflateReset z_inflateReset
#define inflateBack z_inflateBack
#define inflateBackEnd z_inflateBackEnd
#define compress z_compress
#define compress2 z_compress2
#define compressBound z_compressBound
#define uncompress z_uncompress
#define adler32 z_adler32
#define crc32 z_crc32
#define get_crc_table z_get_crc_table
#define zError z_zError
#define alloc_func z_alloc_func
#define free_func z_free_func
#define in_func z_in_func
#define out_func z_out_func
#define Byte z_Byte
#define uInt z_uInt
#define uLong z_uLong
#define Bytef z_Bytef
#define charf z_charf
#define intf z_intf
#define uIntf z_uIntf
#define uLongf z_uLongf
#define voidpf z_voidpf
#define voidp z_voidp
#endif
#if defined(__MSDOS__) && !defined(MSDOS)
#define MSDOS
#endif
#if (defined(OS_2) || defined(__OS2__)) && !defined(OS2)
#define OS2
#endif
#if defined(_WINDOWS) && !defined(WINDOWS)
#define WINDOWS
#endif
#if defined(_WIN32) || defined(_WIN32_WCE) || defined(__WIN32__)
#if !defined(WIN32)
#define WIN32
#endif
#endif
#if (defined(MSDOS) || defined(OS2) || defined(WINDOWS)) && !defined(WIN32)
#if !defined(__GNUC__) && !defined(__FLAT__) && !defined(__386__)
#if !defined(SYS16BIT)
#define SYS16BIT
#endif
#endif
#endif
#if defined(SYS16BIT)
#define MAXSEG_64K
#endif
#if defined(MSDOS)
#define UNALIGNED_OK
#endif
#if defined(__STDC_VERSION__)
#if !defined(STDC)
#define STDC
#endif
#if __STDC_VERSION__ >= 199901L
#if !defined(STDC99)
#define STDC99
#endif
#endif
#endif
#if !defined(STDC) && (defined(__STDC__) || defined(__cplusplus))
#define STDC
#endif
#if !defined(STDC) && (defined(__GNUC__) || defined(__BORLANDC__))
#define STDC
#endif
#if !defined(STDC) && (defined(MSDOS) || defined(WINDOWS) || defined(WIN32))
#define STDC
#endif
#if !defined(STDC) && (defined(OS2) || defined(__HOS_AIX__))
#define STDC
#endif
#if defined(__OS400__) && !defined(STDC)
#define STDC
#endif
#if !defined(STDC)
#if !defined(const)
#define const
#endif
#endif
#if defined(__MWERKS__)||defined(applec)||defined(THINK_C)||defined(__SC__)
#define NO_DUMMY_DECL
#endif
#if !defined(MAX_MEM_LEVEL)
#if defined(MAXSEG_64K)
#define MAX_MEM_LEVEL 8
#else
#define MAX_MEM_LEVEL 9
#endif
#endif
#if !defined(MAX_WBITS)
#define MAX_WBITS 15
#endif
#if !defined(OF)
#if defined(STDC)
#define OF(args) args
#else
#define OF(args) ()
#endif
#endif
#if defined(SYS16BIT)
#if defined(M_I86SM) || defined(M_I86MM)
#define SMALL_MEDIUM
#if defined(_MSC_VER)
#define FAR _far
#else
#define FAR far
#endif
#endif
#if (defined(__SMALL__) || defined(__MEDIUM__))
#define SMALL_MEDIUM
#if defined(__BORLANDC__)
#define FAR _far
#else
#define FAR far
#endif
#endif
#endif
#if defined(WINDOWS) || defined(WIN32)
#if defined(ZLIB_DLL)
#if defined(WIN32) && (!defined(__BORLANDC__) || (__BORLANDC__ >= 0x500))
#if defined(ZLIB_INTERNAL)
#define ZEXTERN extern __declspec(dllexport)
#else
#define ZEXTERN extern __declspec(dllimport)
#endif
#endif
#endif
#if defined(ZLIB_WINAPI)
#if defined(FAR)
#undef FAR
#endif
#include <windows.h>
#define ZEXPORT WINAPI
#if defined(WIN32)
#define ZEXPORTVA WINAPIV
#else
#define ZEXPORTVA FAR CDECL
#endif
#endif
#endif
#if defined (__BEOS__)
#if defined(ZLIB_DLL)
#if defined(ZLIB_INTERNAL)
#define ZEXPORT __declspec(dllexport)
#define ZEXPORTVA __declspec(dllexport)
#else
#define ZEXPORT __declspec(dllimport)
#define ZEXPORTVA __declspec(dllimport)
#endif
#endif
#endif
#if !defined(ZEXTERN)
#define ZEXTERN extern
#endif
#if !defined(ZEXPORT)
#define ZEXPORT
#endif
#if !defined(ZEXPORTVA)
#define ZEXPORTVA
#endif
#if !defined(FAR)
#define FAR
#endif
#if !defined(__MACTYPES__)
typedef unsigned char Byte;
#endif
typedef unsigned int uInt;
typedef unsigned long uLong;
#if defined(SMALL_MEDIUM)
#define Bytef Byte FAR
#else
typedef Byte FAR Bytef;
#endif
typedef char FAR charf;
typedef int FAR intf;
typedef uInt FAR uIntf;
typedef uLong FAR uLongf;
#if defined(STDC)
typedef void const *voidpc;
typedef void FAR *voidpf;
typedef void *voidp;
#else
typedef Byte const *voidpc;
typedef Byte FAR *voidpf;
typedef Byte *voidp;
#endif
#if 0
#include <sys/types.h>
#include <unistd.h>
#if defined(VMS)
#include <unixio.h>
#endif
#define z_off_t off_t
#endif
#if !defined(SEEK_SET)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#if !defined(z_off_t)
#define z_off_t long
#endif
#if defined(__OS400__)
#define NO_vsnprintf
#endif
#if defined(__MVS__)
#define NO_vsnprintf
#if defined(FAR)
#undef FAR
#endif
#endif
#if defined(__MVS__)
#pragma map(deflateInit_,"DEIN")
#pragma map(deflateInit2_,"DEIN2")
#pragma map(deflateEnd,"DEEND")
#pragma map(deflateBound,"DEBND")
#pragma map(inflateInit_,"ININ")
#pragma map(inflateInit2_,"ININ2")
#pragma map(inflateEnd,"INEND")
#pragma map(inflateSync,"INSY")
#pragma map(inflateSetDictionary,"INSEDI")
#pragma map(compressBound,"CMBND")
#pragma map(inflate_table,"INTABL")
#pragma map(inflate_fast,"INFA")
#pragma map(inflate_copyright,"INCOPY")
#endif
#endif
