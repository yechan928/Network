/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int is_head);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,char *longmsg);

void doit(int fd)
{ 
  int is_static;        // 정적(static) 콘텐츠 여부 저장 플래그
  struct stat sbuf;     // 파일 정보(stat) 저장 구조체
  // buf : 요청라인 한 줄 읽어올 버퍼, method : http의 메소드 저장, uri : 요청된 uri 저장, version : http 버전 저장
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];   
  char filename[MAXLINE], cgiargs[MAXLINE];   // filename : 실제 파일 경로로 변환된 이름 저장, cgiargs : CGI 인자(query string) 저장
  rio_t rio;            // CSAPP Robust I/O 버퍼 구조체

  // 1. 요청라인 읽고 분석
  Rio_readinitb(&rio, fd);                            // rio를 fd 소켓과 연결하여 버퍼 초기화
  Rio_readlineb(&rio, buf, MAXLINE);                  // 클라이언트 요청의 첫 줄(요청 라인)을 buf에 저장 
  printf("Request headers : \n");
  printf("%s", buf);                                  // 디버그용: 읽어온 요청 라인 출력
  sscanf(buf, "%s %s %s", method, uri, version);      // buf에서 method, URI, version 토큰 파싱
  
  int is_head = (strcasecmp(method, "HEAD") == 0);

  // 2. Tiny는 "GET"메소드만 지원하기 때문에 다른 메소드 요청시 에러 
  if(strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){                          // method가 GET 아니면 if문 실행
    clienterror(fd, method, "501", "Not implemented",     // 501 Not implemented 에러 (의미 : 서버가 요청된 기능(메소드)을 아직 구현하지 않았다)
                "Tiny does not implement this method");
    return;
  }
  
  /* 3. 예외처리: HTTP/1.1 요청이라면 Host 헤더가 반드시 있는지 검사 */
  if (strcasecmp(version, "HTTP/1.1") == 0) {
    char header[MAXLINE];           
    int has_host = 0;

    /* 헤더를 몽땅 한 번에 읽으면서, HTTP/1.1이면 Host 검사도 함께 */
  Rio_readlineb(&rio, header, MAXLINE);
  while (strcmp(header, "\r\n")) {
      if (!strncasecmp(header, "Host:", 5))
          has_host = 1;
      Rio_readlineb(&rio, header, MAXLINE);
  }

  /* HTTP/1.1인데 Host가 없으면 에러 */
  if (strcasecmp(version, "HTTP/1.1") == 0 && !has_host) {
      clienterror(fd, version, "400", "Bad Request", "Missing Host header");
      return;
    }

  }

  // 4. URI 파싱: 정적/동적 콘텐츠 구분 ->uri로 파싱하는 이유 : HTTP 요청에서 어떤 자원(파일이나 CGI 프로그램)을 달라고 했는지는 전부 uri 안에 들어 있기 때문
  is_static = parse_uri(uri, filename, cgiargs); // filename : 실제 파일 또는 프로그램 경로 저장 , cgiargs : CGI용 인자 저장

  // 5. 파일 존재 여부 확인 
  if(stat(filename, &sbuf)<0){    // 파일이 없으면 404오류(파일을 찾을 수 없음)
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  // 6. 정적 콘텐츠 처리 
  if (is_static){   // 정적 콘텐츠이면 if문 실행
    if(!(S_ISREG(sbuf.st_mode))|| !(S_IRUSR & sbuf.st_mode)){        // 일반 파일이고(readable) 권한이 있는지 검사
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file"); // 일반파일이 아니고 사용자 읽기 권한 없으면 403 오류 (403 오류 : 권한이 없을때)
      return;
    }
    serve_static(fd, filename, sbuf.st_size, is_head);   // 파일 내용 전송
  }
  else{   // 동적 콘텐츠(CGI)이면 
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){ // 위에와 마찬가지 
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);       // CGI 프로그램 실행 및 결과 전송
  }

}

