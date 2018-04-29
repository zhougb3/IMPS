#include "threadpool.h"

#define BUF_SIZE 1024
#define EPOLL_SIZE 50
#define CLIENT_SIZE 100


int clnt_socks[CLIENT_SIZE];
char clnt_names[CLIENT_SIZE][100];
int clnt_destination[CLIENT_SIZE];
pthread_mutex_t mutx;
int clnt_size = 0;
struct epoll_event event;
int epfd;
struct threadpool *pool;
void * work(void * arg);
void * newClient(void * arg);
void * closeClient(void * arg);
void error_handing(char * buf) {
	printf("%s\n", buf);
	exit(1);
}

int main(int argc, char * argv[]) {
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_addr, clnt_addr;
	socklen_t adr_sz;
	uint32_t str_len,message_length;
	int i = 0, j =  0;
	struct epoll_event * ep_events;
	int event_cnt;
	
	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		exit(1);
	}

	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1) {
		error_handing("socket() error !");
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if (bind(serv_sock, (struct sockaddr * ) &serv_addr, sizeof(serv_addr)) == -1) {
		error_handing("bind() error !");
	}
	if (listen(serv_sock, 5) == -1) {
		error_handing("listen() error !");
	}

	epfd = epoll_create(EPOLL_SIZE);
	ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

	event.events = EPOLLIN;
	event.data.fd = serv_sock;
	epoll_ctl(epfd, EPOLL_CTL_ADD,serv_sock, &event);

    pool = threadpool_init(10, 20);
    pthread_mutex_init(&mutx, NULL);

	while(1) {
		event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
		printf("A total of %d monitoring events occurred\n",event_cnt);
		if (event_cnt == -1) {
			printf("epoll_wait() error\n");
			break;
		}
		for (i = 0; i < event_cnt; i++) {
			if (ep_events[i]. data.fd == serv_sock) {
				adr_sz = sizeof(clnt_addr);
				clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &adr_sz);
				if (clnt_sock == -1) {
					printf("accept() error !\n");
					break;
				}
				event.events = EPOLLIN;
				event.data.fd = clnt_sock;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
				pthread_mutex_lock(&mutx);
				clnt_socks[clnt_size++] = clnt_sock;
				pthread_mutex_unlock(&mutx);
				printf("connect client : %d\n", clnt_sock);
			} else {
					str_len = recv(ep_events[i].data.fd, &message_length,sizeof(message_length),MSG_PEEK | MSG_DONTWAIT);
					if (str_len == 0) {
						pthread_mutex_lock(&mutx);
						for(j = 0; j < clnt_size; ++j) {
							if (clnt_socks[j] == ep_events[i].data.fd) {
								while(j++ < clnt_size - 1)
									clnt_socks[j] = clnt_socks[j+1];
								break;
							}
						}
						clnt_size--;
						clnt_names[ep_events[i].data.fd][0] = '\0';
						pthread_mutex_unlock(&mutx);
						epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
						close(ep_events[i].data.fd);
						printf("closed client %d\n", ep_events[i].data.fd);
						threadpool_add_job(pool, closeClient, (void *)&ep_events[i].data.fd);												
					} else {
						if (str_len != sizeof(message_length)) {
							printf("Message head failed to read all, temporarily not to handle, we can read is %d\n",str_len);
							continue;
						}
						epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);						
						threadpool_add_job(pool, work, (void *)&ep_events[i].data.fd);						
					}
			}
		}
	}
	close (serv_sock);
	close(epfd);
    threadpool_destroy(pool);	
	return 0;
}

