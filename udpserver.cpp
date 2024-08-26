#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>  // for inet_addr()
#include <stdlib.h> 	// for atoi()
#include <unistd.h> 	//for close()

#define out_file "msg.txt"
#define N 100


void s_close(int s)
{
	close(s);
}


int sock_err(const char* function)
{
	int err;
	err = errno;
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}


int set_non_block_mode(int s)
{
	int fl = fcntl(s, F_GETFL, 0);
	return fcntl(s, F_SETFL, fl | O_NONBLOCK);
}


int write_message_to_file(char* buffer, int ip, int port, FILE *f);
int check_client(struct client_info *clients_array, int ip, int port, int n);

struct client_info{
	int ip;
	int port;
	int max_msg;
	int recv_msg;
	int* recv_msg_arr;
};


int main(int argc, char* argv[])
{
	if (argc != 3){
		printf("I dont know how to work with such args :_(\n");
		return -1;
	}

	int start_port = atoi(argv[1]), end_port = atoi(argv[2]);
	int port_count = end_port - start_port + 1;

	int cs[N];
	struct sockaddr_in addr[N];


	printf("Listen %d ports: ", port_count);
	for(int i = 0; i < port_count; i++)
    {
        // Создание UDP-сокета
        cs[i] = socket(AF_INET, SOCK_DGRAM, 0);
        if (cs[i] < 0)
            return sock_err("socket");

        set_non_block_mode(cs[i]);

        // Заполнение структуры с адресом прослушивания узла
        memset(&addr[i], 0, sizeof(addr[i]));
        addr[i].sin_family = AF_INET;
        addr[i].sin_port = htons(i + start_port); 
        addr[i].sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(cs[i], (struct sockaddr*) &addr[i], sizeof(addr[i])) < 0)
            return sock_err("bind");

		printf("%d ", start_port + i);
    }
	printf("\n");

	fd_set rfd;
	fd_set wfd;
	int nfds = cs[0];
	struct timeval tv = {30, 0};

	int clients_max = 50;
	int clients_exists = 0;

	struct client_info *clients_array = (struct client_info*) malloc(clients_max * sizeof(struct client_info));

	FILE* f = fopen(out_file, "a");
    if(f == NULL)
        sock_err("file open");

	// for (int i = 0; i < port_count; i++)
	// 	printf("%d ", cs[i]);
	// printf("\n");

	int Run_condition = 1;

	while (Run_condition)
	{
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);

		for (int i = 0; i < port_count; i++)
		{
			FD_SET(cs[i], &rfd);
			FD_SET(cs[i], &wfd);
			if (nfds < cs[i])
				nfds = cs[i];
		}

		if (select(nfds + 1, &rfd, &wfd, 0, &tv) > 0)
		{
			// Есть события
			for (int i = 0; i < port_count; i++)
			{
				if (FD_ISSET(cs[i], &rfd))
				{
					// Сокет cs[i] доступен для чтения. Функция recv вернет данные, recvfrom - дейтаграмму

					// printf(" recved from Client: %u.%u.%u.%u:%u \n",
					// (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF, port);

					char buffer[4096] = { 0 };
					int len = 0;
					unsigned int addrlen = sizeof(sockaddr_in);
					
					int rcv = recvfrom(cs[i], buffer, sizeof(buffer), 0, (struct sockaddr*) &addr[i], &addrlen);
					if (rcv < 0)
						return sock_err("recvfrom");

					unsigned int ip = ntohl(addr[i].sin_addr.s_addr);
					unsigned int port = ntohs(addr[i].sin_port);
					bool new_connection = 1;
					bool new_message = 1;

					int message_num;
					memcpy(&message_num, buffer, 4);
					
					int j = check_client(clients_array, ip, port, clients_exists);

					if(j){
						new_connection = 0;

						//check messages
						for(int k = 0; k < clients_array[j].recv_msg; k++){
							if(message_num == clients_array[j].recv_msg_arr[k])
								new_message = 0;
						}

						// alloc new memory if needed
						if(new_message){ 
							if(clients_array[j].recv_msg == clients_array[j].max_msg){
								clients_array[j].max_msg += 5;
								clients_array[j].recv_msg_arr = (int *) realloc(clients_array[j].recv_msg_arr, clients_array[j].max_msg * sizeof(int));
							}
							clients_array[j].recv_msg_arr[clients_array[j].recv_msg] = message_num;
							clients_array[j].recv_msg++;
						}
					}

					//inintialize new client if new
					if(new_connection){
						if(clients_exists == clients_max){
							clients_max += 20;
							clients_array = (struct client_info*) realloc(clients_array, clients_max * sizeof(struct client_info));
						}
						clients_array[clients_exists].ip = ip;
						clients_array[clients_exists].port = port;

						clients_array[clients_exists].recv_msg_arr = (int*) malloc(clients_array[clients_exists].max_msg * sizeof(int));

						clients_array[clients_exists].max_msg = 20;
						clients_array[clients_exists].recv_msg = 0;
						clients_array[clients_exists].recv_msg_arr[clients_array[clients_exists].recv_msg] = message_num;
						clients_array[clients_exists].recv_msg++;

						clients_exists++;

					} 

					if(new_message){ 
						printf("New message recieved\n");
						if(write_message_to_file(buffer, ip, port, f)){
								Run_condition = 0;
								printf("Stop-message received.\n");
							}
						sendto(cs[i], (char *) &message_num, 4, 0, (struct sockaddr*) &addr[i], addrlen);
					}	
				}

				// if (FD_ISSET(cs[i], &wfd))
				// {
				// 	// Сокет cs[i] доступен для записи. Функция send и sendto будет успешно завершена
				// 	// printf("EVENT w %d\n", i);
				// }
			}
		}

		else
		{
			// Произошел таймаут или ошибка
			for(int i = 0; i < clients_max; i++){
				clients_array[i].ip =0;
				clients_array[i].port = 0;
			}
			clients_exists = 0;
			printf("Clients deleted\n");
		}
	}

	for(int i = 0; i < N; i++)
		close(cs[i]);
	fclose(f);

	return 0;
}

int write_message_to_file(char* buffer, int ip, int port, FILE *f){

	fprintf(f, "%u.%u.%u.%u:%u ", 
		(ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip >> 0) & 0xFF, port);

	char tmp[13] = { 0 };

	//phone nums
	for(int i = 0; i < 2; i++){
		strncpy(tmp, buffer + 4 + i * 12, 12);
		fprintf(f, "%s ", tmp);
	}

	//hh:mm:ss
	for(int i = 0; i < 3; i++){
		tmp[0] = buffer[28 + i];
		fprintf(f, i != 2 ? "%d%d:": "%d%d ", tmp[0] / 10, tmp[0] % 10);
	}

	//message
	fprintf(f, "%s\n", buffer + 31);

	if (!strcmp(buffer + 31, "stop"))
		return 1;

	return 0;
}


int check_client(struct client_info *clients_array, int ip, int port, int n){
	for (int j = 0; j < n; j++)
		if (ip == clients_array[j].ip && port == clients_array[j].port)
			return j;
	return 0;
}
