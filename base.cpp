#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

//#include "myredis.h"
#define MAXLINE_READ_WRITE_BUF_SIZE 4096

//#include "hiredis/hiredis.h"
//#include "log4cpp/Category.hh"
//#include "log4cpp/PropertyConfigurator.hh"

typedef struct connection_struct {
    int sock;
    unsigned char index; /* which epoll fd this conn belongs to*/
    bool useing;
/*
    int roff;
    char rbuf[MAXLINE_READ_WRITE_BUF_SIZE];
    int woff;
    char wbuf[MAXLINE_READ_WRITE_BUF_SIZE];
*/
}*connection_t;
#define debugmod 0
#define CONN_MAXFD 1048576	
//65536
connection_t g_conn_table;

typedef void(*ReceiveFun)(long sockid, char *msg);
typedef void(*ConnectFun)(long sockid, char *msg);
typedef void(*DisConnectFun)(long sockid, char *msg);
ReceiveFun gReceive = NULL;
ConnectFun gConnect = NULL;
DisConnectFun gDisConnect = NULL;

static sig_atomic_t shut_server = 0;

int g_QPScount = 0;

void shut_server_handler(int signo) {
    shut_server = 1;
	printf("shut_server=1\n");
}

#define EPOLL_NUM 4
int g_epfd[EPOLL_NUM];
int g_lisSock;

#define WORKER_PER_GROUP 5

#define NUM_WORKER (EPOLL_NUM * WORKER_PER_GROUP)
pthread_t worker[NUM_WORKER]; /* echo group has 6 worker threads */


void closeConnection(connection_t conn)
{
	conn->useing = false;
    if (epoll_ctl(g_epfd[conn->index], EPOLL_CTL_DEL, conn->sock, NULL) == -1) {
		perror("epoll_ctl:del error");
	}
    close(conn->sock);
}


/*
bool setredis(redisContext* rc, connection_t conn)
{
	char* buf[MAXLINE_READ_WRITE_BUF_SIZE];
	if (conn->roff<=0)
	{
		printf("roff is 0!\n");
		return false;
	}
	memcpy(buf,conn->rbuf,conn->roff);
	buf[conn->roff] = '\0';	
	redisReply *reply = (redisReply *)redisCommand(rc,"SET %d %s", conn->sock, buf);
	if (strcmp(reply->str,"OK")!=0)
	    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);
	return true;
}

bool getredis(redisContext* rc, connection_t conn)
{
	redisReply *reply = (redisReply *)redisCommand(rc,"GET %d", conn->sock);
    //printf("GET: %s\n", reply->str);
	if (reply->len >= MAXLINE_READ_WRITE_BUF_SIZE)
	{
		printf("read redis is big than MAXLINE_READ_WRITE_BUF_SIZE , len=[%d]!\n", reply->len);
    	freeReplyObject(reply);
		return false;
	}
	memcpy(conn->wbuf, reply->str, reply->len);
	conn->woff = reply->len;
    freeReplyObject(reply);
	return true;
}
*/

/*
///
bool setredis(myredis* rc, connection_t conn, char*buf, int size)
{
	if (size<=0)
	{
		printf("set redis size is 0!\n");
		return false;
	}
	buf[size] = '\0';
	rc->fm("%d", conn->sock);
	rc->set(rc->f, buf);
	return true;
}
bool getredis(myredis* rc, connection_t conn, char* buf, int& getSize)
{
	rc->fm("%d",conn->sock);
	int size = rc->get(rc->f,buf,MAXLINE_READ_WRITE_BUF_SIZE);
	//it will append '\0' after ending
	if (size<=0)
	{
		return false;
	}
	getSize = size;
	//printf("size=%d,[%s]\n", size, conn->wbuf);
	return true;
}
///
*/

