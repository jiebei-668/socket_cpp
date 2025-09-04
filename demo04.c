/*****************************************************************************
 * 程序名：demo04.c
 * 作者：jiebei
 * 功能：这是一个网络通信客户端，它连接一个服务端后不停的向服务端发消息，并接收服务端的消息。Ctrl-c或kill可以终止该程序
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

int sockfd = -1;

void cleanup(int sig)
{
	if(sockfd != -1) close(sockfd);
	printf("client exit...\n");
	exit(0);
}
int main(int argc, char* argv[])
{
	if(argc != 3)
	{
		printf("Usage: ./demo04 ip port\n");
		printf("example: ./demo04 127.0.0.1 8888\n");
		return -1;
	}
	signal(2, cleanup);
	signal(15, cleanup);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		printf("cilent socket() failed!\nerrno[%d]-[%s]\n", errno, strerror(errno));
		return -1;
	}
	struct sockaddr_in skad;
	memset(&skad, 0, sizeof(skad));
	skad.sin_family = AF_INET;
	skad.sin_addr.s_addr = inet_addr(argv[1]);
	skad.sin_port = htons((in_port_t)atoi(argv[2]));


	if(connect(sockfd, (struct sockaddr *)&skad, sizeof(skad)) == -1)
	{
		printf("cilent connect() failed!\nerrno[%d]-[%s]\n", errno, strerror(errno));
		close(sockfd);
		return -1;

	}
	char buf[512];
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "this is a random message...");

	int ret;
	while(true)
	{
		ret = send(sockfd, buf, strlen(buf), 0);
		if(ret <= 0)
		{
			cleanup(1);
		}
		printf("send: ===%s===\n", buf);
		ret = recv(sockfd, buf, 512, 0);
		if(ret <= 0)
		{
			cleanup(1);
		}
		printf("recv: ===%s===\n", buf);
		//sleep(1);
	}
	return 0;
}
