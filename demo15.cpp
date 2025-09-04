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
#include <poll.h>
#include <sys/eventfd.h>

#define MAX_BACKLOG 512

#define THREAD_NUMS 5

#define MAX_FD_NUM 1024

int listen_fd = -1;
// 存放所有与客户端通信的子线程的线程id
// std::vector<pthread_t> v_thid;
// 通信子线程的线程结构
struct WorkThread
{
	pthread_t pid;
	int efd;
	std::vector<int> wait_fds;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	int max_fd;
	struct pollfd sockets[MAX_FD_NUM];
	
};
struct WorkThread threads[THREAD_NUMS];
int next_thread = 0;
// 操作v_thid的锁
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// 和客户端进行通信的线程函数
void *thmain(void *);
// 和客户端进行通信的线程函数
void *pollmain(void *arg);
// 与客户端通信的线程的清理函数
void thmain_cleanup(void *arg);
// 服务端主动关闭时的清理函数
void exit_fun(int sig);

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("Usage: ./demo15 port\n");
		printf("example: ./demo15 8888\n");
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
	
	for(int ii = 0; ii < THREAD_NUMS; ii++)
	{
		threads[ii].efd = eventfd(0, 0);
		threads[ii].max_fd = threads[ii].efd;
		for(int jj = 0; jj < MAX_FD_NUM; jj++)
		{
			threads[ii].sockets[jj].fd = -1;
		}
		threads[ii].sockets[threads[ii].efd].fd = threads[ii].efd;
		threads[ii].sockets[threads[ii].efd].events = POLLIN;
		pthread_create(&threads[ii].pid, nullptr, pollmain, (void *)&threads[ii]);
	}
	while(true)
	{
		int connfd = accept(listen_fd, NULL, NULL);
		printf("acceptor: 接收新连接 fd=%d\n", connfd);
		if(connfd == -1)
		{
			continue;
		}
		while(threads[next_thread].pid == 0)
		{
			next_thread = (next_thread+1) % THREAD_NUMS;
		}
		pthread_mutex_lock(&threads[next_thread].mutex);
		threads[next_thread].wait_fds.push_back(connfd);
		pthread_mutex_unlock(&threads[next_thread].mutex);
		uint64_t tmp = 1;
		write(threads[next_thread].efd, &tmp, 8);
		next_thread = (next_thread+1) % THREAD_NUMS;
	}
	return 0;
}
void exit_fun(int sig)
{
	printf("server exit...\n");
	pthread_mutex_lock(&mutex);
	for(auto one: threads)
	{
		if(one.pid != 0)
		{
			printf("thread[%ld] exit ...\n", one.pid);
			pthread_cancel(one.pid);
		}
	}
	pthread_mutex_unlock(&mutex);
	// 这里的sleep为了使通信线程有充分时间做善后工作
	sleep(20);

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
	struct WorkThread *pp = (struct WorkThread *)arg;
	for(int ii = 0; ii < pp->max_fd+1; ii++)
	{
		if(pp->sockets[ii].fd != -1)
		{
			printf("thread[%ld] fd=%d\n", pthread_self(), pp->sockets[ii].fd);
			// close(pp->sockets[ii].fd);
		}
	}
	pp->pid = 0;
	pthread_t thid = pthread_self();
	printf("通信子线程[%ld]退出。。。\n", pthread_self());
	pthread_exit(NULL);
}
void *pollmain(void *arg)
{
	// 分离线程
	pthread_detach(pthread_self());
	pthread_cleanup_push(thmain_cleanup, arg);
	struct WorkThread *self_st = (WorkThread *)arg;
	int ret = -1;
	char buf[512];
	memset(buf, 0, sizeof(buf));
	while(true)
	{
		int events_num = poll(self_st->sockets, self_st->max_fd+1, -1);
		if(events_num < 0)
		{
			// 这里清理掉资源（关socket）后退出
			break;
		}
		if(events_num == 0)
		{
			printf("poll timeout...\n");
			continue;
		}
		int tmp_maxfd = self_st->max_fd;
		for(int ii = 0; ii <= tmp_maxfd; ii++)
		{
			if(self_st->sockets[ii].fd == -1 || (self_st->sockets[ii].revents & POLLIN) == 0)
			{
				continue;
			}
			if(self_st->sockets[ii].fd == self_st->efd)
			{
				uint64_t tmp;
				int readRet = read(self_st->efd, &tmp, 8);
				if(readRet == -1)
				{
					printf("无法读取新连接。。。\n");
					continue;
				}
				pthread_mutex_lock(&self_st->mutex);
				// 新连接的sock如果大于maxfd，就更新maxfd
				// 将新连接的socket加入sockets
				while(!self_st->wait_fds.empty())
				{
					int connfd = self_st->wait_fds.back();
					self_st->wait_fds.pop_back();
					self_st->sockets[connfd].fd = connfd;
					self_st->sockets[connfd].events = POLLIN;
					if(connfd > self_st->max_fd)
					{
						self_st->max_fd = connfd;
					}
				}
				printf("thread[%ld] accept...\n", pthread_self());
				pthread_mutex_unlock(&self_st->mutex);
				continue;
			}
			// 打印出发送过来的消息，或者处理对端关闭情况
			memset(buf, 0, sizeof(buf));	
			int ret = recv(self_st->sockets[ii].fd, buf, 512, 0);
			if(ret <= 0)
			{
				printf("客户端[%d]已断开连接。。。\n", self_st->sockets[ii].fd);
				if(self_st->max_fd == self_st->sockets[ii].fd)
				{
					for(int jj = self_st->max_fd-1; jj >= 0; jj--)
					{
						if(~self_st->sockets[jj].fd)
						{
							self_st->max_fd = self_st->sockets[jj].fd;
							break;
						}
					}
				}
				close(self_st->sockets[ii].fd);
				self_st->sockets[ii].fd = -1;
			}
			else
			{
				// printf("thread[%ld] recv from client[%d]: ==%s==\n", pthread_self(), self_st->sockets[ii].fd, buf);
				send(self_st->sockets[ii].fd, buf, 512, 0);
			}

		}
	}
	pthread_cleanup_pop(1);
}
