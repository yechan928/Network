#include "csapp.h"

void echo(int connfd);      //클라이언트와 데이터 송수신을 담당하는 함수 선언

int main(int argc, char** argv){   
  int listenfd, connfd;                     // 서버 소켓과 클라이언트와의 연결 소켓 파일 디스크립터
  socklen_t clientlen;                      // 클라이언트 주소의 크기 
  struct sockaddr_storage clientaddr;       // 클라이언트의 주소 정보
  char client_hostname[MAXLINE], client_port[MAXLINE];    // 클라이언트의 호스트 명과 포트 번호

  if (argc != 2){   //포트 인자가 없으면 오류 메세지를 출력 (실행시./echoserveri.c 12345(포트번호) 이렇게 실행하기 때문)
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  listenfd = Open_listenfd(argv[1]);      // 서버 소켓을 열고 지정된 포트에서 수신 대기 (즉, listen인 상태)
  while(1){     //클라이언트가 연결할 때마다 처리
    clientlen = sizeof(struct sockaddr_storage);    // 클라이언트 주소 크기 설정
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);   // 클라이언트와 연결 수립(accept함수를 통해 받아들이기)
    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);  //클라이언트의 정보(주소, 길이를 이용해 호스트명과 포트 번호)를 받고 
    printf("Connected to (%s, %s)\n", client_hostname, client_port);  //클라이언트의 호스트명과 포트 번호를 출력
    echo(connfd);   //클라이언트와의 데이터 송수신 처리 함수 호출
    Close(connfd);  //클라이언트와의 연결 종료
  }
  exit(0);
}

void echo(int connfd)     // 서버가 클라이언트로 부터 받은 데이터를 그대로 다시 클라이언트에게 보내는 역할을 하는 함수
{
  size_t n;               // 읽은 데이터의 크기
  char buf[MAXLINE];      // 데이터를 저장할 버퍼
  rio_t rio;              // 리버스 I/O 구조체

  Rio_readinitb(&rio, connfd);    // rio 구조체 초기화, 클라이언트 소켓을 이용한 읽기/쓰기 준비
  
  // 클라이언트로부터 데이터를 읽고, 받은 데이터를 다시 클라이언트에게 송신
  while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){    // 데이터가 0이 아니면 계속 실행
    printf("server received %d bytes\n", (int)n);         // 받은 데이터의 크기 출력
    Rio_writen(connfd, buf, n);                           // 클라이언트에게 받은 데이터를 다시 송신
  }
}