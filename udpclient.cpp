#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
// Директива линковщику: использовать библиотеку сокетов 
#pragma comment(lib, "ws2_32.lib") 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>


int sock_err(const char* function, int s)
{
  int err;
  err = WSAGetLastError();
  fprintf(stderr, "%s: socket error: %d\n", function, err);
  return -1;
}


void s_close(int s)
{
  closesocket(s);
}


int init()
{
    // Для Windows следует вызвать WSAStartup перед началом использования сокетов
    WSADATA wsa_data;
    return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
}


void deinit()
{
    WSACleanup();
}


int read_messages(char *filename, char** arr, int *len);
int send_line(int s, int message_num, char* line, struct sockaddr_in addr_p);
unsigned int recv_response(int s, char msg_status[20]);


int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Wrong args count\n");
        return -1;
    }

    char* save_ptr;
    char *ip = strtok_s(argv[1], ":", &save_ptr);
    char *port = strtok_s(NULL, "", &save_ptr);
    printf("Serv addr: %s:%s\n", ip, port);

    init();

    sockaddr_in addr;

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(atoi(port));
    if (s < 0)
        return sock_err("socket", s);


    char **messages = (char **) calloc(20, sizeof(char *));
    char messages_status[20] = {0};
    int messages_sent = 0;
    int messages_count = 0;

    read_messages(argv[2], messages, &messages_count);

    printf("messages_count: %d\n", messages_count);

    while(messages_sent < 20 && messages_sent != messages_count){
        for(int i = 0; i < messages_count; i++){
            if(!messages_status[i])
                send_line(s, i, messages[i], addr);
        }
        messages_sent += recv_response(s, messages_status);
    }


    s_close(s);
    deinit();
    return 0;
}



int read_messages(char *filename, char** arr, int* count){
    FILE *f = fopen(filename, "r");
    char line[1024];
    *count = 0;

    while (!feof(f)){
        fgets(line, 1024, f);
        
        if(strcmp(line, "\n") == 0)
            continue;

        arr[*count] = (char *) calloc(1024, sizeof (char));
        strcpy(arr[*count], line);
        *count = *count + 1;
    }

    fclose(f);
    return *count;
}


int send_line(int s, int message_num, char* line, struct sockaddr_in addr){
    int len = 0;
    while(line[len++]);


    // len -= 3;
    char* message = (char*) calloc(len, sizeof(char));

    //format message
    int h_msg_num = htonl(message_num);
    memcpy(message, &h_msg_num, 4);

    _snprintf(message + 4, 13, "%s", line);
    _snprintf(message + 16, 13, "%s", line + 13);

    // printf("\n------------\n");

    for (int i = 26, a = 0; i < 26 + 3 * 2 + 2; i += 3, a++) {
        char tmp = 0;
        tmp = (line[i] - '0') * 10 + (line[i + 1] - '0');
        _snprintf(message + 28 + a, 2, "%c", tmp);
        
        // printf("\ntmp = %d ",  tmp); 
    }

    _snprintf(message + 31, len - (strchr(line + 35, '\n') ? 36 : 35), "%s", line + 35); 

    // printf("len: %d", len - 36);
    // printf("line+: %s", line + 35);
    // printf(" 'msg + 24' %s\n", message + 24);
    // printf(" 'msg + 28' %s\n", message + 28);
    // printf(" 'msg + 31' %s\n", message + 31);
    // for(int i = 3; i < 50; i++)
    //     // if ((i > 3 && i < 28) || i > 31)
    //     printf("%c", message[i]);

    int r = sendto(s, message, len, 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in));

    free(message);

    if (r < 0)
        return sock_err("sendto", s); 
    return 0;
}



unsigned int recv_response(int s, char messages_status[20])
{
    struct timeval tv = {0, 100 * 1000}; // 100 msec
    int res;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);

    res = select(s + 1, &fds, 0, 0, &tv);

    if (res > 0)
    {
        struct sockaddr_in addr;
        int addrlen = sizeof(addr);
        char datagram[20];

        int messages_sent = 0;

        int received = recvfrom(s, datagram, sizeof(datagram), 0, (struct sockaddr*) &addr, &addrlen);
        if (received <= 0)
        {
            sock_err("recvfrom", s);
            return 0;
        }

        for(int j = 0; j < received / 4 && j < 20; j++){
            unsigned int num = ntohl(datagram[j]);

            if(!messages_status[num])
                messages_sent++;

            messages_status[num] = 1;

        }
        return messages_sent;
    }
    else if (res == 0)
        return 0;
    else
    {
        sock_err("select", s);
        return 0;
    }
}