void * work(void * arg)
{
    int fd = *(int *)arg, j = 0;
    uint32_t str_len,message_length;
	char buf[BUF_SIZE];
	recv(fd, &message_length,sizeof(message_length),MSG_PEEK | MSG_DONTWAIT);
	do {
		str_len = recv(fd, buf,message_length + sizeof(message_length),MSG_PEEK | MSG_DONTWAIT);		
	} while(str_len != message_length + sizeof(message_length));	
	recv(fd, &message_length,sizeof(message_length),0);
	str_len = recv(fd, buf,message_length,0);
	buf[str_len] = '\0';						
	printf("the message_length is %d(really %d), the message is %s: \n",message_length, str_len, buf);
	if (buf[strlen(buf) - 2] == '%' && buf[strlen(buf) - 1] == '&') {
		buf[strlen(buf) - 2] = '\0';
		strcpy(clnt_names[fd], buf);
		strcpy(buf, "!!");
		pthread_mutex_lock(&mutx);
		if (clnt_size == 1) {
			strcpy(buf, "@@");
			send(fd, buf, strlen(buf), 0);			
		}
		for (j = 0; j < clnt_size;++j) {
			if (clnt_socks[j] == fd)
				continue;
			send(fd, buf, strlen(buf), 0);
			message_length = strlen(clnt_names[clnt_socks[j]]);
			send(fd, &clnt_socks[j],sizeof(clnt_socks[j]),0);
			send(fd, &message_length, sizeof(message_length),0);
			send(fd, clnt_names[clnt_socks[j]],message_length,0);
		}
		pthread_mutex_unlock(&mutx);
		strcpy(buf,"$$");
		send(fd, buf, strlen(buf), 0);		
		threadpool_add_job(pool, newClient, arg);
		printf("we have get the username and handle \n");												
	} else if(str_len == sizeof(clnt_socks[0]) + 2 && buf[str_len - 2] == '#' && buf[str_len - 1] == '#'){
		int num = *((int *)buf);
		clnt_destination[fd] = num;
		printf("we have remember your destination socket\n");
	} else {
		if (clnt_destination[fd]) {
			char temp[BUF_SIZE];
			temp[0] = '[';
			sprintf(&temp[1], "%d", fd);
			strcpy(&temp[strlen(temp)],"(");
			strcpy(&temp[strlen(temp)],clnt_names[fd]);
			strcpy(&temp[strlen(temp)],") send to you:");
			strcpy(&temp[strlen(temp)],buf);
			strcpy(&temp[strlen(temp)],"]");
			message_length = strlen(temp);													
			send(clnt_destination[fd], &message_length, sizeof(message_length), 0);		
			send(clnt_destination[fd], temp, strlen(temp), 0);
			printf("we have transform your message to the destination socket\n");			
		}else
			printf("transform fail!\n");	
	}
	event.events = EPOLLIN;
	event.data.fd = fd;	
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
}

void * newClient(void * arg) {
	int i = 0, fd = *((int *)arg);
	//printf("%d\n", fd);
	uint32_t str_len;
	char message[100];
	message[0] = '[';	
	pthread_mutex_lock(&mutx);	
	for (;i < clnt_size; ++i) {
		if (fd == clnt_socks[i])
			continue;
		sprintf(&message[1], "%d", fd);			
		strcpy(&message[strlen(message)],":");
		strcpy(&message[strlen(message)],clnt_names[fd]);
		strcpy(&message[strlen(message)]," is comming !]");
		str_len = strlen(message);				
		send(clnt_socks[i], &str_len, sizeof(str_len),0);
		send(clnt_socks[i], &message, str_len, 0);
		printf("%s send to %d of length %d \n", message, clnt_socks[i], str_len);
	}
	pthread_mutex_unlock(&mutx);
}

void * closeClient(void * arg) {
	int i = 0, fd = *((int *)arg);
	uint32_t str_len;
	char message[100];
	message[0] = '[';	
	pthread_mutex_lock(&mutx);	
	for (;i < clnt_size; ++i) {
		sprintf(&message[1], "%d", fd);			
		strcpy(&message[strlen(message)],":");
		strcpy(&message[strlen(message)],clnt_names[fd]);
		strcpy(&message[strlen(message)]," is leaving !]");
		str_len = strlen(message);				
		send(clnt_socks[i], &strlen, sizeof(str_len),0);
		send(clnt_socks[i], &message, str_len, 0);
		printf("%s\n send to %d of length %d \n", message, clnt_socks[i], str_len);		
	}
	pthread_mutex_unlock(&mutx);
}