// 일부 명백한 오류에 대해 체크하며 이러한 오류들을 클라이언트에게 알려주는 함수 
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];    //buf : 응답 헤더를 임시로 저장할 버퍼, body : HTML본문을 생성해서 저장할 버퍼

  // errum : 오류 코드, shortmsg : 오류 메세지 , longmsg : 상세 메세지, cause : 원인
  
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n",body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n",errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");                        // Content-Type 헤더 : 본문이 HTML임을 알림
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));      // Content-length 헤더 : 본문 길이(바이트) 지정, 뒤에 빈 줄(\r\n)로 헤더 끝표시
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));                                 // 생성된 HTML 본문을 클라이언트에 전송
}

// parse_uri: 클라이언트 요청 URI를 분석하여
//   • 정적 콘텐츠 요청(static)을 나타내면 filename과 빈 cgiargs를 설정하고 1 반환
//   • 동적 콘텐츠 요청(CGI)을 나타내면 filename과 cgiargs를 분리하여 설정하고 0 반환
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;    // URI에서 '?'위치를 찾기 위한 포인터

  if(!strstr(uri, "cgi-bin")){                    // 정적콘텐츠인 경우, uri에 cgi-bin이 없으면 정적 파일 요청 
    strcpy(cgiargs, "");                          // cgi가 없으니까 빈 문자열로 초기화
    strcpy(filename, ".");                        // 현재 디렉토리 기준으로 파일 경로 시작
    strcat(filename, uri);                        // filename = "." +uri
    if(uri[strlen(uri)-1] == '/') strcat(filename, "home.html");      // uri가 "/"로 끝나면 (디렉토리 요청-> home.html 추가)
    return 1;                                     // 정적 콘텐츠 처리로 하기 위해 1 반환
  }
  else{                                           // 정적 콘텐츠인 경우 uri에 cgi-bin이 있으면 동적 파일 요청
    ptr = index(uri,'?');                         // uri에서 ? 의 위치 검색 (?를 기준으로 분리하는 이유 : <스크립트 경로>?<인자> 와 같은 형태이기 때문에 구분해서 따로 담아줘야한다. 전체를 파일이름 취급하면 404에러남)
    if(ptr){                                      // ?가 존재하면 
      strcpy(cgiargs, ptr+1);                     // '?' 다음부터 끝까지를 CGI 인자로 복사
      *ptr = '\0';                                // '?' 를 문자열 끝('\0')로 바꿔 uri 분리 
    }
    else
      strcpy(cgiargs,"");                         // ?가 없으면 CGI는 빈 문자열
    strcpy(filename,".");                         // 현재 디렉토리 기준으로 파일 경로 시작
    strcat(filename,uri);                         // filename = "."+ uri부터 '?'까지
    return 0;                                     // 동적 콘텐츠 처리로 하기 위해 0 반환
  }
}


// 클라이언트에게 정적 파일(HTML, 이미지, CSS 등)의 내용을 HTTP 프로토콜에 맞춰 전송하는 역할
void serve_static(int fd, char *filename, int filesize, int is_head)
{
  int srcfd;      // 파일 디스크립터 저장 변수
  char *srcp, filetype[MAXLINE], buf[MAXBUF];   // srcp : mmap으로 매핑된 파일의 시작 주소 , filetype : 파일의 MIME 타입 문자열, buf :응답 헤더를 저장할 버퍼

  get_filetype(filename, filetype);                                   // filename 확장자 분석해 filetype에 "text/html" 등 설정
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                                // 상태 라인 작성
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);                 // Server 헤더 추가
  sprintf(buf, "%sConnection : close\r\n", buf);                      // Connection 헤더 추가 (연결 닫기)
  sprintf(buf, "%sContent-length : %d\r\n", buf, filesize);           // 콘텐츠 길이 헤더 추가
  sprintf(buf, "%sContent-type : %s\r\n\r\n", buf, filetype);         // 콘텐츠 타입 헤더 추가 및 빈 줄로 헤더 종료
  Rio_writen(fd, buf, strlen(buf));                                   // 클라이언트에게 헤더 전송
  printf("Response headers:\n");                                      // 디버그용: 콘솔에 헤더 출력
  printf("%s", buf);                                                  // 디버그용: 작성된 헤더 출력

  srcfd = Open(filename, O_RDONLY, 0 );                               // 파일을 읽기 전용으로 오픈

  /* GET일 때만 파일 바이트 전송, HEAD는 스킵 */
    if (!is_head) {
          srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
          Close(srcfd);
          Rio_writen(fd, srcp, filesize);
          Munmap(srcp, filesize);
    }
  //srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0 );        // 파일 전체를 읽기 전용 private 매핑
  // Mmap을 쓴 이유는 메모리처럼 직접 다루기 위함
  // 일반적으로 read()로 파일을 읽어 버퍼에 복사한 뒤, write()로 다시 네트워크에 복사해야 하지만,
  // mmap은 커널 페이지 테이블에 파일을 매핑(mapping)해 두고, 
  // 사용자 공간에서는 단순히 메모리 접근만으로 파일 데이터를 읽을 수 있도록 해 준다. 
  // 따라서 복사 횟수가 줄어들어 대용량 전송 시 성능 이점이 큽니다.
  //Close(srcfd);                                                       // 매핑 후 더 이상 필요 없는 파일 디스크립터 닫기
  //Rio_writen(fd, srcp, filesize);                                     // 매핑된 파일 내용을 클라이언트에 전송
  //Munmap(srcp, filesize);                                             // 메모리 매핑 해제
