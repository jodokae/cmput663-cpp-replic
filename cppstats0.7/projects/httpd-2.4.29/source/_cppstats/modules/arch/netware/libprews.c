#include <netware.h>
#if defined(USE_WINSOCK)
#include <novsock2.h>
#endif
int _NonAppStart
(
void *NLMHandle,
void *errorScreen,
const char *cmdLine,
const char *loadDirPath,
size_t uninitializedDataLength,
void *NLMFileHandle,
int (*readRoutineP)( int conn, void *fileHandle, size_t offset,
size_t nbytes, size_t *bytesRead, void *buffer ),
size_t customDataOffset,
size_t customDataSize,
int messageCount,
const char **messages
) {
#pragma unused(cmdLine)
#pragma unused(loadDirPath)
#pragma unused(uninitializedDataLength)
#pragma unused(NLMFileHandle)
#pragma unused(readRoutineP)
#pragma unused(customDataOffset)
#pragma unused(customDataSize)
#pragma unused(messageCount)
#pragma unused(messages)
#if defined(USE_WINSOCK)
WSADATA wsaData;
return WSAStartup((WORD) MAKEWORD(2, 0), &wsaData);
#else
return 0;
#endif
}
void _NonAppStop( void ) {
#if defined(USE_WINSOCK)
WSACleanup();
#else
return;
#endif
}
int _NonAppCheckUnload( void ) {
return 0;
}
