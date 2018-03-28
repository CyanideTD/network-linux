#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <queue>
#include <map>
#include <hash_map>

using namespace std;

#define LISTENQ 20
#define MAXLINE 5

class entity
{
public:
    string get_data();
    int getFD();
    entity(int i, string s);
    void add_data(string s);
    ~entity()
    {

    }
private:
    int sockfd;
    string data;
};

void entity::add_data(string s)
{
    data += s;
}

string entity::get_data() 
{
    return data;
}

int entity::getFD()
{
    return sockfd;
}

entity::entity(int i, string s) :sockfd(i), data(s) {};

queue<entity> receive_queue;
hash_map<int, char*> send_map;
hash_map<int, int> receive;
hash_map<int, char*> receive_map;

static int set_nonblock(int fd)
{
    int fl = fcntl(fd, F_SETFL);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void send_messages(int epfd, epoll_event &ev) 
{
    hash_map<int, char*>::iterator it;
    it = send_map.begin();
    int n;
    while (it != send_map.end())
    {
        while (n = write(epfd, it->second, strlen(it->second)) > 0)
        {
            it->second += n;
        }
        if (errno == EAGAIN)
        {
            ev.data.fd = it->first;
            ev.events = EPOLLOUT | EPOLLET;
            epoll_ctl(epfd, EPOLL_CTL_MOD, it->first, &ev);
        }
        else
        {
            send_map.erase(it);
        }
        it++;
    }
}

void* network(void* arg)
{
    int i, maxi, listenfd, connfd, sockfd, epfd, nfds;
    int portnumber = 8080;
    ssize_t n;
    socklen_t clilen;

    epoll_event ev, events[20];

    epfd = epoll_create(256);
    struct sockaddr_in clientaddr;
    struct sockaddr_in serveraddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    ev.data.fd = listenfd;
    ev.events = EPOLLIN | EPOLLET;

    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    serveraddr.sin_port = htons(portnumber);

    if (-1 == bind(listenfd, (sockaddr*)&serveraddr, sizeof(serveraddr)))
    {
        cout << "bind failed" << endl;
    }

    listen(listenfd, LISTENQ);

    while (true)
    {
        nfds = epoll_wait(epfd, events, 20, -1);
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == listenfd && events[i].events & EPOLLIN)
            {
                connfd = accept(listenfd, (sockaddr*)&clientaddr, &clilen);

                if (connfd < 0)
                {
                    perror("error when connected");
                }
                set_nonblock(connfd);
                char* str = inet_ntoa(clientaddr.sin_addr);
                cout << "accept from" << str << ":" << ntohs(clientaddr.sin_port) << endl;
                ev.data.fd = connfd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }
            else if (events[i].events & EPOLLIN)
            {
                bool flag = true;
                sockfd = events[i].data.fd;
                char line[1024] = "";
                int k = 0;
                string s = "";
                k = read(sockfd, line, 1024);

                if (k == 0)
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                    cout << "client close" << endl;
                    continue;
                }

                if (receive.count(sockfd) == 1)
                {
                    flag = false;
                    hash_map<int, int>::iterator iter1 = receive.find(sockfd);
                    n = iter1->second;
                    n -= k;
                    s += line;
                }

                if (!flag)
                {
                    string prev = receive_map[sockfd];
                    while ((k = read(sockfd, line, sizeof(line))) > 0)
                    {
                        n -= k;
                        s += line;
                    }

                    prev += s;

                    if (n > 0)
                    {
                        receive_map[sockfd] = (char*) prev.data();
                    }
                    else
                    {
                        receive_queue.push(entity(sockfd, s));
                        receive_map.erase(sockfd);
                    }
                }
                else
                {
                    int length;
                    memcpy(&length, line, 4);
                    length -= k;
                    s += line;
                    while ((k = read(sockfd, line, sizeof(line))) > 0)
                    {
                        n -= k;
                        s += line;
                    }
                    if (length > 0)
                    {
                        receive[sockfd] = length;
                        receive_map[sockfd] = (char*) s.data();
                    }
                    else
                    {
                        receive_queue.push(entity(sockfd, s));
                    }
                }

                cout << "receive: " << s << endl;

                ev.data.fd = sockfd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
            else if (events[i].events & EPOLLOUT)
            {
                sockfd = events[i].data.fd;
                char* p = send_map[sockfd];
                while ((n = write(sockfd, p, strlen(p)) > 0))
                {
                    p += n;
                }
                if (errno == EAGAIN)
                {
                    ev.data.fd = sockfd;
                    ev.events = EPOLLOUT | EPOLLET;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                }
                else
                {
                    ev.data.fd = sockfd;
                    ev.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                }
            }
        }
    }
}

void* process(void* args)
{
    while (true)
    {
        while (!receive_queue.empty())
        {
            entity buf = receive_queue.front();
            receive_queue.pop();
            buf.add_data(" processed");
            send_map[buf.getFD()] = buf.get_data();
        }
    }
}

int main()
{
    pthread_t net;
    pthread_t pro;

    pthread_create(&net, NULL, network, NULL);
    pthread_create(&pro, NULL, process, NULL);

    pthread_join(net, NULL);
    pthread_join(pro, NULL);
}
