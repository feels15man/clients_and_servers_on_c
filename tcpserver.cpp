#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
// Директива линковщику: использовать библиотеку сокетов
#pragma comment(lib, "ws2_32.lib")

// #include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #define CLEAN_FILE

#define FALSE 0
#define TRUE 1

#define N 50
#define STOP_SIGNAL 3
#define out_file "msg.txt"

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


int sock_err(const char* function, int s)
{
	int err;
	err = WSAGetLastError();
	fprintf(stderr, "%s: socket %d error: %d\n", function, s, err);
	return -1;
}


int set_non_block_mode(int s)
{
#ifdef _WIN32
	unsigned long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
#else
	int fl = fcntl(s, F_GETFL, 0);
	return fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
}


int Recv(int s, char* b, size_t length, int flags){
	int r = recv(s, b, length, flags);
	if (r < 0){
		sock_err("recv fail", s);
		exit(EXIT_FAILURE);
	}
	return r;
}


int send_line(int s, int message_num, char* line){
	int r;
	char *save_ptr;
	char *token;

	unsigned int converted_number = htonl(message_num);
	r = send(s, (const char*)&converted_number, sizeof (unsigned int), 0);

	//first phone num
	token = strtok_s(line, " ", &save_ptr);
	r = send(s, token, 12, 0);
	if(r < 0)
		return sock_err("send", s);

	//second phone num
	token = strtok_s(NULL, " ", &save_ptr);
	r = send(s, token, 12, 0);
	if(r < 0)
		return sock_err("send", s);

	//convert and send hh:mm:ss
	char tmp;
	//hh
	token = strtok_s(NULL, ":", &save_ptr);
	tmp = (token[0] - '0') * 10 + (token[1] - '0');
	r = send(s, (const char*) &tmp, 1, 0);
	if(r < 0)
		return sock_err("send", s);

	//mm
	token = strtok_s(NULL, ":", &save_ptr);
	tmp = (token[0] - '0') * 10 + (token[1] - '0');
	r = send(s, (const char*) &tmp, 1, 0);
	if(r < 0)
		return sock_err("send", s);

	//ss
	token = strtok_s(NULL, " ", &save_ptr);
	tmp = (token[0] - '0') * 10 + (token[1] - '0');
	r = send(s, (const char*) &tmp, 1, 0);
	if(r < 0)
		return sock_err("send", s);

	// message
	token = strtok_s(NULL, "\n", &save_ptr);

	int len = 0;
	while (token[len++]);
	r = send(s, token, len, 0);
	// printf("l %d : r %d\n", len, read);
	// r = send(s, line + 35, len - 35, 0); // 35 = 12 + 12 + 6 + 2 + 3

	if(r < 0)
		return sock_err("send", s);
	return 0;

}


