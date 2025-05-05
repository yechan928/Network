#include "csapp.h"

int main(int argc, char ** argv){
  int clientfd;                         //클라이언트 소켓 파일 디스크립터 
  char *host, *port, buf[MAXLINE];      // 서버 호스트와 포트, 그리고 데이터를 읽고 쓸 버퍼(데이터를 임시로 저장하는 역할)
  rio_t rio;                            // 읽기/쓰기 연산을 관리하는 구조체

  if (argc != 3){     // 인자가 3개가 아니면 오류 메세지를 출력(./echoclient localhost 12345 => argc[o](실행된 프로그램), argc[1](호스트), argc[2](포트))
    fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
    exit(0);
  }
  host = argv[1];
  port = argv[2];

  clientfd = Open_clientfd(host, port);   // 서버와 연결하기 위한 클라이언트 소켓 열기
  Rio_readinitb(&rio, clientfd);          // rio 구조체 초기화, 읽기/쓰기 연산을 위한 준비

  

  // 표준 입력에서 한 줄씩 읽어서 서버로 전송하고, 서버의 응답을 받는다. 
  while(Fgets(buf, MAXLINE, stdin) != NULL){
    Rio_writen(clientfd, buf, strlen(buf));     //서버에 버퍼 데이터를 전송
    Rio_readlineb(&rio, buf, MAXLINE);          //서버의 응답을 읽음
    Fputs(buf, stdout);                         //서버의 응답을 화면에 출력
  }
  Close(clientfd);      // 서버와의 연결 종료
  exit(0);
}