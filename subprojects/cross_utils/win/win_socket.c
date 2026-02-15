#define CROSS_SOCKET_IMPL
#include "cross_socket.h"
#undef CROSS_SOCKET_IMPL

#include <minwindef.h>
#include <psdk_inc/_socket_types.h>
#include <unistd.h>
#include <winsock.h>
#include <winsock2.h>

#include "my_types.h"

Socket open_socket_tcp()
{
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0)
        return -1;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, "TRUE", sizeof(int)) == -1)
        return -1;
    return sock;
}

int close_socket(Socket socket)
{
    return (closesocket(socket) | WSACleanup()) ? -1 : 0;
}
