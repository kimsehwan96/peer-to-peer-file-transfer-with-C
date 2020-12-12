//here will be useful common functions
/*
유저 인증 및 macro 상수를 정의하는 파일 임.
유저의 id, password를 저장하고 있으며,
유저를 구분하기 위한 token 값을 상수로 define 하여 사용함.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define INIT_MSG "===================\n Hi this is p2p test plz login\n ===============\n"
#define ID_REQ "input id : "
#define PW_REQ "input password : "
#define USER1_ID "user"
#define USER1_PW "passwd1"
#define USER2_ID "user2"
#define USER2_PW "passwd2"

#define USER1_LOGIN 1
#define USER2_LOGIN 2
#define LOGIN_FAIL 0
#define BUFSIZE 512
#define USER1_FTP_PORT 4141 //FTP를 위한 포트 번호
#define USER2_FTP_PORT 4142 //FTP를 위한 포트 번호


// 유저의 인증로직을 위한 함수.
unsigned int authenticate(int fd, char *id, char *pw)
{
    send(fd, ID_REQ, strlen(ID_REQ) + 1, 0); //strlen((char *)some_string +1) 인 이유는 Null 포함
    read(fd, id, sizeof(id));
    send(fd, PW_REQ, strlen(PW_REQ) + 1, 0);
    read(fd, pw, sizeof(pw));

    printf("===========================\n");
    printf("User Information\n");
    printf("id : %s  pw : %s \n", id, pw);
    printf("===========================\n");

    if (strcmp(id, USER1_ID) == 0)
    {
        if (strcmp(pw, USER1_PW) == 0)
        {
            printf("%s Login success \n", id);
            return USER1_LOGIN;
        }
        else
        {
            return LOGIN_FAIL;
        }
    }
    else if (strcmp(id, USER2_ID) == 0)
    {
        if (strcmp(pw, USER2_PW) == 0)
        {
            printf("%s Login success \n", id);
            return USER2_LOGIN;
        }
        else
        {
            return LOGIN_FAIL;
        }
    }
    else
        return LOGIN_FAIL;
}

// 파일 포인터를 입력받아서 파일의 사이즈를 리턴하는 함수

int get_file_size(FILE *stream)
{
    int fd, pos;
    if ((fd = fileno(stream)) < 0)
    {
        perror("fileno");
        return -1; //error intger return
    }
    //파일 오프셋을 0으로 설정
    lseek(fd, 0, SEEK_SET);
    //파일 오프셋을 파일의 끝으로 설정하고 크기 받아오기
    pos = lseek(fd, 0, SEEK_END);
    //다시 0으로
    lseek(fd, 0, SEEK_SET);
    return pos;
}

//파일 송신 / 수신 함수

void send_file(FILE *fp, int sockfd)
{
    int n;
    char data[BUFSIZE] = {0};
    int cnt = 0;
    while (fgets(data, BUFSIZE, fp) != NULL)
    {
        cnt++;
        if (send(sockfd, data, BUFSIZE, 0) == -1)
        {
            perror("[-]Error in sending file.");
            exit(1);
        }
        printf("%d : %s  <---- sended \n", cnt, data);
        bzero(data, BUFSIZE);
    }
    cnt++;
    bzero(data, BUFSIZE);
    strncpy(data, "DATSD\0", 6); //DATSD는 전송 완료되었음을 명시하기 위해서 사용
    printf("%d : %s \n", cnt, data);
    //보안에 취약점이 있지만 그냥 쓰자.
    send(sockfd, data, BUFSIZE, 0);
    return;
}

void write_file(int sockfd, char *store_name)
{
    int n;
    FILE *fp;
    char buffer[BUFSIZE];
    int cnt = 0;

    fp = fopen(store_name, "w+");
    printf("will write file as %s \n", store_name);
    while (1)
    {
        cnt++;
        n = read(sockfd, buffer, BUFSIZE);
        if (n <= 0)
        {
            break;
            return;
        }
        printf("%d : %s <----- recved \n", cnt, buffer);
        fprintf(fp, "%s", buffer);
        bzero(buffer, BUFSIZE);
    }
    fclose(fp);
    return;
}

void write_file_to_fd(int sockfd, int fd)
{   sleep(2);
    int n;
    FILE *fp;
    char buffer[BUFSIZE];
    int cnt = 0;
    int cmp_num;

    fp = fdopen(fd, "w");
    if (fp == NULL)
    {
        perror("open");
    }
    while (1)
    {
        cnt++;
        n = recv(sockfd, buffer, BUFSIZE, 0); //마지막에 EOF를 던져주지 않으면 블로킹 당함.
        if (n <= 0)
        { //EOF -> 소켓이 끊어지면 전달된다 (n = 0)
            // n < 0 -> recv 에러.
            break;
            return;
        }
        cmp_num = strncmp(buffer, "DATSD", 5);
        if (cmp_num == 0)
        {
            printf("this is strcmp : %d \n", cmp_num);
            printf("file recv done!\n");
            break;
            return;
        }
        else
        {
            printf("this is strcmp : %d \n", cmp_num);
        }
        printf("%d : %s <----- recv \n", cnt, buffer);
        fprintf(fp, "%s", buffer);
        bzero(buffer, BUFSIZE);
    }
    fclose(fp);
    return;
}

void print_recv_file(int sockfd)
{
    int n;
    char buffer[BUFSIZE];
    while (1)
    {
        n = recv(sockfd, buffer, BUFSIZE, 0);
        if (n <= 0)
        {
            break;
            return;
        }
        if ((strcmp(buffer, "DATSD")) == 0)
        {
            printf("file recv done!\n");
            break;
            return;
        }
        printf("%s", buffer);
        bzero(buffer, BUFSIZE);
    }
    return;
}