///int handleWriteEvent(connection_t conn, myredis *rc, char* wbuf, int size) 
int handleWriteEvent(connection_t conn, char* wbuf, int size) 
{
	int woff = size;
	if (!conn->useing) 
	{
		perror("socket write event useing=0\n") ;
		return -1;
	}
   	if (woff == 0) 
	{
		perror("socket write event woff=0\n") ;
		return 0;
	}
	bool wok = false;
	int nwrite = 0;
	int count = 0;
	while(1)
	{
		nwrite = write(conn->sock, wbuf + count, woff);
		//printf("total=%d,nwrite=%d\n",strlen(head)+1, nwrite);
		if (nwrite < 0)
		{
			//linux叫做EAGAIN，windows叫做EWOULDBLOCK
			if (errno == EAGAIN)
			{
			 	// 对于nonblocking 的socket而言，这里说明了已经全部发送成功了
				wok = true;
				break;
			}
			else if(errno == ECONNRESET)
			{
				// 对端重置,对方发送了RST
				printf("[w ECONNREST]ERROR: errno = %d, strerror = %s \n", errno, strerror(errno));
				closeConnection(conn);
				break;
			}
			else if (errno == EINTR)
			{
				// 被信号中断
				perror("write EINTR");
				continue;
			}
			else
			{
				perror("other");
				closeConnection(conn);
				// 其他错误
			}
		}

		if (nwrite == 0)
		{
			 // 这里表示对端的socket已正常关闭.
			closeConnection(conn);
			//perror("counterpart has shut off");
			if (gDisConnect!=NULL)
			{
				gDisConnect(conn->sock, "socket closed(1)");
			}
			break;
		}

		 // 以下的情况是writenLen > 0
		count += nwrite;
		woff -= nwrite;
		if (count >= MAXLINE_READ_WRITE_BUF_SIZE)
		{
			printf("write thistimesize=%d woff=%d full! sock=%d\n", nwrite, woff, conn->sock);
			closeConnection(conn);
			break;
		}
		wok = true;
		break; // 退出while(1)
	 }
	//if (nwrite!=10)
	//       printf("1nwrite=%d\n", nwrite);
	 if (wok)
	 {
		if (woff != 0)
			printf("not write clean! sock=%d\n", conn->sock);
	//   if (nwrite!=10)
	//       printf("nwrite=%d\n", nwrite);
		if ((g_QPScount) % 100000 == 0)
			printf("sent: no=%d, sock=%d\n",g_QPScount, conn->sock);
		++g_QPScount;
		return count;
	}
    return -1;
}




///int handleReadEvent(connection_t conn, myredis *rc)
int handleReadEvent(connection_t conn)
{
	if (!conn->useing)
	{
		perror("socket read event useing=0\n") ;
		return -1;
	}
	char readbuf[MAXLINE_READ_WRITE_BUF_SIZE+1];
	bool readok=false;
	int nread = 0;
	int roff = 0;
	while (1)
	{
	    nread = read(conn->sock, readbuf + roff, MAXLINE_READ_WRITE_BUF_SIZE - roff);
		if (nread < 0)
		{
			//linux叫做EAGAIN，windows叫做EWOULDBLOCK
			if (errno == EAGAIN)
			{
				// 由于是非阻塞的模式,所以当errno为EAGAIN时,表示当前缓冲区已无数据可读        
				// 在这里就当作是该次事件已处理处.
				readok=true;
				break;
			}
			else if(errno == ECONNRESET)
			{
				printf("[r ECONNREST]ERROR: errno = %d, strerror = %s ,fd=%d\n", errno, strerror(errno), conn->sock);
				// 对方发送了RST
				closeConnection(conn);
				break;
			}
			else if (errno==EINTR)
			{
				//被信号中断
				perror("read EINTR");
				continue;
			}
			else
			{
				//其他不可以弥补错误  
				printf("[read]ERROR: errno = %d, strerror = %s \n", errno, strerror(errno));
				closeConnection(conn);
				break;
			}
		}
		if (nread == 0)
		{
			// 这里表示对端的socket已正常关闭.发送过FIN了。
			//printf("found socket close in,fd=%d\n", conn->sock);
			closeConnection(conn);
			if (gDisConnect!=NULL)
			{
				gDisConnect(conn->sock, "socket closed(0)");
			}
			break;
		}
		roff += nread;
		if (roff >= MAXLINE_READ_WRITE_BUF_SIZE)
		{
			printf("read thistimesize=%d roff=%d full! sock=%d\n", nread, roff, conn->sock);
			closeConnection(conn);
			break;
		}
		readok = true;
		break;
	}

	if (readok)
	{
		int getSize = roff;
		readbuf[roff]='\0';
		//printf("tttt\n");
		if (gReceive != NULL)
			gReceive(conn->sock, readbuf);
		else
		{
	/*
			if (debugmod) printf("readed size=%d,[%s]\n",roff,readbuf);		
			bool ret = setredis(rc,conn,readbuf, roff);
			if (!ret)
			{
				if (debugmod) printf("setredis error\n");
				return -1;
			}
			ret = getredis(rc,conn,readbuf, getSize);	
			if (!ret)
			{
				if (debugmod) printf("getredis error\n");
				return -1;
			}
			//printf("socket=%d,getsize=%d,roff=%d,%s\n", conn->sock, getSize, roff,readbuf);
			*/
			int retsize = 0;
			if (getSize>0)
				///retsize = handleWriteEvent(conn, rc, readbuf, getSize);
				retsize = handleWriteEvent(conn, readbuf, getSize);
			if (retsize > 0)
			{
				if (debugmod) printf("writed size=%d,[%s]\n", getSize, readbuf);
			}
			else
				if (debugmod) printf("writed error\n");
		}
		return roff;
	}
	return -1;
}

