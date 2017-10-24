#pragma once
#if ! MORE_FRAMEWORK_INCLUDES
#include <MacTypes.h>
#include <CodeFragments.h>
#include <Devices.h>
#include <CFBundle.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
typedef pascal OSStatus (*CFMLateImportLookupProc)(ConstStr255Param symName, CFragSymbolClass symClass,
void **symAddr, void *refCon);
extern pascal OSStatus CFMLateImportCore(const CFragSystem7DiskFlatLocator *fragToFixLocator,
CFragConnectionID fragToFixConnID,
CFragInitFunction fragToFixInitRoutine,
ConstStr255Param weakLinkedLibraryName,
CFMLateImportLookupProc lookup,
void *refCon);
extern pascal OSStatus CFMLateImportLibrary(const CFragSystem7DiskFlatLocator *fragToFixLocator,
CFragConnectionID fragToFixConnID,
CFragInitFunction fragToFixInitRoutine,
ConstStr255Param weakLinkedLibraryName,
CFragConnectionID connIDToImport);
extern pascal OSStatus CFMLateImportBundle(const CFragSystem7DiskFlatLocator *fragToFixLocator,
CFragConnectionID fragToFixConnID,
CFragInitFunction fragToFixInitRoutine,
ConstStr255Param weakLinkedLibraryName,
CFBundleRef bundleToImport);
#if defined(__cplusplus)
}
#endif
