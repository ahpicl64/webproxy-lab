/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <signal.h>    // 터미널 종료 시그널 함수 추가
#include <string.h>    // sigemptyset을 위해 추가
#include <errno.h>     // 추가: errno 사용을 위해

void doit(int fd); // 클라이언트 요청 처리
void read_requesthdrs(rio_t *rp); // HTTP 요청 헤더 모두 읽기
int parse_uri(char *uri, char *filename, char *cgiargs); // URI를 파일 경로와 CGI 인수로 분해
void serve_static(int fd, char *filename, int filesize); // 정적 콘텐츠 전송
void get_filetype(char *filename, char *filetype); // 파일 타입 결정
void serve_dynamic(int fd, char *filename, char *cgiargs); // CGI 이용 동적 콘텐츠 전송
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg); // HTTP 오류 응답 전송

// 서버 종료용 시그널 핸들러 추가
void sigint_handler(int signo) {  // SIGINT 시 호출되는 함수
    _exit(0);
}


// 프로그램 진입점: 인자 처리 및 서버 소켓 준비
int main(int argc, char **argv) {
    int listenfd, connfd; // int: 32비트 정수형, 소켓 디스크립터 저장용
    char hostname[MAXLINE], port[MAXLINE]; // char[]: 고정 크기 문자열 버퍼
    socklen_t clientlen; // socklen_t: 소켓 주소 길이 저장 타입
    struct sockaddr_storage clientaddr; // sockaddr_storage: IPv4/IPv6 모두 지원하는 주소 구조체

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]); // 포트 번호 미입력 시 사용법 출력
        exit(1); // 프로그램 종료
    }
    
    struct sigaction action;
    action.sa_handler = sigint_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);

    
    listenfd = Open_listenfd(argv[1]); // 서버 듣기 소켓 생성 및 바인딩
    while (1) { // SIGINT 전달 시 종료
        clientlen = sizeof(clientaddr); // 클라이언트 주소 길이 초기화
        int tempfd;
        tempfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (tempfd < 0) {
            if (errno == EINTR) continue;
            continue;
        }
        connfd = tempfd;
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE,0); // 클라이언트 호스트명과 포트 정보 얻기
        printf("Accepted connection from (%s, %s)\n", hostname, port); // 연결 정보 출력
        doit(connfd);   // 클라이언트 요청 처리 함수 호출
        Close(connfd);  // 연결 소켓 닫기
    }
    Close(listenfd);
    return 0;
}

// 클라이언트 요청을 처리하는 함수
void doit(int fd)
{
    int is_static; // 요청이 정적 콘텐츠인지 여부
    struct stat sbuf; // 파일 상태 정보 구조체
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 요청 라인 저장 버퍼
    char filename[MAXLINE], cgiargs[MAXLINE]; // 파일명과 CGI 인자 저장 버퍼
    rio_t rio; // rio_t: CSAPP RIO 버퍼 타입

    Rio_readinitb(&rio, fd); // RIO 버퍼 초기화
    if (!Rio_readlineb(&rio, buf, MAXLINE)){ // 요청 라인 읽기
        return;
    }
    
    printf("Request headers:\n"); // 요청 헤더 출력 시작
    printf("%s", buf); // 요청 라인 출력
    sscanf(buf, "%s %s %s", method, uri, version); // sscanf: 문자열 포맷 파싱 함수
    if (strcasecmp(method, "GET")){ // GET 메서드가 아니면
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method"); // 501 오류 반환
        return; // 함수 종료
    }
    read_requesthdrs(&rio); // 나머지 요청 헤더 모두 읽기

    // GET 요청으로부터 URI 파싱
    is_static = parse_uri(uri, filename, cgiargs); // URI를 정적/동적 파일명과 인자로 분해
    if (stat(filename, &sbuf) < 0 ){ // 파일 존재 여부 확인
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file"); // 404 오류 반환
        return; // 함수 종료
    }

    if (is_static) { // 정적 콘텐츠 전달
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // &: 권한 비트 검사
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file"); // 403 오류 반환
            return; // 함수 종료
        }
        serve_static(fd, filename, sbuf.st_size); // 정적 파일 전송
    }
    else { // 동적 콘텐츠 전달
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 실행 권한 확인
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program"); // 403 오류 반환
            return; // 함수 종료
        }
        serve_dynamic(fd, filename, cgiargs); // 동적 콘텐츠 전송
    }
}


