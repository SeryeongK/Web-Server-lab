/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */

/* 실행하기 전에 포트를 열어야 함 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/* 단일 스레드로 동작
여러 클라이언트들이 동시에 연결 요청을 보내면 각각의 연결 요청이 순차적으로 처리됨 */
int main(int argc, char **argv) /* 인자의 개수 / 인자의 내용을 담고 있는 배열 */
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) /* 포트 번호가 전달되지 않으면. 원래는 프로그램 이름(argv[0])과 포트 번호(argv[1]) 두 개의 인자가 전달되어야 함 */
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); /* 전달된 포트 번호를 사용하여 서버 소켓을 열고, 클라이언트의 연결 요청을 기다림 */
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                       /* line:netp:tiny:accept 반복적으로 연결 요청 접수. 클라이언트의 연결 요청 수락
                                                                                        => 새로운 소켓 디스크립터(connfd, 서버와 클라이언트 간의 통신에 사용. 연결 식별자) 반환 */
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); /* 클라이언트의 IP 주소와 포트 번호를 가져와서 호스트 이름 hostname과 포트 번호 port로 변환 */
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit 클라이언트와의 트랜잭션 수행. 서버에서는 클라이언트의 요청을 수신하고, 요청에 대한 응답을 보내는 작업 수행
    Close(connfd); // line:netp:tiny:close 자신 쪽의 연결 끝을 닫음
  }
}

/* 클라이언트 요청에 대한 처리 담당 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  printf("!!!!!!: %s\n", filename);

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("????????????????????????: %s\n", filename);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); /* 요청 라인을 읽음 */
  if (strcasecmp(method, "GET"))                 /* GET 메소드일 경우, 에러 */
  {
    /* 501 에러 반환 */
    clienterror(fd, method, "501", "Not implemented",
                "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); /* 클라이언트 요청의 나머지 부분을 읽어들임 */

  /* Parse URI from GET request */
  printf("여기??????? %s\n", filename);
  is_static = parse_uri(uri, filename, cgiargs); /* 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그 설정*/
  if (stat(filename, &sbuf) < 0)                 /* 해당 파일이 디스크 상에 있지 않으면 */
  {
    /* 404 에러 반환 */
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
    return;
  }

  if (is_static)                                               /* 만일 요청이 정적 컨텐츠를 위한 것이라면 */
  {                                                            /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) /* 해당 파일이 보통 파일 / 읽기 권한을 갖고 있는지를 검증*/
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); /* 만일 그렇다면 정적 컨텐츠를 클라이언트에게 제공 */
  }
  else
  {                                                            /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) /* 요청이 동적 컨텐츠에 대한 것 / 이 파일이 실행 가능한지 검증 */
    {
      /* 403 오류 반환 */
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); /* 만일 그렇다면 해당 프로그램을 실행 -> 결과를 클라이언트에게 반환 */
  }
}

/* HTTP 응답을 응답 라인에 적절한 상태 코드와 상태 메시지와 함께 클라이언트에 보내며,
브라우저 사용자에게 에러를 설명하는 응답 본체에 HTML 파일도 함께 보냄 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* HTTP 요청 헤더를 읽어들이는 함수 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) /* '\r\n'이 나올 때까지 */
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* 클라이언트 요청 URI를 파싱하여 해당 요청이 정적인 콘텐츠인지 동적인 콘텐츠인지를 결정하고
   요청에 대한 파일 이름과 CGI 인자를 추출
   정적 콘텐츠일 경우 1, 동적 콘텐츠일 경우 0을 반환 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  printf("++++++ %s\n", filename);
  if (!strstr(uri, "cgi-bin")) /* 만일 URI에 'cgi-bin' 문자열이 포함되어 있다면
                                                                해당 요청은 동적인 콘텐츠 요청 */
  {                            /* Static content */
    strcpy(cgiargs, "");       /* CGI 인자 스트링을 지움 */
    /* URI를 상대 리눅스 경로 이름으로 변환 */
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/') /* 만일 URI가 '/' 문자로 끝난다면 */
      strcat(filename, "home.html"); /* 기본 리눅스 파일 이름을 추가 */
    return 1;
  }
  else
  { /* Dynamic content
  모든 CGI 인자들을 추출 */
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    /* 나머지 URI 부분을 상대 리눅스 파일 이름으로 변환 */
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

/* 정적 콘텐츠 처리
   지역 파일의 내용을 포함하고 있는 본체를 갖는 HTTP 응답을 보냄 */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* 파일이 존재하는지 확인 */
  if ((srcfd = open(filename, O_RDONLY, 0)) < 0)
  {
    printf("serve_static error: cannot open file %s\n", filename);
    return;
  }

  /* 파일 사이즈 확인 */
  if (filesize <= 0)
  {
    printf("serve_static error: invalid file size %d\n", filesize);
    close(srcfd);
    return;
  }
  printf("============\n %s, %s\n", filename, filetype);
  /* Send response headers to client */
  get_filetype(filename, filetype); /* 인자로 전달된 filename으로부터 파일 타입을 결정 */
  /* 클라이언트에 응답 줄과 응답 헤더를 생성 */
  printf("여기\n");
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); /* Rio_writen: 주어진 연결 식별자(fd)에 대해 주어진 버퍼(buf)에 있는 데이터를 전송 */
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client
  요청한 파일의 내용을 연결 식별자 fd로 복사해서 응답 본체를 보냄 */
  srcfd = Open(filename, O_RDONLY, 0);                        /* 읽기 위해서 filename을 오픈하고 식별자를 얻어옴 */
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); /* 리눅스 mmap함수는 요청한 파일을 가상메모리 영역으로 매핑
                                                                  mmap: 파일 srcfd의 첫 번째 filesize 바이트를
                                                                  주소 srcp에서 시작하는 사적 읽기-허용 가상 메모리 영역으로 매핑함 */
                                                              /* 🚨 파일을 메모리에 매핑하는 이유
                                                                    매핑은 파일을 메모리에 올려서 해당 파일의 내용을 가상메모리 영역에서 직접 읽을 수 있도록 함
                                                                    => 파일의 내용을 메모리에 캐싱하여 파일을 다시 읽는 데 걸리는 시간을 줄일 수 있으며
                                                                        파일 입출력 대신 메모리 접근으로 인한 성능 향상을 가져올 수 있음 */
  Close(srcfd);                                               /* 파일을 메모리에 매핑한 후에 더 이상 이 식별자는 필요없음 => 닫음 */
  Rio_writen(fd, srcp, filesize);                             /* 실제로 파일을 클라이언트에게 전송
                                                                  rio_writen: 주소 srcp에서 시작하는 filesize 바이트를
                                                                  클라이언트의 연결 식별자로 복사함 */
  Munmap(srcp, filesize);                                     /* 매핑된 가상메모리 주소를 반환 */
}

/* 동적 콘텐츠 처리 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response -> 클라이언트 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) /* 새로운 자식 프로세스 fork. 부모 프로세스에서는 자식 프로세스 ID를 반환, 자식 프로세스에서는 0을 반환 */
  {                /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);   /* QUERY_STRING 환경변수를 요청 URI의 CGI 인자들로 초기화 */
    Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client. 자식은 표준 출력을 연결 파일 식별자로 재지정. 클라이언트와 연결(직접 통신 가능)
                                              => 출력(응답 본체)은 클라이언트로 전송됨 */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* Parent waits for and reaps child. 부모 프로세스는 자식 프로세스가 완료되기를 기다림
                  자식 프로세스가 종료되면 이를 회수하고 메모리를 해제 */
}

/*
 * get_filetype - Derive file type from filename
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, '.mp4'))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}
