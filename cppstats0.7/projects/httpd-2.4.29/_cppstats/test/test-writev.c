#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <errno.h>
#if !defined(INADDR_NONE)
#define INADDR_NONE (-1ul)
#endif
void main( int argc, char **argv ) {
struct sockaddr_in server_addr;
int s;
struct iovec vector[3];
char buf[100];
int i;
const int just_say_no = 1;
if( argc != 3 ) {
usage:
fprintf( stderr, "usage: test-writev a.b.c.d port#\n" );
exit( 1 );
}
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = inet_addr( argv[1] );
if( server_addr.sin_addr.s_addr == INADDR_NONE ) {
fprintf( stderr, "bogus address\n" );
goto usage;
}
server_addr.sin_port = htons( atoi( argv[2] ) );
s = socket( AF_INET, SOCK_STREAM, 0 );
if( s < 0 ) {
perror("socket");
exit(1);
}
if( connect( s, (struct sockaddr *)&server_addr, sizeof( server_addr ) )
!= 0 ) {
perror("connect");
exit(1);
}
if( setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&just_say_no,
sizeof(just_say_no)) != 0 ) {
perror( "TCP_NODELAY" );
exit(1);
}
for( i = 0; i < sizeof( buf ); ++i ) {
buf[i] = 'x';
}
vector[0].iov_base = buf;
vector[0].iov_len = sizeof(buf);
vector[1].iov_base = buf;
vector[1].iov_len = sizeof(buf);
vector[2].iov_base = buf;
vector[2].iov_len = sizeof(buf);
i = writev( s, &vector[0], 3 );
fprintf( stdout, "i=%d, errno=%d\n", i, errno );
exit(0);
}