/* malloc, rio_readn, rio_writen을 사용해서 연결 식별자에게 복사해보기 -숙제 11.9
  // 1) malloc으로 버퍼 할당 
  srcp = malloc(filesize);
  if (srcp == NULL) {
      clienterror(fd, filename, "500", "Internal server error",
                  "Tiny couldn't allocate memory");
      return;
  }

  // 2) 파일을 읽어서 버퍼에 채우기 
  srcfd = Open(filename, O_RDONLY, 0);
  Rio_readn(srcfd, srcp, filesize);   // Rio_readn으로 정확히 filesize 바이트 읽기
  Close(srcfd);

  // 3) 클라이언트로 파일 전송 
  Rio_writen(fd, srcp, filesize);

  // 4) 할당 해제 
  free(srcp);
  */
}

// get_filetype : 요청된 파일 이름(filename)의 확장자를 검사하여 알맞은 MIME 타입 문자열을 filetype에 복사하는 함수
// strstr(haystack, needle): 문자열 haystack 안에 needle이 있으면 그 위치 포인터, 없으면 NULL 반환
// strcpy(dest, src): src 문자열 전체를 dest 버퍼에 널 종료(\0)까지 복사
void get_filetype(char *filename, char *filetype){
  if(strstr(filename, ".html"))         // filename에 ".html"이 포함되어 있으면
    strcpy(filetype, "text/html");      //  → HTML 타입 복사
  else if (strstr(filename, ".gif"))    // filename에 ".gif"이 포함되어 있으면
    strcpy(filetype, "image/gif");      //  → GIF 타입 복사
  else if (strstr(filename, ".png"))    // filename에 ".png"이 포함되어 있으면
    strcpy(filetype, "image/png");      //  → png 타입 복사
  else if (strstr(filename, ".jpg"))    // filename에 ".jpg"이 포함되어 있으면
    strcpy(filetype, "image/jpeg");     //  → jpeg 타입 복사
  // 숙제 11.7
  else if (strstr(filename, ".mpg")||strstr(filename, ".mpeg"))
    strcpy(filetype, "vedio/mpeg");     
  else                                  // 위 확장자 모두 아닐때
    strcpy(filetype, "text/plain");     //  → 기본 텍스트 타입 복사
}

