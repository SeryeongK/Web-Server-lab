#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *new_version = "HTTP/1.0";

void do_it(int fd);
void do_request(int p_clientfd, char *method, char *uri_ptos, char *host);
void do_response(int p_connfd, int p_clientfd);
int parse_uri(char *uri, char *uri_ptos, char *host, char *port);
int parse_responsehdrs(rio_t *rp, int length);

/*
파일 디스크립터: 컴퓨터 프로그램이 파일 또는 기타 입/출력 리소스를 참조하는 방법
책에 대한 도서관의 참조 번호와 같음. 리소스는 일반적으로 운영 체제에서 관리함.

rio_t 구조체는 Robust I/O(Rio)라이브러리에서 사용되는 데이터 구조체

typedef struct {
    int rio_fd;                 // 파일 디스크립터
    ssize_t rio_cnt;            // 남아 있는 데이터의 바이트 수
    char *rio_bufptr;           // 다음에 읽어들일 데이터의 위치
    char rio_buf[RIO_BUFSIZE];  // 내부 버퍼
} rio_t;
*/

int main(int argc, char **argv)
{
  int listenfd, p_connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  listenfd = Open_listenfd(argv[1]);

  while (1)
  {
    clientlen = sizeof(clientaddr);
    p_connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    do_it(p_connfd);
    Close(p_connfd); /* 클라이언트와의 연결 종료 */
  }
  return 0;
}

void do_it(int p_connfd)
{
  int p_clientfd;
  char buf[MAXLINE], host[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; /* 프록시가 요청을 보낼 서버의 IP, port */
  char uri_ptos[MAXLINE];
  rio_t rio;

  /*
  Rio_readinitb: rio_t 구조체를 초기화하는 함수
  serve_fd라는 파일 디스크립터를 사용하여 rio_t 구조체인 server_rio 초기화
  */
  Rio_readinitb(&rio, p_connfd); /* 클라이언트와의 connectoin */
  /*
  Rio_readlineb: 소켓을 통해 데이터를 읽는 함수
  rio_t 구조체 변수 / 응답 메시지를 저장하기 위한 버퍼 / 버퍼의 크기
  */
  Rio_readlineb(&rio, buf, MAXLINE); /* 클라이언트의 요청 읽기 */
  printf("Request headers to proxy:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); /* 클라이언트의 요청 파싱 => method, uri, version */

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    /* 클라이언트의 요청이 GET이나 HEAD가 아니면 501 에러 */
    printf("[PROXY] 501 ERROR\n");
    return;
  }

  parse_uri(uri, uri_ptos, host, port); /* uri 파싱 => path, hostname, path, port에 할당 */

  /* end server에 요청 보낼 준비 완료! */

  /* hostname, port 서버에 대한 connection 열기 => 서버와의 소켓 디스크립터 생성 */
  p_clientfd = Open_clientfd(host, port);
  do_request(p_clientfd, method, uri_ptos, host);
  /* end server에 요청 완료! */
  do_response(p_connfd, p_clientfd);
  Close(p_clientfd); /* 서버와의 연결 종료 */
}

void do_request(int p_clientfd, char *method, char *uri_ptos, char *host)
{
  char buf[MAXLINE];
  printf("Request headers to server: \n");
  printf("%s %s %s\n", method, uri_ptos, new_version);

  sprintf(buf, "GET %s %s\r\n", uri_ptos, new_version);
  sprintf(buf, "%sHost: %s\r\n", buf, host);
  sprintf(buf, "%s%s", buf, user_agent_hdr);
  sprintf(buf, "%sConnections: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);

  /*
  Rio_writen: 소켓을 통해 데이터를 씀
  소켓 파일 디스크립터(데이터를 보낼 대상) / HTTP 요청 메시지를 가리키는 포인터(보낼 데이터)) / HTTP 요청 메시지의 길이(보낼 데이터의 크기)
  */
  Rio_writen(p_clientfd, buf, (size_t)strlen(buf)); /* 서버에 HTTP request 메시지를 보냄 */
}

void do_response(int p_connfd, int p_clientfd)
{
  char buf[MAX_CACHE_SIZE];
  ssize_t n;
  rio_t rio;

  Rio_readinitb(&rio, p_clientfd); /* 서버 소켓과 연결 */
  n = Rio_readnb(&rio, buf, MAX_CACHE_SIZE);
  Rio_writen(p_connfd, buf, n); /* 서버에서 받은 응답 메시지를 클라이언트에게 전송 */
}

int parse_uri(char *uri, char *uri_ptos, char *host, char *port)
{
  char *ptr;

  if (!(ptr = strstr(uri, "://")))
    return -1;
  ptr += 3;
  strcpy(host, ptr);

  if ((ptr = strchr(host, ':'))) /* strchr(): 문자 하나만 찾는 함수 (''작은따옴표사용) */
  {
    *ptr = '\0';
    ptr += 1;
    strcpy(port, ptr);
  }
  else
  {
    if ((ptr = strchr(host, '/')))
    {
      *ptr = '\0';
      ptr += 1;
    }
    strcpy(port, "80");
  }

  if ((ptr = strchr(port, '/')))
  {
    *ptr = '\0';
    ptr += 1;
    strcpy(uri_ptos, "/");
    strcat(uri_ptos, ptr);
  }
  else
    strcpy(uri_ptos, "/");

  return 0;
}