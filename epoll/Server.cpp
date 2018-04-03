#include <iostream>
#include <stdlib.h>
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
    return this->data;
}

int entity::getFD()
{
    return this->sockfd;
}

entity::entity(int i, string s) :sockfd(i), data(s) {};

queue<entity> receive_queue;
map<int, char*> send_map;
map<int, int> receive;
map<int, char*> receive_map;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int set_nonblock(int fd)
{
    int fl = fcntl(fd, F_SETFL);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void send_messages(int epfd, epoll_event &ev) 
{
    map<int, char*>::iterator it;
    it = send_map.begin();
    int n;
    while (it != send_map.end())
    {
        cout << it -> second << endl;
        while ((n = write(it->first, it->second, strlen(it->second))) > 0)
        {
            it->second += n;
        }
        if (n == -1)
        {
            cout << errno << endl;
            if (errno == EAGAIN)
            {
                ev.data.fd = it->first;
                ev.events = EPOLLOUT | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_MOD, it->first, &ev);
            }
            else if (errno == ECONNRESET)
            {
                epoll_ctl(epfd, EPOLL_CTL_DEL, it->first, NULL);
                close(it->first);
                cout << "client close" << endl;
                cout << "send error" << endl;
            }
        } else
        {
            cout << "send success" << endl;
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
        nfds = epoll_wait(epfd, events, 20, 100);
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == listenfd && events[i].events & EPOLLIN)
            {
                connfd = accept(listenfd, (sockaddr*)&clientaddr, &clilen);

                if (connfd < 0)
                {
                    perror("error when connected");
                    continue;
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
                bool flag = false;
                sockfd = events[i].data.fd;
                char line[1024] = { 0 };
                int k = 0;
                string s = "";
                int size = 0;
                k = read(sockfd, line, 1024);
                size += k;

                if (k <= 0)
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                    cout << "client close" << endl;
                    send_map.erase(sockfd);
                    continue;
                } 

                if (receive.count(sockfd) == 1)
                {
                    flag = true;
                    map<int, int>::iterator iter1 = receive.find(sockfd);
                    n = iter1->second;
                    n -= k;
                    s += line;
                }

                if (flag)
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
                        receive[sockfd] = n;
                    }
                    else
                    {
                        receive_queue.push(entity(sockfd, s));
                        receive_map.erase(sockfd);
                        receive.erase(sockfd);
                    }
                }
                else
                {
                    int length;
                    memcpy(&length, line, 4);
                    length -= k;
                    while ((k = read(sockfd, line + size, sizeof(line))) > 0)
                    {
                        n -= k;
                        size += k;
                    }

                    if (length > 0)
                    {
                        receive[sockfd] = length;
                        receive_map[sockfd] = (char*) s.data();
                    }
                    else
                    {
                        cout << "receive success" << endl;
                        line[size] = '\0';
                        s = line + 4;
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
                if (n == -1 && errno == EAGAIN)
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
        pthread_mutex_lock(&mutex);
        send_messages(epfd, ev);
        pthread_mutex_unlock(&mutex);
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
            if (send_map.count(buf.getFD()) == 1) 
            {
                receive_queue.push(buf);
                continue;
            }
            buf.add_data(" processed");
            string s = buf.get_data();
            send_map[buf.getFD()] = (char*) s.data();
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
