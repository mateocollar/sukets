#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <winsock2.h>
#include        <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

int
sukets_connect(const char *ip, int port)
{
        WSADATA wsaData;
        SOCKET sock;
        struct sockaddr_in server_addr;

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                return -1;
        }

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
                WSACleanup();
                return -2;
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = inet_addr(ip);

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
                closesocket(sock);
                WSACleanup();
                return -3;
        }

        return (int)sock;
}

int
sukets_send(int sock, const char *data, int len)
{
        return send((SOCKET)sock, data, len, 0);
}

int
sukets_recv(int sock, char *buffer, int max_len)
{
        return recv((SOCKET)sock, buffer, max_len - 1, 0);
}

int
sukets_close(int sock)
{
        int res = closesocket((SOCKET)sock);
        WSACleanup();
        return res;
}