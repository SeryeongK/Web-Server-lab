#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    /* Enough space for any address. accept로 보내지는 소켓 주소 구조체 */
    struct sockaddr_storage clientaddr; /* sockaddr_storage 구조체: 모든 형태의 소켓 주소를 저장하기에 충분히 크며, 코드를 프로토콜-독립적으로 유지 */
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
        5444
    }

    /* 듣기 식별자 오픈 */
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); /* 클라이언트로부터 연결 요청 기다림*/
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                    client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port); /* 도메인이름과 연결된 클라이언트의 포트를 출력 */
        echo(connfd);                                                    /* 클라이언트를 서비스 */
        Close(connfd);                                                   /* 클리이언트와 서버가 자신들의 식별자를 닫은 후에 연결 종료 6*/
    }
    exit(0);
}