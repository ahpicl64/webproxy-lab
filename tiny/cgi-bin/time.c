#include <stdio.h>
#include <time.h>

int main(void) {
    time_t t = time(NULL);
    char *s = ctime(&t);

    // HTTP 응답 헤더
    printf("Content-Type: text/plain\r\n\r\n");
    // 본문: 현재 시간
    printf("현재 서버 시간은: %s", s);
    return 0;
}