#define MoreAssert(x) (true)
#define MoreAssertQ(x)
#if ! MORE_FRAMEWORK_INCLUDES
#include <CodeFragments.h>
#include <PEFBinaryFormat.h>
#endif
#include <string.h>
#define MoreBlockZero BlockZero
#include "CFMLateImport.h"
#if TARGET_RT_MAC_MACHO
#error CFMLateImport is not suitable for use in a Mach-O project.
#elif !TARGET_RT_MAC_CFM || !TARGET_CPU_PPC
#error CFMLateImport has not been qualified for 68K or CFM-68K use.
#endif
#pragma mark ----- Utility Routines -----
static OSStatus FSReadAtOffset(SInt16 refNum, SInt32 offset, SInt32 count, void *buffer)
{
ParamBlockRec pb;
pb.ioParam.ioRefNum = refNum;
pb.ioParam.ioBuffer = (Ptr) buffer;
pb.ioParam.ioReqCount = count;
pb.ioParam.ioPosMode = fsFromStart;
pb.ioParam.ioPosOffset = offset;
return PBReadSync(&pb);
}
#pragma mark ----- Late Import Engine -----
struct FragToFixInfo {
CFragSystem7DiskFlatLocator locator;
CFragConnectionID connID;
CFragInitFunction initRoutine;
PEFContainerHeader containerHeader;
PEFSectionHeader *sectionHeaders;
PEFLoaderInfoHeader *loaderSection;
SInt16 fileRef;
void *section0Base;
void *section1Base;
Boolean disposeSectionPointers;
};
typedef struct FragToFixInfo FragToFixInfo;
static OSStatus ReadContainerBasics(FragToFixInfo *fragToFix)
{
OSStatus err;
UInt16 sectionIndex;
Boolean found;
MoreAssertQ(fragToFix != nil);
MoreAssertQ(fragToFix->locator.fileSpec != nil);
MoreAssertQ(fragToFix->connID != nil);
MoreAssertQ(fragToFix->loaderSection == nil);
MoreAssertQ(fragToFix->sectionHeaders == nil);
MoreAssertQ(fragToFix->fileRef == 0);
fragToFix->disposeSectionPointers = true;
err = FSpOpenDF(fragToFix->locator.fileSpec, fsRdPerm, &fragToFix->fileRef);
if (err == noErr) {
err = FSReadAtOffset(fragToFix->fileRef,
fragToFix->locator.offset,
sizeof(fragToFix->containerHeader),
&fragToFix->containerHeader);
if (err == noErr) {
if ( fragToFix->containerHeader.tag1 != kPEFTag1
|| fragToFix->containerHeader.tag2 != kPEFTag2
|| fragToFix->containerHeader.architecture != kCompiledCFragArch
|| fragToFix->containerHeader.formatVersion != kPEFVersion) {
err = cfragFragmentFormatErr;
}
}
if (err == noErr) {
fragToFix->sectionHeaders = (PEFSectionHeader *) NewPtr(fragToFix->containerHeader.sectionCount * sizeof(PEFSectionHeader));
err = MemError();
}
if (err == noErr) {
err = FSReadAtOffset(fragToFix->fileRef,
fragToFix->locator.offset + sizeof(fragToFix->containerHeader),
fragToFix->containerHeader.sectionCount * sizeof(PEFSectionHeader),
fragToFix->sectionHeaders);
}
if (err == noErr) {
sectionIndex = 0;
found = false;
while ( sectionIndex < fragToFix->containerHeader.sectionCount && ! found ) {
found = (fragToFix->sectionHeaders[sectionIndex].sectionKind == kPEFLoaderSection);
if ( ! found ) {
sectionIndex += 1;
}
}
}
if (err == noErr && ! found) {
err = cfragNoSectionErr;
}
if (err == noErr) {
fragToFix->loaderSection = (PEFLoaderInfoHeader *) NewPtr(fragToFix->sectionHeaders[sectionIndex].containerLength);
err = MemError();
}
if (err == noErr) {
err = FSReadAtOffset(fragToFix->fileRef,
fragToFix->locator.offset + fragToFix->sectionHeaders[sectionIndex].containerOffset,
fragToFix->sectionHeaders[sectionIndex].containerLength,
fragToFix->loaderSection);
}
}
return err;
}
static UInt32 DecodeVCountValue(const UInt8 *start, UInt32 *outCount)
{
UInt8 * bytePtr;
UInt8 byte;
UInt32 count;
bytePtr = (UInt8 *)start;
count = 0;
do {
byte = *bytePtr++;
count = (count << kPEFPkDataVCountShift) | (byte & kPEFPkDataVCountMask);
} while ((byte & kPEFPkDataVCountEndMask) != 0);
*outCount = count;
return bytePtr - start;
}
static UInt32 DecodeInstrCountValue(const UInt8 *inOpStart, UInt32 *outCount)
{
MoreAssertQ(inOpStart != nil);
MoreAssertQ(outCount != nil);
if (PEFPkDataCount5(*inOpStart) != 0) {
*outCount = PEFPkDataCount5(*inOpStart);
return 1;
} else {
return 1 + DecodeVCountValue(inOpStart + 1, outCount);
}
}
static OSStatus UnpackPEFDataSection(const UInt8 * const packedData, UInt32 packedSize,
UInt8 * const unpackedData, UInt32 unpackedSize) {
OSErr err;
UInt32 offset;
UInt8 opCode;
UInt8 * unpackCursor;
MoreAssertQ(packedData != nil);
MoreAssertQ(unpackedData != nil);
MoreAssertQ(unpackedSize >= packedSize);
MoreAssertQ( packedSize == GetPtrSize( (Ptr) packedData ) );
MoreAssertQ( unpackedSize == GetPtrSize( (Ptr) unpackedData) );
err = noErr;
offset = 0;
unpackCursor = unpackedData;
while (offset < packedSize) {
MoreAssertQ(unpackCursor < &unpackedData[unpackedSize]);
opCode = packedData[offset];
switch (PEFPkDataOpcode(opCode)) {
case kPEFPkDataZero: {
UInt32 count;
offset += DecodeInstrCountValue(&packedData[offset], &count);
MoreBlockZero(unpackCursor, count);
unpackCursor += count;
}
break;
case kPEFPkDataBlock: {
UInt32 blockSize;
offset += DecodeInstrCountValue(&packedData[offset], &blockSize);
BlockMoveData(&packedData[offset], unpackCursor, blockSize);
unpackCursor += blockSize;
offset += blockSize;
}
break;
case kPEFPkDataRepeat: {
UInt32 blockSize;
UInt32 repeatCount;
UInt32 loopCounter;
offset += DecodeInstrCountValue(&packedData[offset], &blockSize);
offset += DecodeVCountValue(&packedData[offset], &repeatCount);
repeatCount += 1;
for (loopCounter = 0; loopCounter < repeatCount; loopCounter++) {
BlockMoveData(&packedData[offset], unpackCursor, blockSize);
unpackCursor += blockSize;
}
offset += blockSize;
}
break;
case kPEFPkDataRepeatBlock: {
UInt32 commonSize;
UInt32 customSize;
UInt32 repeatCount;
const UInt8 *commonData;
const UInt8 *customData;
UInt32 loopCounter;
offset += DecodeInstrCountValue(&packedData[offset], &commonSize);
offset += DecodeVCountValue(&packedData[offset], &customSize);
offset += DecodeVCountValue(&packedData[offset], &repeatCount);
commonData = &packedData[offset];
customData = &packedData[offset + commonSize];
for (loopCounter = 0; loopCounter < repeatCount; loopCounter++) {
BlockMoveData(commonData, unpackCursor, commonSize);
unpackCursor += commonSize;
BlockMoveData(customData, unpackCursor, customSize);
unpackCursor += customSize;
customData += customSize;
}
BlockMoveData(commonData, unpackCursor, commonSize);
unpackCursor += commonSize;
offset += (repeatCount * (commonSize + customSize)) + commonSize;
}
break;
case kPEFPkDataRepeatZero: {
UInt32 commonSize;
UInt32 customSize;
UInt32 repeatCount;
const UInt8 *customData;
UInt32 loopCounter;
offset += DecodeInstrCountValue(&packedData[offset], &commonSize);
offset += DecodeVCountValue(&packedData[offset], &customSize);
offset += DecodeVCountValue(&packedData[offset], &repeatCount);
customData = &packedData[offset];
for (loopCounter = 0; loopCounter < repeatCount; loopCounter++) {
MoreBlockZero(unpackCursor, commonSize);
unpackCursor += commonSize;
BlockMoveData(customData, unpackCursor, customSize);
unpackCursor += customSize;
customData += customSize;
}
MoreBlockZero(unpackCursor, commonSize);
unpackCursor += commonSize;
offset += repeatCount * customSize;
}
break;
default:
#if MORE_DEBUG
DebugStr("\pUnpackPEFDataSection: Unexpected data opcode");
#endif
err = cfragFragmentCorruptErr;
goto leaveNow;
break;
}
}
leaveNow:
return err;
}
struct TVector {
void *codePtr;
void *tocPtr;
};
typedef struct TVector TVector;
static OSStatus SetupSectionBaseAddresses(FragToFixInfo *fragToFix)
{
OSStatus err;
TVector * relocatedExport;
SInt32 initSection;
UInt32 initOffset;
PEFSectionHeader * initSectionHeader;
Ptr packedDataSection;
Ptr unpackedDataSection;
TVector originalOffsets;
packedDataSection = nil;
unpackedDataSection = nil;
relocatedExport = (TVector *) fragToFix->initRoutine;
err = noErr;
initSection = fragToFix->loaderSection->initSection;
initOffset = fragToFix->loaderSection->initOffset;
if (initSection == -1) {
err = cfragFragmentUsageErr;
}
if (err == noErr) {
MoreAssertQ( initSection >= 0 );
MoreAssertQ( initSection < fragToFix->containerHeader.sectionCount );
initSectionHeader = &fragToFix->sectionHeaders[initSection];
if ( initSectionHeader->sectionKind == kPEFPackedDataSection ) {
packedDataSection = NewPtr(initSectionHeader->containerLength);
err = MemError();
if (err == noErr) {
unpackedDataSection = NewPtr(initSectionHeader->unpackedLength);
err = MemError();
}
if (err == noErr) {
err = FSReadAtOffset( fragToFix->fileRef,
fragToFix->locator.offset
+ initSectionHeader->containerOffset,
initSectionHeader->containerLength,
packedDataSection);
}
if (err == noErr) {
err = UnpackPEFDataSection( (UInt8 *) packedDataSection, initSectionHeader->containerLength,
(UInt8 *) unpackedDataSection, initSectionHeader->unpackedLength);
}
if (err == noErr) {
BlockMoveData(unpackedDataSection + initOffset, &originalOffsets, sizeof(TVector));
}
} else {
MoreAssertQ(fragToFix->sectionHeaders[initSection].sectionKind == kPEFUnpackedDataSection);
err = FSReadAtOffset(fragToFix->fileRef,
fragToFix->locator.offset
+ fragToFix->sectionHeaders[initSection].containerOffset
+ initOffset,
sizeof(TVector),
&originalOffsets);
}
}
if (err == noErr) {
fragToFix->section0Base = ((char *) relocatedExport->codePtr) - (UInt32) originalOffsets.codePtr;
fragToFix->section1Base = ((char *) relocatedExport->tocPtr) - (UInt32) originalOffsets.tocPtr;
}
if (packedDataSection != nil) {
DisposePtr(packedDataSection);
MoreAssertQ( MemError() == noErr );
}
if (unpackedDataSection != nil) {
DisposePtr(unpackedDataSection);
MoreAssertQ( MemError() == noErr );
}
return err;
}
static void *GetSectionBaseAddress(const FragToFixInfo *fragToFix, UInt16 sectionIndex)
{
void *result;
MoreAssertQ(fragToFix != nil);
MoreAssertQ(fragToFix->containerHeader.tag1 == kPEFTag1);
switch (sectionIndex) {
case 0:
result = fragToFix->section0Base;
break;
case 1:
result = fragToFix->section1Base;
break;
default:
result = nil;
break;
}
return result;
}
static OSStatus FindImportLibrary(PEFLoaderInfoHeader *loaderSection, const char *libraryName, PEFImportedLibrary **importLibrary)
{
OSStatus err;
UInt32 librariesRemaining;
PEFImportedLibrary *thisImportLibrary;
Boolean found;
MoreAssertQ(loaderSection != nil);
MoreAssertQ(libraryName != nil);
MoreAssertQ(importLibrary != nil);
thisImportLibrary = (PEFImportedLibrary *) (loaderSection + 1);
librariesRemaining = loaderSection->importedLibraryCount;
found = false;
while ( librariesRemaining > 0 && ! found ) {
found = (strcmp( libraryName,
((char *)loaderSection)
+ loaderSection->loaderStringsOffset
+ thisImportLibrary->nameOffset) == 0);
if ( ! found ) {
thisImportLibrary += 1;
librariesRemaining -= 1;
}
}
if (found) {
*importLibrary = thisImportLibrary;
err = noErr;
} else {
*importLibrary = nil;
err = cfragNoLibraryErr;
}
return err;
}
static OSStatus LookupSymbol(CFMLateImportLookupProc lookup, void *refCon,
PEFLoaderInfoHeader *loaderSection,
UInt32 symbolIndex,
UInt32 *symbolValue)
{
OSStatus err;
UInt32 *importSymbolTable;
UInt32 symbolStringOffset;
Boolean symbolIsWeak;
CFragSymbolClass symbolClass;
char *symbolStringAddress;
Str255 symbolString;
MoreAssertQ(lookup != nil);
MoreAssertQ(loaderSection != nil);
MoreAssertQ(symbolIndex < loaderSection->totalImportedSymbolCount);
MoreAssertQ(symbolValue != nil);
importSymbolTable = (UInt32 *)(((char *)(loaderSection + 1)) + (loaderSection->importedLibraryCount * sizeof(PEFImportedLibrary)));
symbolStringOffset = importSymbolTable[symbolIndex];
symbolClass = PEFImportedSymbolClass(symbolStringOffset);
symbolIsWeak = ((symbolClass & kPEFWeakImportSymMask) != 0);
symbolClass = symbolClass & ~kPEFWeakImportSymMask;
symbolStringOffset = PEFImportedSymbolNameOffset(symbolStringOffset);
symbolStringAddress = ((char *)loaderSection) + loaderSection->loaderStringsOffset + symbolStringOffset;
symbolString[0] = strlen(symbolStringAddress);
BlockMoveData(symbolStringAddress, &symbolString[1], symbolString[0]);
err = lookup(symbolString, symbolClass, (void **) symbolValue, refCon);
if (err != noErr) {
*symbolValue = 0;
if (symbolIsWeak) {
err = noErr;
}
}
return err;
}
struct EngineState {
UInt32 currentReloc;
UInt32 terminatingReloc;
UInt32 *sectionBase;
UInt32 *relocAddress;
UInt32 importIndex;
void *sectionC;
void *sectionD;
};
typedef struct EngineState EngineState;
static OSStatus InitEngineState(const FragToFixInfo *fragToFix,
UInt16 relocHeaderIndex,
EngineState *state)
{
OSStatus err;
PEFLoaderRelocationHeader *relocHeader;
MoreAssertQ(fragToFix != nil);
MoreAssertQ(state != nil);
relocHeader = (PEFLoaderRelocationHeader *) (((char *) fragToFix->loaderSection) + fragToFix->loaderSection->relocInstrOffset - relocHeaderIndex * sizeof(PEFLoaderRelocationHeader));
MoreAssertQ(relocHeader->reservedA == 0);
state->currentReloc = relocHeader->firstRelocOffset;
state->terminatingReloc = relocHeader->firstRelocOffset + relocHeader->relocCount;
state->sectionBase = (UInt32 *) GetSectionBaseAddress(fragToFix, relocHeader->sectionIndex);
state->relocAddress = state->sectionBase;
state->importIndex = 0;
state->sectionC = GetSectionBaseAddress(fragToFix, 0);
if (state->sectionC != nil) {
#if MORE_DEBUG
if (fragToFix->sectionHeaders[0].defaultAddress != 0) {
DebugStr("\pInitEngineState: Executing weird case.");
}
#endif
(char *) state->sectionC -= fragToFix->sectionHeaders[0].defaultAddress;
}
state->sectionD = GetSectionBaseAddress(fragToFix, 1);
if (state->sectionD != nil) {
#if MORE_DEBUG
if (fragToFix->sectionHeaders[1].defaultAddress != 0) {
DebugStr("\pInitEngineState: Executing weird case.");
}
#endif
(char *) state->sectionD -= fragToFix->sectionHeaders[1].defaultAddress;
}
err = noErr;
if (state->relocAddress == nil) {
err = cfragFragmentUsageErr;
}
return err;
}
static UInt8 kPEFRelocBasicOpcodes[kPEFRelocBasicOpcodeRange] = { PEFMaskedBasicOpcodes };
static OSStatus RunRelocationEngine(const FragToFixInfo *fragToFix,
PEFImportedLibrary *importLibrary,
CFMLateImportLookupProc lookup, void *refCon)
{
OSStatus err;
EngineState state;
UInt16 sectionsLeftToRelocate;
UInt32 totalRelocs;
UInt16 *relocInstrTable;
UInt16 opCode;
MoreAssertQ(fragToFix != nil);
MoreAssertQ(fragToFix->containerHeader.tag1 == kPEFTag1);
MoreAssertQ(fragToFix->sectionHeaders != nil);
MoreAssertQ(fragToFix->loaderSection != nil);
MoreAssertQ(fragToFix->section0Base != nil);
MoreAssertQ(fragToFix->section1Base != nil);
MoreAssertQ(importLibrary != nil);
MoreAssertQ(lookup != nil);
totalRelocs = (fragToFix->loaderSection->loaderStringsOffset - fragToFix->loaderSection->relocInstrOffset) / sizeof(UInt16);
relocInstrTable = (UInt16 *)((char *) fragToFix->loaderSection + fragToFix->loaderSection->relocInstrOffset);
MoreAssertQ(fragToFix->loaderSection->relocSectionCount <= 0x0FFFF);
sectionsLeftToRelocate = fragToFix->loaderSection->relocSectionCount;
err = noErr;
while ( sectionsLeftToRelocate > 0 ) {
err = InitEngineState(fragToFix, sectionsLeftToRelocate, &state);
if (err != noErr) {
goto leaveNow;
}
while ( state.currentReloc != state.terminatingReloc ) {
MoreAssertQ( state.currentReloc < totalRelocs );
opCode = relocInstrTable[state.currentReloc];
switch ( PEFRelocBasicOpcode(opCode) ) {
case kPEFRelocBySectDWithSkip: {
UInt16 skipCount;
UInt16 relocCount;
skipCount = ((opCode >> 6) & 0x00FF);
relocCount = (opCode & 0x003F);
state.relocAddress += skipCount;
state.relocAddress += relocCount;
}
break;
case kPEFRelocBySectC:
case kPEFRelocBySectD: {
UInt16 runLength;
runLength = (opCode & 0x01FF) + 1;
state.relocAddress += runLength;
}
break;
case kPEFRelocTVector12: {
UInt16 runLength;
runLength = (opCode & 0x01FF) + 1;
state.relocAddress += (runLength * 3);
}
break;
case kPEFRelocTVector8:
case kPEFRelocVTable8: {
UInt16 runLength;
runLength = (opCode & 0x01FF) + 1;
state.relocAddress += (runLength * 2);
}
break;
case kPEFRelocImportRun: {
UInt32 symbolValue;
UInt16 runLength;
runLength = (opCode & 0x01FF) + 1;
while (runLength > 0) {
if ( state.importIndex >= importLibrary->firstImportedSymbol && state.importIndex < (importLibrary->firstImportedSymbol + importLibrary->importedSymbolCount) ) {
err = LookupSymbol(lookup, refCon, fragToFix->loaderSection, state.importIndex, &symbolValue);
if (err != noErr) {
goto leaveNow;
}
*(state.relocAddress) += symbolValue;
}
state.importIndex += 1;
state.relocAddress += 1;
runLength -= 1;
}
}
break;
case kPEFRelocSmByImport: {
UInt32 symbolValue;
UInt32 index;
index = (opCode & 0x01FF);
if ( index >= importLibrary->firstImportedSymbol && index < (importLibrary->firstImportedSymbol + importLibrary->importedSymbolCount) ) {
err = LookupSymbol(lookup, refCon, fragToFix->loaderSection, index, &symbolValue);
if (err != noErr) {
goto leaveNow;
}
*(state.relocAddress) += symbolValue;
}
state.importIndex = index + 1;
state.relocAddress += 1;
}
break;
case kPEFRelocSmSetSectC: {
UInt32 index;
index = (opCode & 0x01FF);
state.sectionC = GetSectionBaseAddress(fragToFix, index);
MoreAssertQ(state.sectionC != nil);
}
break;
case kPEFRelocSmSetSectD: {
UInt32 index;
index = (opCode & 0x01FF);
state.sectionD = GetSectionBaseAddress(fragToFix, index);
MoreAssertQ(state.sectionD != nil);
}
break;
case kPEFRelocSmBySection:
state.relocAddress += 1;
break;
case kPEFRelocIncrPosition: {
UInt16 offset;
offset = (opCode & 0x0FFF) + 1;
((char *) state.relocAddress) += offset;
}
break;
case kPEFRelocSmRepeat:
#if MORE_DEBUG
DebugStr("\pRunRelocationEngine: kPEFRelocSmRepeat not yet implemented");
#endif
err = unimpErr;
goto leaveNow;
break;
case kPEFRelocSetPosition: {
UInt32 offset;
state.currentReloc += 1;
offset = PEFRelocSetPosFullOffset(opCode, relocInstrTable[state.currentReloc]);
state.relocAddress = (UInt32 *) ( ((char *) state.sectionBase) + offset);
}
break;
case kPEFRelocLgByImport: {
UInt32 symbolValue;
UInt32 index;
state.currentReloc += 1;
index = PEFRelocLgByImportFullIndex(opCode, relocInstrTable[state.currentReloc]);
if ( index >= importLibrary->firstImportedSymbol && index < (importLibrary->firstImportedSymbol + importLibrary->importedSymbolCount) ) {
err = LookupSymbol(lookup, refCon, fragToFix->loaderSection, index, &symbolValue);
if (err != noErr) {
goto leaveNow;
}
*(state.relocAddress) += symbolValue;
}
state.importIndex = index + 1;
state.relocAddress += 1;
}
break;
case kPEFRelocLgRepeat:
#if MORE_DEBUG
DebugStr("\pRunRelocationEngine: kPEFRelocLgRepeat not yet implemented");
#endif
err = unimpErr;
goto leaveNow;
break;
case kPEFRelocLgSetOrBySection:
#if MORE_DEBUG
DebugStr("\pRunRelocationEngine: kPEFRelocLgSetOrBySection not yet implemented");
#endif
err = unimpErr;
goto leaveNow;
break;
case kPEFRelocUndefinedOpcode:
err = cfragFragmentCorruptErr;
goto leaveNow;
break;
default:
MoreAssertQ(false);
err = cfragFragmentCorruptErr;
goto leaveNow;
break;
}
state.currentReloc += 1;
}
sectionsLeftToRelocate -= 1;
}
leaveNow:
return err;
}
extern pascal OSStatus CFMLateImportCore(const CFragSystem7DiskFlatLocator *fragToFixLocator,
CFragConnectionID fragToFixConnID,
CFragInitFunction fragToFixInitRoutine,
ConstStr255Param weakLinkedLibraryName,
CFMLateImportLookupProc lookup,
void *refCon)
{
OSStatus err;
OSStatus junk;
FragToFixInfo fragToFix;
PEFImportedLibrary *importLibrary;
char weakLinkedLibraryNameCString[256];
MoreAssertQ(fragToFixLocator != nil);
MoreAssertQ(fragToFixConnID != nil);
MoreAssertQ(fragToFixInitRoutine != nil);
MoreAssertQ(weakLinkedLibraryName != nil);
MoreAssertQ(lookup != nil);
MoreBlockZero(&fragToFix, sizeof(fragToFix));
fragToFix.locator = *fragToFixLocator;
fragToFix.connID = fragToFixConnID;
fragToFix.initRoutine = fragToFixInitRoutine;
BlockMoveData(weakLinkedLibraryName + 1, weakLinkedLibraryNameCString, weakLinkedLibraryName[0]);
weakLinkedLibraryNameCString[weakLinkedLibraryName[0]] = 0;
err = ReadContainerBasics(&fragToFix);
if (err == noErr) {
err = SetupSectionBaseAddresses(&fragToFix);
}
if (err == noErr) {
err = FindImportLibrary(fragToFix.loaderSection, weakLinkedLibraryNameCString, &importLibrary);
}
if (err == noErr) {
if ((importLibrary->options & kPEFWeakImportLibMask) == 0) {
err = cfragFragmentUsageErr;
}
}
if (err == noErr) {
err = RunRelocationEngine(&fragToFix, importLibrary, lookup, refCon);
}
if (fragToFix.disposeSectionPointers) {
if (fragToFix.fileRef != 0) {
junk = FSClose(fragToFix.fileRef);
MoreAssertQ(junk == noErr);
}
if (fragToFix.loaderSection != nil) {
DisposePtr( (Ptr) fragToFix.loaderSection);
MoreAssertQ(MemError() == noErr);
}
if (fragToFix.sectionHeaders != nil) {
DisposePtr( (Ptr) fragToFix.sectionHeaders);
MoreAssertQ(MemError() == noErr);
}
}
return err;
}
static pascal OSStatus FragmentLookup(ConstStr255Param symName, CFragSymbolClass symClass,
void **symAddr, void *refCon)
{
OSStatus err;
CFragConnectionID connIDToImport;
CFragSymbolClass foundSymClass;
MoreAssertQ(symName != nil);
MoreAssertQ(symAddr != nil);
MoreAssertQ(refCon != nil);
connIDToImport = (CFragConnectionID) refCon;
err = FindSymbol(connIDToImport, symName, (Ptr *) symAddr, &foundSymClass);
if (err == noErr) {
if (foundSymClass != symClass) {
MoreAssertQ(false);
*symAddr = nil;
err = cfragNoSymbolErr;
}
}
return err;
}
extern pascal OSStatus CFMLateImportLibrary(const CFragSystem7DiskFlatLocator *fragToFixLocator,
CFragConnectionID fragToFixConnID,
CFragInitFunction fragToFixInitRoutine,
ConstStr255Param weakLinkedLibraryName,
CFragConnectionID connIDToImport)
{
MoreAssertQ(connIDToImport != nil);
return CFMLateImportCore(fragToFixLocator, fragToFixConnID, fragToFixInitRoutine,
weakLinkedLibraryName, FragmentLookup, connIDToImport);
}
static pascal OSStatus BundleLookup(ConstStr255Param symName, CFragSymbolClass symClass,
void **symAddr, void *refCon)
{
OSStatus err;
CFBundleRef bundleToImport;
CFStringRef symNameStr;
MoreAssertQ(symName != nil);
MoreAssertQ(symAddr != nil);
MoreAssertQ(refCon != nil);
symNameStr = nil;
bundleToImport = (CFBundleRef) refCon;
err = noErr;
if (symClass != kTVectorCFragSymbol) {
MoreAssertQ(false);
err = cfragNoSymbolErr;
}
if (err == noErr) {
symNameStr = CFStringCreateWithPascalString(kCFAllocatorSystemDefault,
symName, kCFStringEncodingMacRoman);
if (symNameStr == nil) {
err = coreFoundationUnknownErr;
}
}
if (err == noErr) {
*symAddr = CFBundleGetFunctionPointerForName(bundleToImport, symNameStr);
if (*symAddr == nil) {
err = cfragNoSymbolErr;
}
}
if (symNameStr != nil) {
CFRelease(symNameStr);
}
return err;
}
extern pascal OSStatus CFMLateImportBundle(const CFragSystem7DiskFlatLocator *fragToFixLocator,
CFragConnectionID fragToFixConnID,
CFragInitFunction fragToFixInitRoutine,
ConstStr255Param weakLinkedLibraryName,
CFBundleRef bundleToImport)
{
MoreAssertQ(bundleToImport != nil);
return CFMLateImportCore(fragToFixLocator, fragToFixConnID, fragToFixInitRoutine,
weakLinkedLibraryName, BundleLookup, bundleToImport);
}