//serve_dynamic 함수는 CGI 프로그램을 실행하여 동적 콘텐츠를 생성하고, 그 결과를 HTTP 응답으로 클라이언트에 전송하는 역할
void serve_dynamic(int fd, char *filename, char *cgiargs)
// fd: 클라이언트 소켓 디스크립터. HTTP 헤더와 CGI 출력을 전송할 대상
// filename: 실행할 CGI 프로그램 경로.
// cgiargs: URI에서 추출한 쿼리 스트링. QUERY_STRING 환경 변수로 설정되어 CGI 프로그램이 사용함
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* 1. 상태 라인 및 기본 헤더 전송 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                    // HTTP 상태 라인
  Rio_writen(fd, buf, strlen(buf));                       // 클라이언트로 전송
  sprintf(buf, "Server : Tiny Web Server\r\n");           // Server 헤더
  Rio_writen(fd, buf, strlen(buf));                       // 클라이언트로 전송


  /* 2. CGI 실행을 위해 자식 프로세스 생성
   *   - 자식 프로세스에서만 Execve를 호출해도 부모(Tiny 서버)는 계속 살아남아
   *     다른 요청을 처리할 수 있음 (서버 프로세스 보호)
   *   - 표준출력 리다이렉션(Dup2)도 자식에게만 적용되므로, 부모 서버 로직에 영향 없음
   *   - 나중에 여러 자식을 띄워 동시 처리(concurrency)로 확장하기에도 유리
   */

  if (Fork()==0){
    setenv("QUERY_STRING", cgiargs, 1);       // CGI 프로그램에 전달할 환경 변수 설정
    // 환경 변수 name("QUERY_STRING")을 값 value(cgiargs 문자열)으로 설정
    
    Dup2(fd, STDOUT_FILENO);                  // 자식 프로세스의 표준 출력을 클라이언트 소켓으로 리다이렉트
    //자식 프로세스에서 이후에 하는 모든 표준 출력(printf나 write(1, ...) 등)이 fd 소켓으로 가도록 하는 것
    // CGI 프로그램이 표준 출력으로 HTML을 찍으면, 그 출력이 바로 클라이언트에게 전송되도록 설정하는 것
    // 자식 프로세스의 표준 출력(printf 등)을 소켓(fd)으로 연결해 놓으면, 
    // CGI 프로그램이 stdout으로 찍는 모든 내용이 TCP를 통해 클라이언트(브라우저)로 전송되어 화면에 표시
    
    Execve(filename, emptylist, environ);     // CGI 프로그램 실행
    // Execve는 “이름이 filename인 프로그램을, 인자 emptylist와 환경 변수 environ을 주고 실행”하는 시스템 호출임
    // 자식 프로세스에서 호출함으로써, CGI 프로그램만 실행되고,
    // 부모(tiny 웹서버)는 영향을 받지 않고 Wait()를 통해 종료 시기를 관리할 수 있다.
  }
  Wait(NULL);           // 자식 프로세스가 종료될 때까지 대기
}

int main(int argc, char **argv)
{
  int listenfd, connfd;                       // 듣기 소켓과 연결 소켓
  char hostname[MAXLINE], port[MAXLINE];      // 클라이언트의 호스트명과 포트번호를 저장할 버퍼
  socklen_t clientlen;                        // 클라이언트의 주소 구조체의 크기
  struct sockaddr_storage clientaddr;         // 클라이언트의 주소 정보 저장 구조체

  /* Check command line args */
  if (argc != 2)        
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);   // 사용법 출력 
    exit(1);                                          // 비정상 종료
  }

  // 듣기 소켓 생성 : 지정된 포트번호로 들어오는 연결을 기다림
  listenfd = Open_listenfd(argv[1]);     // 포트만 받는 이유 : 1 대 다 이기 때문(다: 클라이언트)              
  // 무한 루프 : 클라이언트 연결을 반복 처리
  while (1)
  {
    clientlen = sizeof(clientaddr);                               // clientaddr 구조체의 크기 설정
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);     // line:netp:tiny:accept - 연결 요청 수락 , clientaddr에 클라이언트 정보 저장
    // 클라이언트의 주소 정보를 사람이 읽을 수 있게 문자열로 변환
    Getnameinfo((SA *)&clientaddr, clientlen,       // 원본 주소 정보
                hostname, MAXLINE,                  // 호스트명 버퍼
                port, MAXLINE,                      // 포트번호 버퍼
                0);                                 // 추가 옵션 없이 기본 동작
    printf("Accepted connection from (%s, %s)\n", hostname, port);    // 접속한 클라이언트의 정보를 출력(브라우저의 포트)
    doit(connfd);  // line:netp:tiny:doit                             // 클라이언트 요청 처리 함수 호출
    Close(connfd); // line:netp:tiny:close                            // 연결 소켓 닫기 
  }
}