void *workerThread(void *arg) {
    int epfd = *(int *)arg;
    
    struct epoll_event event;
    struct epoll_event evReg;

	struct timeval timeout = { 1, 500000 }; 
	const char * ip = "127.0.0.1";
	int port = 6379;
	const char * ip_port = "127.0.0.1:6379";
/*
	redisContext *rc = redisConnectWithTimeout(ip,port,timeout);
	if (rc==NULL || rc->err)
	{
		printf("content to redis server[%s:%d], error[%s]\n", ip, port, rc->errstr);
		return NULL;
	}
	redisReply *reply = (redisReply *)redisCommand(rc,"auth 123456");
	if (strcmp(reply->str,"OK") != 0)
	    printf("auth: %s\n", reply->str);
    freeReplyObject(reply);	
	//redis end connect
*/
	///myredis* rc = new myredis();
	///rc->connect(ip_port); 	
	
    /* only handle connected socket */
    while (!shut_server) {
        int numEvents = epoll_wait(epfd, &event, 1, 1000);
        
        if (numEvents > 0) {
            int sock = event.data.fd;
            connection_t conn = &g_conn_table[sock];
                
            /*
			if (event.events & EPOLLOUT) {
                if (handleWriteEvent(conn, rc) == -1) {
                    continue;
                }
            }
			*/

            if (event.events & EPOLLIN) {
                ///if (handleReadEvent(conn, rc) == -1) {
                if (handleReadEvent(conn) == -1) {
                    continue;
                }
            }
                
            //if (conn->woff > 0)
			//	evReg.events = EPOLLET | EPOLLOUT | EPOLLONESHOT;
			//else 
				evReg.events = EPOLLET | EPOLLIN | EPOLLONESHOT;
	        evReg.data.fd = sock;
    	    epoll_ctl(epfd, EPOLL_CTL_MOD, conn->sock, &evReg);

        }
    }
	//redisFree(rc);
    return NULL;
}

void *listenThread(void *arg) {
    int lisEpfd = epoll_create(100000);//65535);

    struct epoll_event evReg;
    evReg.events  = EPOLLIN;
    evReg.data.fd = g_lisSock;

    epoll_ctl(lisEpfd, EPOLL_CTL_ADD, g_lisSock, &evReg);
    
    struct epoll_event event;

    int rrIndex = 0; /* round robin index */
    
    /* only handle listen socekt */
	int listenCount = 0;
	struct sockaddr_in addrClient;
	socklen_t len = sizeof(addrClient);
    while (!shut_server) {
        int numEvent = epoll_wait(lisEpfd, &event, 1, 1000);
        if (numEvent > 0) {
            int sock = accept(g_lisSock, (struct sockaddr *)&addrClient, &len);
            if (sock > 0) {
                g_conn_table[sock].useing = true;
                    
                int flag;
                flag = fcntl(sock, F_GETFL);
                fcntl(sock, F_SETFL, flag | O_NONBLOCK);
                    
                evReg.data.fd = sock;
                evReg.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
				if (listenCount%10000 == 0)
				{
					printf("accept: no=%d, sock=%d, ip=%s, port=%d\n", listenCount, sock,  inet_ntoa(addrClient.sin_addr),  ntohs(addrClient.sin_port));
				}
                            
				++listenCount;
                /* register to worker-pool's epoll,
                 * not the listen epoll */
                g_conn_table[sock].index= rrIndex;
                epoll_ctl(g_epfd[rrIndex], EPOLL_CTL_ADD, sock, &evReg);
                rrIndex = (rrIndex + 1) % EPOLL_NUM;
            }
			else
        		perror("accept");
        }
    }

    close(lisEpfd);
    return NULL;
}

