/*
한국산업기술대학교 임베디드 시스템과 2015146007 김세환
임베디드 운영체제 과목 P2P Server Client Implementaion Source Code
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h> //open, close 등 파일 디스크립터 사용을 위함
#include <errno.h>
#include "common.h"    //유저 인증 로직 및 macro 상수
#include "list_func.h" //내부 파일 리스트를 생성하는 함수
#include "my_ip.h"     //ip check 함수

#define SERV_IP "220.149.128.100"
#define SERV_PORT 4140
#define INIT_STATE 0
#define AFTER_STATE 1
#define SEG 1

struct p2p_file
{
    int idx;
    char user_name[20];
    char ip[40];
    int port;
    char file_name[50];
};
//위는 스레드에게 넘겨주면 되겠당.

extern int errno;
//p2p를 위한 데이터 송수신은 thread로 구현해보기

int setup_socket(int port);
int setup_socket_connect(char *ip, int port);
//서버와 인증 로직을 수행하기 위한 read, send, scanf 등의 함수를 하나의 flow로 함수화함
void auth_request(int fd, char *id, char *pw, char *buf)
{
    read(fd, buf, BUFSIZE);
    printf("%s", buf);
    scanf("%s", id);
    send(fd, id, strlen(id) + 1, 0);
    memset(buf, 0, BUFSIZE);
    read(fd, buf, BUFSIZE);
    printf("\n%s", buf);
    scanf("%s", pw);
    send(fd, pw, strlen(pw) + 1, 0);
    memset(buf, 0, BUFSIZE);
}

int main(void)
{
    int sockfd, fd, p2p_fd, p2p_new_fd;
    int p2p_req_fd;
    int p2p_recv_file_fd, p2p_send_file_fd;
    int rcv_byte, file_size;
    int token = 0;
    int req_file_idx;
    char my_ip[BUFSIZE];
    unsigned int sin_size;
    struct sockaddr_in dest_addr;  //서버의 어드레스
    struct sockaddr_in my_addr;    //클라이언트가 p2p 서버로 동작하기 위한 addr
    struct sockaddr_in their_addr; //p2p 요청으로 들어온 상대 유저의 ip 어드레스
    char *buf = (char *)malloc(BUFSIZE);
    char *msg = (char *)malloc(BUFSIZE);
    char *file_buf = (char *)malloc(SEG);
    char file_name[512];
    char id[20];
    char pw[20];
    int state = INIT_STATE;
    int len = BUFSIZE;

    int p2p_port;
    char p2p_ip[512];
    char p2p_file_name[512];

    FILE *write_sock_fp;
    FILE *read_sock_fp;
    pid_t childpid; //p2p

    sockfd = socket(AF_INET, SOCK_STREAM, 0); //socket fd

    if (sockfd == -1)
    {
        perror("socket");
        exit(1);
    }
    else
    {
        printf("socket is ok \n");
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERV_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(SERV_IP);
    memset(&(dest_addr.sin_zero), 0, 8);
    if (connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("connect");
        exit(1);
    }
    else
    {
        printf("connect ok\n");
    }
    auth_request(sockfd, id, pw, buf); //if user 1 success, we will get 1
    read(sockfd, buf, BUFSIZE);
    printf("this is token %d\n", *(int *)&buf[0]);
    token = *(int *)&buf[0];
    if (token == 1)
    { //유저 1 로그인 성공
        printf("login success user%d\n", token);
        myIp(my_ip);
        mklistf("user1", my_ip, USER1_FTP_PORT); //뒤에 인자(ip address)는 my_ip 헤더를 이용할 것.
        strcpy(file_name, "user1_file_list.lst");
        if ((fd = open("user1_file_list.lst", O_RDWR)) < 0)
        {
            perror("open() error !");
        }
        send(sockfd, file_name, BUFSIZE, 0);
        FILE *user_list_fp;
        user_list_fp = fdopen(fd, "r+");
        send_file(user_list_fp, sockfd);
        printf("sending file done ! \n");

        close(fd); // 유저의 최초 파일 전송 이후 child process fork.
    }
    else if (token == 2)
    {
        printf("login success user%d\n", token);
        myIp(my_ip);
        mklistf("user2", my_ip, USER2_FTP_PORT); //뒤에 인자(ip address)는 my_ip 헤더를 이용할 것.
        strcpy(file_name, "user2_file_list.lst");
        if ((fd = open("user2_file_list.lst", O_RDWR)) < 0)
        {
            perror("open() error !");
        }
        send(sockfd, file_name, BUFSIZE, 0);
        FILE *user_list_fp;
        user_list_fp = fdopen(fd, "r+");
        send_file(user_list_fp, sockfd);
        printf("sending file done ! \n");

        close(fd);
    }
    else
    {
        printf("login failed.");
        exit(1);
    }
    /*          클라이언트의 p2p 파일공유를 위한 서버 모드 셋업     */

    //after init logic, we fork child process for p2p file tranfer service.
    //child process..p2p Listen mode.
    char *p2p_ip_address = (char *)malloc(512);
    // my_ip(my_ip_address); //이 클라이언트 코드가 돌아가는 ip주소 실제 배포시 사용
    strcpy(p2p_ip_address, my_ip);
    if (token == USER1_LOGIN)
    {
        p2p_fd = setup_socket(USER1_FTP_PORT); //소켓 생성
        printf("this is main process's p2p server mode sockfd : %d\n", p2p_fd);
        printf("user 1 has p2p server sub-process %s : %d\n", p2p_ip_address, USER1_FTP_PORT);
    }
    else if (token == USER2_LOGIN)
    {
        p2p_fd = setup_socket(USER2_FTP_PORT); //소켓 생성
        printf("this is main process's p2p server mode sockfd : %d\n", p2p_fd);
        printf("user 2 has p2p server sub-process %s : %d\n", p2p_ip_address, USER2_FTP_PORT);
    }
    else
    {
        printf("wrong process...\n");
        return 1; //child process done with exit code 1
        //애초에 로그인 실패하면 여기까지 오지도 않는다.
    }
    sin_size = sizeof(struct sockaddr_in);

    if ((childpid = fork()) == 0)
    {
        char p2p_buf[BUFSIZE];
        char tt_buf[BUFSIZE];
        char will_recv_file_name[BUFSIZE];
        FILE *p2p_send_file;
        for (;;)
        {   printf("this is accpeted new p2p server mode on....sockfd %d\n", p2p_fd);
            p2p_new_fd = accept(p2p_fd, (struct sockaddr *)&their_addr, &sin_size);
            if (p2p_new_fd < 0)
            {
                perror("bind error");
                return 0;
            }
            printf("Connection accept from %s\n", inet_ntoa(their_addr.sin_addr));
            for (;;)
            {
                //p2p main logic
                read(p2p_new_fd, p2p_buf, BUFSIZE); //filename읽음.
                strcpy(will_recv_file_name, p2p_buf);
                printf("I'll send this file : %s to %s \n", p2p_buf, inet_ntoa(their_addr.sin_addr));
                getcwd(tt_buf, BUFSIZE);
                printf("sending in p2p mode current work directory : %s \n", tt_buf);
                p2p_send_file_fd = open(p2p_buf, O_RDWR);
                p2p_send_file = fdopen(p2p_send_file_fd, "r+");
                send_file(p2p_send_file, p2p_new_fd);
                break;
            }
        }
        close(p2p_new_fd);
        close(p2p_fd);
        printf("p2p sock cloese \n");
        return 0;
    }
    /*          클라이언트의 p2p 파일공유를 위한 서버 모드 셋업 끝. listen 상태이며 accept 가능.    */
    /*          상대 클라이언트로부터 파일명을 받아서 fp열고, 스레드 호출해서 데이터 다 받고 connection 끊어내기 */

    for (;;)
    {
        printf("\nplz input your command user%d :", token);
        scanf("%s", buf);
        if ((send(sockfd, buf, BUFSIZE, 0)) == -1)
        {
            perror("send error ! ");
        }
        printf("send result : %s \n", buf);
        if ((read(sockfd, buf, BUFSIZE)) > 0)
        {
            printf("this msg received from server : %s \n", buf);
        }

        if (strcmp(buf, "exit") == 0)
        {
            printf("connect done !");
            break;
        }

        else if (strcmp(buf, "list") == 0)
        {
            fputs("wait", stdout);
            for (int i = 0; i < 50; i++)
            {
                fputs(".", stdout);
                fflush(stdout);
                usleep(10000);
            }
            fputs("\n", stdout);
            print_recv_file(sockfd);
            printf("recv done !~!");
        }
        else if (strcmp(buf, "data") == 0)
        {
            printf("which file do you want ? : ");
            scanf("%d", &req_file_idx);
            bzero(buf, BUFSIZE);
            *(int *)&buf[0] = req_file_idx;
            send(sockfd, buf, BUFSIZE, 0); //send file no.
            //여기서 서버로부터 ip, address, 파일 이름에 대한 정보를 수신하고
            //그 정보를 토대로 socket 연결을 해당 ip, address, port에 대해 connect & file recv & close 하면 끝
            read(sockfd, buf, BUFSIZE);
            strcpy(p2p_file_name, buf);

            read(sockfd, buf, BUFSIZE);
            strcpy(p2p_ip, buf);

            read(sockfd, buf, BUFSIZE);
            p2p_port = *(int *)&buf[0];

            printf("received %s %s %d \n",p2p_file_name, p2p_ip, p2p_port);

            p2p_req_fd = setup_socket_connect(p2p_ip, p2p_port);
            //connect 된 상태다. 나는 상대방 소켓으로부터 데이터를 받아야 한다.
            //먼저 받고싶은 파일의 이름을 보내주고, 그 파일을 받아버리자.
            strcpy(buf, p2p_file_name);
            printf("Requested this file : %s \n", buf);
            send(p2p_req_fd, buf, BUFSIZE, 0); //파일 이름 전송
            strcat(p2p_file_name, ".got");
            p2p_recv_file_fd = open(p2p_file_name, O_CREAT | O_RDWR | O_TRUNC, 0644); //읽기/쓰기/생성 및 0644            
            write_file_to_fd(p2p_req_fd, p2p_recv_file_fd);
            printf("recv requested file done. \n");
            close(p2p_req_fd); //p2p connection close.
            close(p2p_recv_file_fd);

        }
        else
        {
            printf("%s\n", buf);
            printf("no meaning...\n");
        }
    }
    close(sockfd);
    free(buf);

    return 0;
}

