#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024

void * readMessage(void * arg);
void error_handing(char * buf) {
	printf("%s\n", buf);
	exit(1);
}

int main(int argc, char * argv[]) {
	int sock;
	uint32_t str_len, username_length;
	char message[BUF_SIZE];
	struct sockaddr_in serv_adr;
	pthread_t t_id;
	if (argc != 3) {
		printf("Usage : %s <IP> <port> \n", argv[0]);
		exit(1);
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		error_handing("socket() error");
	}

	memset(&serv_adr,0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));

	if (connect(sock, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) == -1)
		error_handing("connect() error!");
	else
		printf("connect OK!\n");

	printf("[please tell your name to the service:]");
	gets(message);
	username_length = strlen(message) + 2;
	send(sock,&username_length, sizeof(username_length),0);
	str_len = strlen(message);
	message[str_len++] = '%';
	message[str_len++] = '&';
	message[str_len] = '\0';
	send(sock, message,strlen(message),0);
	printf("[At present, there are the following users online except you. Please send user numbers to initiate chat:]\n");
	int user_number = -1;
	char user_name[100];
	while(1) {
		do {
			str_len = recv(sock, message, 2,MSG_PEEK);			
		} while(str_len != 2);
		recv(sock, message, 2,0);
		if (str_len == 2 && (message[0] == '@' && message[1] == '@')) {
			printf("[No users are online at the moment. Please wait for the user to go online......]\n");
			continue;		
		}
		if (str_len == 2 && message[0] == '$' && message[1] == '$') 
			break;
		do {
			str_len = recv(sock, message, sizeof(user_number)+sizeof(username_length),MSG_PEEK);			
		} while(str_len != sizeof(user_number) + sizeof(username_length));		
		recv(sock, &user_number, sizeof(user_number), 0);
		recv(sock, &username_length, sizeof(username_length), 0);
		str_len = 0;
		int recv_len = 0;
		while(recv_len < username_length) {
			str_len = recv(sock, &user_name[recv_len], username_length - recv_len,0);
			recv_len += str_len;			
		}	
		printf("[%d]: %s\n", user_number, user_name);
	}

	if (pthread_create(&t_id,NULL,readMessage,(void *)&sock) != 0) {
		printf("create pthread error\n");
		return -1;
	}
		
	printf("[Please enter the user number that you want to send:]");
	scanf("%d", &user_number);
	gets(message);
	uint32_t length = sizeof(user_number) + 2;
	send(sock, &length, sizeof(length),0);
	send(sock,&user_number, sizeof(user_number),0);
	strcpy(message, "##");
	send(sock, message, strlen(message),0);

	while(1) {
		printf("[Please enter the information you want to send to number %d :]", user_number);		
		gets(message);
		str_len = strlen(message);
		send(sock, &str_len, sizeof(str_len),0);
		send(sock, message,strlen(message),0);				
	}
	close(sock);

	pthread_detach(t_id);
	return 0;
}

void * readMessage(void * arg) {
	while(1) {
		int fd  = *((int *)arg);
		uint32_t length = 0;
		char  message[BUF_SIZE];
		do {
			length = recv(fd, message, sizeof(length),MSG_PEEK);			
		} while(length != sizeof(length));	
		int recv_len = 0, str_len = 0;
		recv(fd, &length,sizeof(length),0);
		//printf("have read the head of length : %d!\n", length);
		while(recv_len < length) {
			str_len = recv(fd, &message[recv_len], length - recv_len,0);
			recv_len += str_len;			
		}
		message[recv_len] = '\0';
		printf("\n%s", message);
		printf("\n[If you have specified the user you sent, please input the information, otherwise enter the user number:]\n");			
	}	
}