int main(int argc, char *const argv[]) 
{

    int c;
	g_conn_table = (connection_t) malloc(CONN_MAXFD*sizeof(connection_struct));
    for (c = 0; c < CONN_MAXFD; ++c) {
        g_conn_table[c].sock = c;
    }
    struct sigaction act;
    memset(&act, 0, sizeof(act));

    act.sa_handler = shut_server_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
	signal(SIGPIPE, SIG_IGN);

    int epi;
    for (epi = 0; epi < EPOLL_NUM; ++ epi) {
        g_epfd[epi] = epoll_create(170000);//65535);
    }
    
    g_lisSock = socket(AF_INET, SOCK_STREAM, 0); 
    
    int reuse = 1;
    setsockopt(g_lisSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    int flag;
    flag = fcntl(g_lisSock, F_GETFL);
    fcntl(g_lisSock, F_SETFL, flag | O_NONBLOCK);

    struct sockaddr_in lisAddr;
    lisAddr.sin_family = AF_INET;
    lisAddr.sin_port = htons(8887);
    //lisAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (argc>1)
	    lisAddr.sin_addr.s_addr = inet_addr(argv[1]);
	else
	    lisAddr.sin_addr.s_addr = inet_addr("192.168.5.190");
    
    if (bind(g_lisSock, (struct sockaddr *)&lisAddr, sizeof(lisAddr)) == -1) {
        perror("bind");
        return -1;
    }

    listen(g_lisSock, 4096);
    pthread_t lisTid;
    pthread_create(&lisTid, NULL, listenThread, NULL);

    int i;

    for (i = 0; i < EPOLL_NUM; ++i) {
        int j;
        for (j = 0; j < WORKER_PER_GROUP; ++j) {
            pthread_create(worker + (i * WORKER_PER_GROUP + j), NULL, workerThread, g_epfd + i);
        }
    }
    
    for (i = 0; i < NUM_WORKER; ++i) {
        pthread_join(worker[i], NULL);
    }

    pthread_join(lisTid, NULL);
    
    struct epoll_event evReg;

    for (c = 0; c < CONN_MAXFD; ++c) {
        connection_t conn = g_conn_table + c;
        if (conn->useing) { 
            epoll_ctl(g_epfd[conn->index], EPOLL_CTL_DEL, conn->sock, &evReg);
            close(conn->sock);
        }
    }    

    for (epi = 0; epi < EPOLL_NUM; ++epi) {
        close(g_epfd[epi]);
    }
    close(g_lisSock);

    return 0;
}

extern "C" {
bool init(char*ip, int port) 
{
	static bool runonce = false;
	if (runonce) return false;
    int c;
	g_conn_table = (connection_t) malloc(CONN_MAXFD*sizeof(connection_struct));
    for (c = 0; c < CONN_MAXFD; ++c) {
        g_conn_table[c].sock = c;
    }
    struct sigaction act;
    memset(&act, 0, sizeof(act));

    act.sa_handler = shut_server_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
	signal(SIGPIPE, SIG_IGN);

    int epi;
    for (epi = 0; epi < EPOLL_NUM; ++ epi) {
        g_epfd[epi] = epoll_create(170000);//65535);
    }
    
    g_lisSock = socket(AF_INET, SOCK_STREAM, 0); 
    
    int reuse = 1;
    setsockopt(g_lisSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    int flag;
    flag = fcntl(g_lisSock, F_GETFL);
    fcntl(g_lisSock, F_SETFL, flag | O_NONBLOCK);

    struct sockaddr_in lisAddr;
    lisAddr.sin_family = AF_INET;
	int iport = port;
    lisAddr.sin_port = htons(port);
    //lisAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	lisAddr.sin_addr.s_addr = inet_addr(ip);
    
    if (bind(g_lisSock, (struct sockaddr *)&lisAddr, sizeof(lisAddr)) == -1) {
        perror("bind");
        return -1;
    }
    listen(g_lisSock, 4096);
    pthread_t lisTid;
    pthread_create(&lisTid, NULL, listenThread, NULL);

    int i;

    for (i = 0; i < EPOLL_NUM; ++i) {
        int j;
        for (j = 0; j < WORKER_PER_GROUP; ++j) {
            pthread_create(worker + (i * WORKER_PER_GROUP + j), NULL, workerThread, g_epfd + i);
        }
    }
    
    //for (i = 0; i < NUM_WORKER; ++i) {
    //    pthread_join(worker[i], NULL);
    //}
    //pthread_join(lisTid, NULL);
    runonce = true;
	printf("init(%s,%d) ok!\n", ip, iport);
	return true;
}

bool destory()
{
    struct epoll_event evReg;
    for (int c = 0; c < CONN_MAXFD; ++c) {
        connection_t conn = g_conn_table + c;
        if (conn->useing) { 
            epoll_ctl(g_epfd[conn->index], EPOLL_CTL_DEL, conn->sock, &evReg);
            close(conn->sock);
        }
    }    

    for (int epi = 0; epi < EPOLL_NUM; ++epi) {
        close(g_epfd[epi]);
    }
    close(g_lisSock);
	free(g_conn_table);
	printf("destory() ok!\n");
	return true;
}
bool sendMessage(int socketid, char* msg)
{
	connection_t conn = &g_conn_table[socketid];
	int getSize = strlen(msg);
	int retSize = handleWriteEvent(conn, msg, getSize);
    if (retSize != getSize)
       printf("writed error! msg size=%d,real write size=%d,msg=%s\n", getSize,retSize,msg);
	return retSize==getSize;
}


void setCallbackFunc(ReceiveFun pReceive, ConnectFun pConnect, DisConnectFun pDisConnect)
{
	gReceive = pReceive;	
	gConnect = pConnect;
	gDisConnect = pDisConnect;
	printf("func addr:%x,%x,%x\n", pReceive,pConnect,pDisConnect);
}

int test(int i, char* p)
{
	printf("%s\n", p);
	return 1;	
}
}
