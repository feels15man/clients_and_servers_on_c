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

void s_close(int s)
{
	close(s);
}


int recv_ok(int s){
	int r;
	char ok[3] = {0};

	r = recv(s, ok, sizeof(char), 0);
	if (r < 0)
		return sock_err("recv response error");

	r = recv(s, ok + 1, sizeof(char), 0);
	if (r < 0)
		return sock_err("recv response error");
	
	if (strcmp(ok, "ok")){
		printf("Undefined rponse: '%s'\n", ok);
		return -1;
	}
	return 0;
}

int check_n_char_in_range(char *buff, char min, char max, int n){
	for(int i = 0; i < n;i++){
		if (buff[i] > max || buff[i] < min)
			return 0;
	}
	return 1;
}


int send_line(int s, int message_num, char* line, int read){
	int r;
	char *save_ptr;
	char *token;

	// +
	int f = check_n_char_in_range(line, '+', '+', 1);
	if (!f)
		return 2;
	//phone 1
	f = check_n_char_in_range(line + 1, '0', '9', 11);
	if (!f)
		return 2;

	f = check_n_char_in_range(line + 12, ' ', ' ', 1);
	if (!f)
		return 2;

	//phone 2
	f = check_n_char_in_range(line + 13, '+', '+', 1);
	if (!f)
		return 2;

	f = check_n_char_in_range(line + 14, '0', '9', 11);
	if (!f)
		return 2;

	f = check_n_char_in_range(line + 25, ' ', ' ', 1);
	if (!f)
		return 2;
	//hh
	f = check_n_char_in_range(line + 26, '0', '9', 2);
	if (!f)
		return 2;
	f = check_n_char_in_range(line + 28, ':', ':', 1);
	if (!f)
		return 2;

	f = check_n_char_in_range(line + 29, '0', '9', 2);
	if (!f)
		return 2;
	f = check_n_char_in_range(line + 31, ':', ':', 1);
	if (!f)
		return 2;

	f = check_n_char_in_range(line + 32, '0', '9', 2);
	if (!f)
		return 2;
	f = check_n_char_in_range(line + 34, ' ', ' ', 1);
	if (!f)
		return 2;


	uint32_t converted_number = htonl(message_num);
	r = send(s, &converted_number, sizeof (uint32_t), 0);
	// if(r < 0)
	// 	return sock_err("send");

	// r = send(s, line, 12, 0);
	// if (r < 0)
	// 	return sock_err("Send line error");

	// r = send(s, line + 13, 12, 0);
	// if (r < 0)
	// 	return sock_err("Send line error");

	// char tmp;




	// r = send(s, line + 35, len - 35, 0);


	//first phone num
	token = strtok_r(line, " ", &save_ptr);
	r = send(s, token, 12, 0);
	if(r < 0)
		return sock_err("send");

	//second phone num
	token = strtok_r(NULL, " ", &save_ptr);
	r = send(s, token, 12, 0);
	if(r < 0)
		return sock_err("send");

	//convert and send hh:mm:ss
	char tmp;
	//hh
	token = strtok_r(NULL, ":", &save_ptr);
	tmp = (token[0] - '0') * 10 + (token[1] - '0');
	r = send(s, (const char*) &tmp, 1, 0);
	if(r < 0)
		return sock_err("send");

	//mm
	token = strtok_r(NULL, ":", &save_ptr);
	tmp = (token[0] - '0') * 10 + (token[1] - '0');
	r = send(s, (const char*) &tmp, 1, 0);
	if(r < 0)
		return sock_err("send");

	//ss
	token = strtok_r(NULL, " ", &save_ptr);
	tmp = (token[0] - '0') * 10 + (token[1] - '0');
	r = send(s, (const char*) &tmp, 1, 0);
	if(r < 0)
		return sock_err("send");

	// message
	token = strtok_r(NULL, "\n", &save_ptr);

	int len = 0;
	while (token[len++]);
	r = send(s, token, len, 0);
	// printf("l %d : r %d\n", len, read);
	// r = send(s, line + 35, len - 35, 0); // 35 = 12 + 12 + 6 + 2 + 3

	if(r < 0)
		return sock_err("send");
	return 0;

}


int send_info(int s, char *filename)
{
	FILE *f = fopen(filename, "r");
	char * line = NULL;
    size_t len = 0;
    ssize_t read;
	uint32_t message_num = 0;

	if (!f){
		printf("Unknown filename\n");
		exit(EXIT_FAILURE);
	}

	while (!feof(f) && (read = getline(&line, &len, f)) != -1) {
		if(read == 1)
			continue;
		int r = send_line(s, message_num, line, read);
		if (r == 1)
			printf("send line err");
		else if (r == 0)
			printf("Message %d: send %zu bytes\n", message_num, read);
		else if(r == 2)
			printf("Message skipped\n");

		if(!recv_ok(s))
			message_num++;
		else
			return sock_err("ok recv");


    }
	fclose(f);
	return 0;
}


int main(int argc, char *argv[])
{
	if (argc != 3){
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
				s_close(s);
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
	if (send(s, "put", 3, 0) == -1){
		return sock_err("send put");
	}
	// Отправка результата
	send_info(s, argv[2]);
	// Закрытие соединения
	s_close(s);
	return 0;
}