int main(int argc, char* argv[])
{
	if (argc != 2) {
		printf("Wrong arguments count\n");
		return -1;
	}

#ifdef CLEAN_FILE 
	//clear file
	FILE* f_clean = fopen(out_file, "w");
	fclose(f_clean);
#endif

	FILE* f = fopen(out_file, "a");

	init();

	int ls; // Прослушивающий сокет		
	struct sockaddr_in addr;

	// Создание TCP-сокета
	ls = socket(AF_INET, SOCK_STREAM, 0);
	if (ls < 0)
		return sock_err("socket", ls);
	if (set_non_block_mode(ls) != 0)
		return sock_err("non block mode", ls);

	// Заполнение адреса прослушивания
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1])); // Сервер прослушивает порт argv[1]
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // Все адреса

	// Связывание сокета и адреса прослушивания
	if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		return sock_err("bind", ls);
	// Начало прослушивания
	if (listen(ls, N) < 0)
		return sock_err("listen", ls);


	int cs[N]; // Клиентские соединения
	int cs_cnt = 0; // Количество текущих открытых соединений
	bool got_put_flag[N] = { 0 };
	WSAEVENT events[2]; // Первое событие - прослушивающего сокета, второе - клиентских соединений
	int i;
	events[0] = WSACreateEvent();
	events[1] = WSACreateEvent();
	WSAEventSelect(ls, events[0], FD_ACCEPT);
	for (i = 0; i < cs_cnt; i++)
		WSAEventSelect(cs[i], events[1], FD_READ | FD_WRITE | FD_CLOSE);

	printf("Closely listening: %s\n", argv[1]);

	while (1)
	{
		WSANETWORKEVENTS ne;
		// Ожидание событий в течение секунды
		DWORD dw = WSAWaitForMultipleEvents(2, events, FALSE, 1000, FALSE);
		WSAResetEvent(events[0]);
		WSAResetEvent(events[1]);
		if (0 == WSAEnumNetworkEvents(ls, events[0], &ne) &&
			(ne.lNetworkEvents & FD_ACCEPT))
		{
			// Поступило событие на прослушивающий сокет, можно принимать подключение функцией accept
			// Принятый сокет следует добавить в массив cs и подключить его к событию events[1]

			int addrlen = sizeof(addr);
			cs[cs_cnt] = accept(ls, (struct sockaddr*)&addr, &addrlen);
			if (cs[cs_cnt] == -1)
				continue;
			WSAEventSelect(cs[cs_cnt], events[1], FD_READ | FD_WRITE | FD_CLOSE);

			cs_cnt++;

			unsigned int ip = ntohl(addr.sin_addr.s_addr);
			unsigned short port = ntohs(addr.sin_port);
			printf(" Client connected: %u.%u.%u.%u %u\n",
				(ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF, port);
		}
		for (i = 0; i < cs_cnt; i++)
		{
			if (0 == WSAEnumNetworkEvents(cs[i], events[1], &ne))
			{
				// По сокету cs[i] есть события

				if (ne.lNetworkEvents & FD_READ)
				{
					// Есть данные для чтения, можно вызывать recv/recvfrom на cs[i]
					// printf("Event read\n");
					int rcv;
					char buffer[512] = {0};

					// check put
					if(!got_put_flag[i])
					{	
						char put_str[4] = { 0 };
						// int index = 0;
						while (Recv(cs[i], put_str, 3, MSG_PEEK) != 3);
						rcv = Recv(cs[i], put_str, 3, 0);
						if (!strcmp(put_str, "put"))
							got_put_flag[i] = TRUE;
						else if(!strcmp(put_str, "get")){
							FILE* fs = fopen(out_file, "r");
							int i = 0;
							while(!feof(fs)){
								char *str;
								fgets(str, 512, fs);
								send_line(cs[i], i++, str);
							}
							closesocket(cs[i]);
						}
						else
							printf(" put != %s\n", put_str);

						// rcv = Recv(cs[i], put_str, 3, 0);

						// if (!strcmp(put_str, "put"))
						// 	got_put_flag[i] = TRUE;
						// else if(!strcmp(put_str, "get")){
						// 	FILE* fs = fopen(out_file, "r");
						// 	int i = 0;
						// 	while(!feof(fs)){
						// 		char *str;
						// 		fgets(str, 512, fs);
						// 		send_line(cs[i], i++, str);
						// 	}
						// 	closesocket(cs[i]);
						// }
						// else
						// 	printf(" put != %s\n", put_str);
						continue;
					} 

					unsigned int ip = ntohl(addr.sin_addr.s_addr);
					unsigned short port = ntohs(addr.sin_port);
					fprintf(f, "%u.%u.%u.%u:%u ",
						(ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF, port);

					rcv = Recv(cs[i], buffer, 4, 0);

					//phones
					for (int j = 0; j < 2; j ++){
						rcv = Recv(cs[i], buffer, 12, 0);
						fputs(buffer, f);
						fputc(' ', f);
					}


					//time
					rcv = Recv(cs[i], buffer, 1, 0);
					fputc(buffer[0] / 10 + '0', f);
					fputc(buffer[0] % 10 + '0', f);

					for (int j = 0; j < 2; j++){
						rcv = Recv(cs[i], buffer, 1, 0);
						fputc(':', f);
						fputc(buffer[0] / 10 + '0', f);
						fputc(buffer[0] % 10 + '0', f);
					}

					fputc(' ', f);

					rcv = Recv(cs[i], buffer, sizeof(buffer), 0);
					fputs(buffer, f);
					fputc('\n', f);
					send(cs[i], "ok", 2, 0);
					if(!strcmp(buffer, "stop"))
					{
						printf("Stop message recieved, terminating...\n");
						for (i = 0; i < cs_cnt; i++)
							closesocket(cs[i]);
						deinit();
						return 0;
					}
				}
				if (ne.lNetworkEvents & FD_CLOSE)
				{
					// Удаленная сторона закрыла соединение, можно закрыть сокет и удалить его из cs
					// printf("Event close\n");
					closesocket(cs[i]);
					cs[i] = cs[cs_cnt - 1];
					cs[cs_cnt - 1] = 0;
					cs_cnt--;
					unsigned int ip = ntohl(addr.sin_addr.s_addr);
					unsigned short port = ntohs(addr.sin_port);
					printf(" Client disconnected: %u.%u.%u.%u %u\n",
						(ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF, port);
				}
			}
		}
	}
	return 0;
}