// HTTP 오류 응답을 생성하고 전송하는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
    char buf[MAXLINE], body[MAXBUF];

    // make HTTP response body
    sprintf(body, "<html><title>Tiny Error</title>"); // 오류 페이지 HTML 시작 생성
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); // 배경색 흰색 설정
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); // 오류 번호와 간단한 메시지 추가
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); // 상세 메시지와 원인 추가
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body); // 하단 서명 추가

    // print the HTTP response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); // 상태 라인 생성
    Rio_writen(fd, buf, strlen(buf)); // 상태 라인 전송
    sprintf(buf, "Content-Type: text/html; charset=utf-8\r\n"); // 콘텐츠 타입 헤더 생성, charset=utf를 통해 UTF-8 인코딩한 HTML임을 인식
    Rio_writen(fd, buf, strlen(buf)); // 콘텐츠 타입 헤더 전송
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // 콘텐츠 길이 헤더 생성
    Rio_writen(fd, buf, strlen(buf)); // 콘텐츠 길이 헤더 전송
    Rio_writen(fd, body, strlen(body)); // 본문 전송
}

// HTTP 요청 헤더를 모두 읽어들이는 함수
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE); // 요청 헤더 한 줄 읽기
    while(strcmp(buf, "\r\n")){ // 빈 줄이 나올 때까지 반복
        Rio_readlineb(rp, buf, MAXLINE); // 다음 헤더 줄 읽기
        printf("%s", buf); // 헤더 내용 출력
    }
    return;
}

// URI를 정적 또는 동적 파일 경로와 CGI 인수로 분해하는 함수
int parse_uri(char *uri, char *filename, char *cgiargs){
    char *ptr;

    if (!strstr(uri, "cgi-bin")) { // 정적 콘텐츠 처리
        strcpy(cgiargs, ""); // CGI 인수 빈 문자열로 초기화
        strcpy(filename, "."); // strcpy: 문자열 복사
        strcat(filename, uri); // strcat: 문자열 이어붙이기
        if (uri[strlen(uri)-1] == '/'){
            strcat(filename, "home.html"); // URI가 디렉터리면 home.html 추가
        }
        return 1;
    }
    else { // 동적 콘텐츠 처리
        ptr = index(uri, '?'); // CGI 인수 구분자 '?' 위치 찾기
        if (ptr) {
            strcpy(cgiargs, ptr+1); // '?' 이후 문자열을 CGI 인수로 복사
            *ptr = '\0'; // URI에서 '?' 이후 문자열 제거
        }    
        else {
            strcpy(cgiargs, ""); // 인수가 없으면 빈 문자열
        }
        strcpy(filename, "."); // strcpy: 문자열 복사
        strcat(filename, uri); // strcat: 문자열 이어붙이기
        return 0;
    }
}

// 정적 파일을 메모리 매핑하여 클라이언트로 전송
void serve_static(int fd, char *filename, int filesize){
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF]; // char*: 메모리 주소(포인터) 저장

    // 클라이언트로 response 헤더 전송
    get_filetype(filename, filetype); // 파일 타입 결정 호출
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 상태 라인 생성
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); // 서버 헤더 추가
    sprintf(buf, "%sConnection: close\r\n", buf); // 연결 종료 헤더 추가
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize); // 콘텐츠 길이 헤더 추가
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // 콘텐츠 타입 헤더 추가 및 헤더 종료
    Rio_writen(fd, buf, strlen(buf)); // 헤더 전송
    printf("Response headers:\n");
    printf("%s", buf);

    // 클라이언트로 response body 전송
    srcfd = Open(filename, O_RDONLY, 0); // 파일 열기
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 메모리에 매핑
    Close(srcfd); // 파일 디스크립터 닫기
    Rio_writen(fd, srcp, filesize); // 매핑한 파일 내용 클라이언트로 전송
    Munmap(srcp, filesize); // 메모리 매핑 해제
}

// 파일 확장자에 따라 MIME 타입 문자열을 설정하는 함수
void get_filetype(char *filename, char *filetype){
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html"); // .html 확장자는 text/html
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif"); // .gif 확장자는 image/gif
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png"); // .png 확장자는 image/png
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg"); // .jpg 확장자는 image/jpeg
    else
        strcpy(filetype, "text/plain"); // 그 외는 text/plain
}

// CGI 프로그램을 실행하여 동적 콘텐츠를 전송
void serve_dynamic(int fd, char *filename, char *cgiargs){
    char buf[MAXLINE], *emptylist[] = { NULL };

    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 상태 라인 생성
    Rio_writen(fd, buf, strlen(buf)); // 상태 라인 전송
    sprintf(buf, "Server: Tiny Web Server\r\n"); // 서버 헤더 생성
    Rio_writen(fd, buf, strlen(buf)); // 서버 헤더 전송

    if (Fork() == 0) { // 자식 프로세스 생성
        setenv("QUERY_STRING", cgiargs, 1); // 환경변수 QUERY_STRING 설정
        Dup2(fd, STDOUT_FILENO); // 표준 출력을 클라이언트 소켓으로 리다이렉트
        Execve(filename, emptylist, environ); // CGI 프로그램 실행
    }
    Wait(NULL); // 자식 프로세스 종료 대기
}