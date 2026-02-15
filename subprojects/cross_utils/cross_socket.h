#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef SOCKET Socket;
typedef struct sockaddr_in SocketAddress;
#else
#include <sys/socket.h>
#include <netinet/in.h>
typedef int Socket;
typedef struct sockaddr_in SocketAddress;
#endif

#include "my_types.h"

Socket open_socket_tcp();

int close_socket(Socket socket);

// int bind_socket(Socket socket, SocketAddress *address);
//
// int listen_socket(Socket socket, int max_pending);
//
// Socket accept_socket(Socket socket);

SocketAddress init_ipv4_addr(u16 port);

#ifdef CROSS_SOCKET_IMPL
SocketAddress init_ipv4_addr(u16 port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    return addr;
}
#endif
