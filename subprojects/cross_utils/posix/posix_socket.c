#define CROSS_SOCKET_IMPL
#include "cross_socket.h"
#undef CROSS_SOCKET_IMPL

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "my_types.h"

Socket open_socket_tcp()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        return -1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1)
        return -1;
    return sock;
}

int close_socket(Socket socket) 
{
    return close(socket);
}

// int bind_socket(Socket socket, SocketAddress *address)
// {
//     return bind(socket, (struct sockaddr *)address, sizeof(*address));
// }
//
// int listen_socket(Socket socket, int max_pending)
// {
//     return listen(socket, max_pending);
// }
//
// Socket accept_socket(Socket socket)
// {
//     return accept(socket, NULL, NULL);
// }
    
