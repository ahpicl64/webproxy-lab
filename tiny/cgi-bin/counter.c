#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    const char *path = "/tmp/count.txt";
    FILE *fp = fopen(path, "r+");
    if (!fp) {
        fp = fopen(path, "w+");
        if (!fp) { perror("fopen"); return 1; }
    }
    int cnt;
    if (fscanf(fp, "%d", &cnt) != 1) cnt = 0;
    cnt++;
    rewind(fp);
    fprintf(fp, "%d\n", cnt);
    fclose(fp);

    printf("Content-type: text/plain\r\n\r\n");
    printf("당신은 #%d번째 방문자입니다.\n", cnt);
    return 0;
}