int setup_socket(int port)
{   //client가 p2p 서버로 동작하기 위해 셋업하는 함수
    //클라이언트의 자식 프로세스는 ANY ADDRESS로 listen 상태임.
    int e;
    int sockfd;
    int val = 1;
    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(1);
    }
    printf("[+]Server socket created successfully.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    memset(&(server_addr.sin_zero), 0, 8);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val)) < 0)
    {
        perror("setsockopt");
        close(sockfd);
        exit(1);
    }

    e = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (e < 0)
    {
        perror("bind");
        exit(1);
    }
    printf("[+]Binding successfull in sub process.....\n");

    if (listen(sockfd, 10) == -1)
    {
        perror("listen");
        exit(1);
    }
    else
    {
        printf("listen ok \n");
    }
    printf("returing p2p server sock fd %d \n", sockfd);
    return sockfd;
}


int setup_socket_connect(char *ip, int port)
{   
    int e;
    int sockfd;
    int val = 1;
    struct sockaddr_in target_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(1);
    }
    printf("[+]P2P Client socket created successfully.\n");

    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    target_addr.sin_addr.s_addr = inet_addr(ip);

    printf("attemp to connect %s : %d\n",ip, port);

    memset(&(target_addr.sin_zero), 0, 8);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val)) < 0)
    {
        perror("setsockopt");
        close(sockfd);
        exit(1);
    }
    if (connect(sockfd, (struct sockaddr *)&target_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("connect");
        exit(1);
    }
    else
    {
        printf("connect ok\n");
    }
    printf("returing p2p client sock fd %d \n", sockfd);
    return sockfd;
}