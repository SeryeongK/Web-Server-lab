#include "csapp.h"

/* getaddrinfo과 getnameinfo를 이용해서 도메인 이름과 연관된 IP 주소로의 매핑을 출력
NSLOOPUP과 유사 */
int main(int argc, char **argv)
{
    struct addrinfo *p, *listp, hints;
    char buf[MAXLINE];
    int rc, flags;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(0);
    }

    /* Get a list of addrinfo records */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;       /* IPv4 only */
    hints.ai_socktype = SOCK_STREAM; /* Connections only */
    /* getaddrinfo: 우리가 원하는 주소 리턴 */
    if ((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0) /* 도메인 이름만 반환할 것을 요구 => NULL 서비스 인자로 호출 */
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(1);
    }

    /* Walk the list and display each IP address */
    flags = NI_NUMERICHOST; /* Display address string instead of domain name */
    for (p = listp; p; p = p->ai_next)
    {
        /* getnameinfo: 각 소켓의 주소를 점-십진수 주소 스트링으로 변환하도록 함 */
        Getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, NULL, 0, flags); /* addrinfo 구조체의 리스트를 하나씩 방문 */
        printf("%s\n", buf);
    }

    /* Clean up */
    Freeaddrinfo(listp);

    exit(0);
}