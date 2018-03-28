#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr = { 0 };
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(8080);
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (-1 == connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)))
    {
        cout << "connection failed" << endl;
    }

    int flags = fcntl(sockfd, F_SETFL);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    char buf[1024] = { 0 };
    while (true)
    {
        cin >> buf;
        int length = strlen(buf);
        memcpy(buf + 4, buf, strlen(buf));
        memcpy(buf, &length, 4);
        int i = write(sockfd, buf, sizeof(buf));
        int n;
        char buff[1024] = { 0 };
        string s = "";
        while ((n = read(sockfd, buff, strlen(buff))) > 0)
        {
            s += buff;
        }

        cout << "get: " << s << endl;
    }
    close(sockfd);
}
