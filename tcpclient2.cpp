#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>  // for inet_addr()
#include <stdlib.h> 	// for atoi()
#include <unistd.h> 	//for close()


int sock_err(const char* function)
{
	int err;
	err = errno;
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}


int Recv(int s, char* b, size_t length, int flags){
	int r = recv(s, b, length, flags);
	if (r < 0){
		sock_err("recv fail");
		exit(EXIT_FAILURE);
	}
	return r;
}


void get_info(int s, char *filename){
	FILE *f = fopen(filename, "a");
	if (f == NULL)
		return;

	char tmp[4] = {0};
	int rcv = Recv(s, tmp, 4, 0);

	while(rcv > 0){
		char buffer[500] = {0};
		//phones
		for (int j = 0; j < 2; j ++){
			rcv = Recv(s, buffer, 12, 0);
			fputs(buffer, f);
			fputc(' ', f);
		}


		//time
		rcv = Recv(s, buffer, 1, 0);
		fputc(buffer[0] / 10 + '0', f);
		fputc(buffer[0] % 10 + '0', f);

		for (int j = 0; j < 2; j++){
			rcv = Recv(s, buffer, 1, 0);
			fputc(':', f);
			fputc(buffer[0] / 10 + '0', f);
			fputc(buffer[0] % 10 + '0', f);
		}

		fputc(' ', f);

		rcv = Recv(s, buffer, 500, 0);
		fputs(buffer, f);
		fputc('\n', f);
		rcv = Recv(s, tmp, 4, 0);
	}
	fclose(f);
}


int main(int argc, char *argv[])
{
	if (argc != 4){
		printf("I dont know how to work with it :_(\n");
		return -1;
	}
	int s;
	struct sockaddr_in addr;
	FILE* f;

	// Создание TCP-сокета
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return sock_err("socket");

	// Заполнение структуры с адресом удаленного узла
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	char *save_ptr;
	char *ipaddr = strtok_r(argv[1], ":", &save_ptr);
	char *port = strtok_r(NULL, "", &save_ptr);
	// printf("addr:%s\tport:%d\n", ipaddr, atoi(port));
	addr.sin_addr.s_addr = inet_addr(ipaddr);
	addr.sin_port = htons(atoi(port));

	// Установка соединения с удаленным хостом
	for (int i = 0; i < 10; i++){
		if (connect(s, (struct sockaddr*) &addr, sizeof(addr))){
			if (i == 9){
				close(s);
				printf("Connection error\n");
				return sock_err("connect");
			}
			usleep(100 * 1000); // 100 ms to microseconds 
		}
		else
			break;
	}
	printf("Connection established\n");
	// Отправка put на удаленный сервер
	if (send(s, "get", 3, 0) == -1){
		return sock_err("send put");
	}
	// Отправка результата
	get_info(s, argv[3]);
	// Закрытие соединения
	close(s);
	return 0;
}