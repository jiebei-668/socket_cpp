/*****************************************************************************
 * 程序名：demo03.cpp
 * 作者：jiebei
 * 功能：这是一个多线程网络通信服务端，它接受客户端连接后将接受到的来自客户端的消息原封不动发送回去。信号2（Ctrl-c）或信号15可以终止该程序
 ***************************************************************************/
#include <stdio.h>
#include <vector>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define MAX_BACKLOG 512

int listen_fd = -1;
// 存放所有与客户端通信的子线程的线程id
std::vector<pthread_t> v_thid;
// 操作v_thid的锁
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// 和客户端进行通信的线程函数
void *thmain(void *);
// 与客户端通信的线程的清理函数
void thmain_cleanup(void *arg);
// 服务端主动关闭时的清理函数
void exit_fun(int sig);

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("Usage: ./demo03 port\n");
		printf("example: ./demo03 8888\n");
		return -1;
	}
	signal(2, exit_fun);
	signal(15, exit_fun);
	
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);	
	if(listen_fd == -1)
	{
		printf("server socket() failed!\n");
		printf("errno[%d] info[%s]\n", errno, strerror(errno));
		exit(-1);
	}
	struct sockaddr_in server;
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[1]));
	server.sin_addr.s_addr = inet_addr("127.0.0.1");

	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

	if(bind(listen_fd, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		printf("server bind() failed!\n");
		printf("errno[%d] info[%s]\n", errno, strerror(errno));
		close(listen_fd);
		exit(-1);
	}

	if(listen(listen_fd, MAX_BACKLOG) == -1)
	{
		printf("server listen() failed!\n");
		printf("errto[%d] info[%s]\n", errno, strerror(errno));
		close(listen_fd);
		exit(-1);
	}
	int connfd = -1;
	while(true)
	{
		connfd = accept(listen_fd, NULL, NULL);
		if(connfd == -1)
		{
			continue;
		}
		pthread_t id;
		pthread_create(&id, NULL, thmain, (void *)(long)connfd);
		pthread_mutex_lock(&mutex);
		v_thid.push_back(id);
		pthread_mutex_unlock(&mutex);
	}
	return 0;
}
void exit_fun(int sig)
{
	printf("server exit...\n");
	pthread_mutex_lock(&mutex);
	for(auto one: v_thid)
	{
		if(one != 0)
		{
			pthread_cancel(one);
		}
	}
	pthread_mutex_unlock(&mutex);
	// 这里的sleep为了使通信线程有充分时间做善后工作
	sleep(2);

	// 用于测试线程是否有序退出，互斥锁是否成功	
	/*
	int remain = 0;
	pthread_mutex_lock(&mutex);
	for(auto one: v_thid)
	{
		//printf("%ld\n", one);
		remain += (one != 0);
	}
	pthread_mutex_unlock(&mutex);
	printf("remain thid=%d\n", remain);
	*/
	exit(0);
}
void thmain_cleanup(void *arg)
{
	int connfd = (int)(long)arg;
	close(connfd);
	pthread_mutex_lock(&mutex);
	pthread_t thid = pthread_self();
	for(int ii = 0; ii < v_thid.size(); ii++)
	{
		if(v_thid[ii] == thid)
		{
			v_thid[ii] = 0;
			//v_thid.erase(v_thid.begin()+ii);
			break;
		}
	}
	pthread_mutex_unlock(&mutex);
	printf("通信子线程[%ld]退出。。。\n", pthread_self());
	pthread_exit(NULL);
}
// @param: 强转为void *类型的连接的客户端的sockid
// 与客户端通信的线程有两种退出方式，一种是客户端主动断开连接，这种情况需要在退出时主动关掉通信socket和将自己线程id从v_thid中去掉，另一种是服务端程序主动要退出，这种情况需要主线程做善后工作，取消所有通信线程，关掉所有socket
void *thmain(void *arg)
{
	// 分离线程
	pthread_detach(pthread_self());
	pthread_cleanup_push(thmain_cleanup, arg);
	int connfd = (int)(long)arg;
	int ret = -1;
	char buf[512];
	memset(buf, 0, sizeof(buf));
	while(true)
	{
		ret = recv(connfd, buf, 512, 0);
		if(ret <= 0)
		{
			break;
		}
		// printf("recv from client[%d]: ==%s==\n", connfd, buf);
		ret = send(connfd, buf, 512, 0);
		if(ret <= 0)
		{
			break;
		}
		// printf("send to client[%d]: ==%s==\n", connfd, buf);
	}
	printf("客户端[%d]已断开连接。。。\n", connfd);
	// 线程清理函数三个触发条件：被取消 pthread_exit 运行到pthread_cleanup_pop
	pthread_cleanup_pop(1);
	return NULL;
}
