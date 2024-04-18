#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h> 
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/time.h>


#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



#define ENABLE_HTTP_RESPONSE 1




#define BUFFER_LENGTH 512
typedef int (*RCALLBACK)(int fd);

//listenfd
//EPOLLIN-->
int accept_cb(int fd);
//clientfd
int recv_cb(int fd);
//
int send_cb(int fd);

struct conn_item {
	int fd;



	char rbuffer[BUFFER_LENGTH];
	int rlen;
	char wbuffer[BUFFER_LENGTH];
	int wlen;

	char resource[BUFFER_LENGTH];  //abc.html
	
	union  {
		RCALLBACK accept_callback;
		RCALLBACK recv_callback;

	}recv_t;
	RCALLBACK send_callback;
};

int epfd = 0;
struct conn_item connlist[1048576] = {0};//1024 2G 2*1024*1024*1024
//list动态数组
struct timeval tv_begin;

#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000) //得到ms

#if 0
struct reactor {

	int epfd;
	struct conn_item *connlist;
};
else

#endif

//block

#if ENABLE_HTTP_RESPONSE

#define ROOT_DIR  "/home/khb/2.1.1-multi-io/"

typedef struct conn_item connection_t;

//http://192.168.17.125:2048/index.html
//GET /index.html HTTP/1.1
//http://192.168.17.125:2048/abc.html
//GET /abc.html HTTP/1.1

int http_request(connection_t * conn) {
//GET / HTTP/1.1 Host: 192.168.17.125:2048
//	conn->rbuffer;
//	conn->rlen;

	return 0;
}


//ngix
int http_response(connection_t *conn) {

#if 1
	conn->wlen = sprintf(conn->wbuffer,
		"HTTP/1.1 200 OK\r\n"
		"Accept-Ranges:bytes\r\n"
		"Content-Length: 82\r\n"
		"Content-Type: text/html\r\n"
		"Date: Sat, 06 Aug 2024 10:39:51 GMT\r\n\r\n"
		"<html><head><title>0voice.king</title></head><body><h1>King</h1></body></html>\r\n\r\n");

#else

	int filefd = open("index.html", O_RDONLY);

	struct stat stat_buf;
	fstat(filefd, &stat_buf);

	conn->wlen = sprintf(conn->wbuffer,
	"HTTP/1.1 200 OK\r\n"
	"Accept-Ranges:bytes\r\n"
	"Content-Length: %ld\r\n"
	"Content-Type: text/html\r\n"
	"Date: Sat, 06 Aug 2024 10:39:51 GMT\r\n\r\n", stat_buf.st_size);

	int count = read(filefd, conn->wbuffer + conn->wlen, BUFFER_LENGTH - conn->wlen);
	conn->wlen += count;

	//sendfile
#endif
	return conn->wlen; 
} 
#endif




int set_event(int fd, int event, int flag) {

	if(flag) {//1 add, 0 mod 
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd; //添加到kv存储中
		epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	
	}else {
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd; //添加到kv存储中
		epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);

	}

}

int accept_cb(int fd) {

	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	
	int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
	if (clientfd < 0) {
		return -1;
	
	}

	set_event(clientfd, EPOLLIN, 1);

	connlist[clientfd].fd = clientfd;
	memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].rlen = 0;
	memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].wlen = 0;
	
	connlist[clientfd].recv_t.recv_callback = recv_cb;
	connlist[clientfd].send_callback = send_cb;

	if ((clientfd % 1000) == 999) {

		struct timeval tv_cur;
		gettimeofday(&tv_cur, NULL);
		int time_used = TIME_SUB_MS(tv_cur, tv_begin);

		memcpy(&tv_begin, &tv_cur, sizeof(struct timeval));
		
		printf("clientfd: %d, time_used: %d\n", clientfd, time_used);



	}
	
	return clientfd;

}

//recv
//buffer -->
int recv_cb(int fd) {

	char *buffer = connlist[fd].rbuffer;
	int  idx = connlist[fd].rlen;
	
	int count = recv(fd, buffer+idx, BUFFER_LENGTH-idx, 0);
	if (count == 0) {
		//printf("disconnect\n");
	
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);
		return -1;
	}
	connlist[fd].rlen += count;
	
	//send(fd, buffer, connlist[fd].idx, 0);
	//event 修改事件
#if 1 //echo:need to send
	memcpy(connlist[fd].wbuffer, connlist[fd].rbuffer, connlist[fd].rlen);
	connlist[fd].wlen = connlist[fd].rlen;
	connlist[fd].rlen -= connlist[fd].rlen;
#else
	http_request(&connlist[fd]);
	http_response(&connlist[fd]);
	
#endif
	set_event(fd, EPOLLOUT, 0);

	return count;
}

int send_cb(int fd) {

	char *buffer = connlist[fd].wbuffer;
	int  idx = connlist[fd].wlen;
	int count = send(fd, buffer, idx, 0);

	set_event(fd, EPOLLIN, 0);

	return count;
}


int init_server(unsigned short port){

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);

	if(bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))){
		perror("bind");
		return -1;
	}
	
	listen(sockfd, 10);

	return sockfd;

}


//tcp
int main(){
	int port_count = 20;
	unsigned short port = 2048;
	int i = 0;

	epfd = epoll_create(1);//int size 参数已经失效 但必须大于0

	for(i = 0; i < port_count; i ++) {
		int sockfd = init_server(port + i); //2048,2049,2050,2051...2057
		connlist[sockfd].fd = sockfd;
		connlist[sockfd].recv_t.accept_callback = accept_cb;
		set_event(sockfd, EPOLLIN, 1);	

	}

//pthread_create();

	gettimeofday(&tv_begin, NULL);


	struct epoll_event events[1024] = {0};
	while (1) { //mainloop();

		int nready = epoll_wait(epfd, events, 1024, -1);//数量

		int i = 0;
		for (i = 0; i < nready; i ++) {

			int connfd = events[i].data.fd;
			/*
			if (sockfd == connfd) {
				//int clientfd = accept_cb(sockfd);
				int clientfd = connlist[sockfd].accept_callback(sockfd);

				printf("clientfd: %d\n", clientfd);
			}else */
			if (events[i].events & EPOLLIN){

				//int count = connlist[connfd].recv_callback(connfd);
				int count = connlist[connfd].recv_t.recv_callback(connfd);
				//printf("recv count : %d <----  buffer: %s\n", count, connlist[connfd].rbuffer);
			} else if(events[i].events & EPOLLOUT) {
				//int count = send_cb(connfd);
				//printf(" send --> buffer: %s\n", connlist[connfd].wbuffer);
				int count = connlist[connfd].send_callback(connfd);
				//printf(" send --> buffer: %s\n", connlist[connfd].wbuffer);
				
			}
				
		}
	}
	
	



	
	getchar();
	//close(clientfd);		

	
}

//c1000k

