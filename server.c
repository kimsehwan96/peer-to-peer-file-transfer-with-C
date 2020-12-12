/*
한국산업기술대학교 임베디드 시스템과 2015146007 김세환
임베디드 운영체제 과목 P2P Server Client Implementaion Source Code
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>  //for posix thread
#include <signal.h>   //for signal handler.
#include <fcntl.h>    //for file handling
#include <sys/stat.h> //for file status
#include <dirent.h>
//for custom functions.
#include "common.h"    //macro 상수 define
#include "my_ip.h"     // 불필요.
#include "list_func.h" //실행파일이 수행중인 디렉터리의 파일 목록을 파일로 만드는 함수 구현되어있음

#include <errno.h>

//macro
#define SERV_IP "127.0.0.1" // 서버의 로컬 호스트 주소를 define
#define SERV_PORT 4140      //서버의 포트 번호를 define
#define BACKLOG 10
#define INIT_STATE 0
#define AFTER_STATE 1

struct p2p_file
{
    int idx;
    char user_name[20];
    char ip[40];
    int port;
    char file_name[50];
};

int make_tmp_file(int token);
void parse_file_info(struct p2p_file *file_info, int idx, FILE *stream); //file_info에 해당 idx에 해당하는 정보를을 넘겨줄 함수임.

extern int errno;

int main()
{
    int sockfd, new_fd, ftp_fd; //server 호스트의 소켓 파일디스크립터 및 새로운 연결을 정의할 new_fd
    int file_size;              //파일의 사이즈, 미사용중
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    unsigned int sin_size;
    char file_name[BUFSIZE];                           //파일의 이름 -> 서버에서 유저별로 관리하기 위함
    char server_file_path[BUFSIZE] = "./data/server_"; //각 유저별로 서버에 파일을 따로 저장함.
    //서버 코드가 돌아가는 디렉터리의 하위 폴더인 data 밑에 저장함

    //for server concurrency, we will fork server process with each connection request.
    pid_t childpid;
    int rcv_byte;
    int recv_idx;
    char *buf = (char *)malloc(BUFSIZE);
    char *tmp_file_path = (char *)malloc(BUFSIZE);
    char id[20];
    char pw[20];
    char msg[512];
    int rb = 0;
    int len = 65536;

    int val = 1;
    int state = INIT_STATE;

    struct p2p_file *file_info = (struct p2p_file *)malloc(sizeof(struct p2p_file));
    FILE *p2p_stream;

    //socket TCP file descirptor
    //check sockfd condition
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("serverf-socket() error occured");
        exit(1);
    }
    else
    {
        printf("server socket() sockfd is ok \n");
    }

    my_addr.sin_family = AF_INET; //address family is AF_INET

    my_addr.sin_port = htons(SERV_PORT);

    my_addr.sin_addr.s_addr = INADDR_ANY; //any address can accept

    memset(&(my_addr.sin_zero), 0, 8);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val)) < 0)
    {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == 1)
    {
        perror("bind error ");
        exit(1);
    }
    else
    {
        printf("server bind ok \n");
    }
    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("Listen error");
        exit(1);
    }
    else
    {
        printf("listen ok \n\n");
    }

    memset(id, 0, sizeof(id));
    memset(pw, 0, sizeof(pw));

    sin_size = sizeof(struct sockaddr_in);

    for (;;)
    {
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

        if (new_fd < 0)
        {
            perror("bind error ");
            exit(1);
        }
        printf("Connection accept from %s\n", inet_ntoa(their_addr.sin_addr));

        if ((childpid = fork()) == 0)
        { //child process
            //close(sockfd); //더이상 필요하지 않을때 (프로그램 서버측 종료시에) 종료하는게 맞음
            for (;;)
            {
                printf("accept ok \n");
                int token = 0;
                token = authenticate(new_fd, id, pw);
                if (token == USER1_LOGIN)
                {
                    memset(buf, 0, BUFSIZE);
                    *(int *)&buf[0] = 1;
                    send(new_fd, buf, sizeof(buf), 0);
                    read(new_fd, file_name, BUFSIZE);
                    printf("client will sent this file %s \n", file_name);
                    strcat(server_file_path, file_name);                           //server_user1_file_list.lst 라는 이름으로 파일 관리.
                    int fd = 0;                                                    //fd를 열어놓음
                    fd = open(server_file_path, O_CREAT | O_RDWR | O_TRUNC, 0644); //읽기쓰기 및 덮어쓰기, 실행권한
                    write_file_to_fd(new_fd, fd);                                  //파일 수신

                    for (;;)
                    {
                        printf("waiting for client's command \n");
                        bzero(buf, BUFSIZE);
                        read(new_fd, buf, BUFSIZE);
                        printf("client send this command %s \n", buf);

                        if (strcmp("exit", buf) == 0)
                        {
                            printf("client conenct close !\n");
                            strcpy(buf, "exit");
                            send(new_fd, buf, BUFSIZE, 0);
                            break;
                        }

                        else if (strcmp("show", buf) == 0)
                        {
                            int rd_bytes = 0;
                            printf("user%d requested file list ... \n", token);
                            if (make_tmp_file(token) != 1)
                            {
                                printf("error occured on server.\n");
                                send(new_fd, "exit", BUFSIZE, 0); //for exit client
                            }
                            bzero(buf, BUFSIZE);
                            strcpy(buf, "list"); //list 파일을 보내줄것이라고 이야기 해주기.
                            send(new_fd, buf, BUFSIZE, 0);
                            bzero(tmp_file_path, BUFSIZE);
                            getcwd(tmp_file_path, BUFSIZE);
                            strcat(tmp_file_path, "/tmp/");
                            strcat(tmp_file_path, "user1_tmp.lst");
                            printf("tmp_file path : %s \n", tmp_file_path);
                            FILE *server_list_file = fopen(tmp_file_path, "r");
                            if (server_list_file == NULL)
                            {
                                printf("error occured on FP");
                            }
                            printf("will send file...! \n");
                            send_file(server_list_file, new_fd);
                            printf("server send done !! \n");
                            fclose(server_list_file);
                        }
                        else if (strcmp("hello\0", buf) == 0)
                        {
                            printf("got hello from client \n");
                            strcpy(buf, "hello !!");
                            send(new_fd, buf, BUFSIZE, 0);
                        }

                        else if (strcmp("FTP", buf) == 0)
                        {
                            //FTP를 위한 모드 진입
                            strcpy(buf, "data");
                            send(new_fd, buf, BUFSIZE, 0);
                            recv(new_fd, buf, BUFSIZE, 0);
                            recv_idx = *(int *)&buf[0];
                            //여기서 tmp 파일 인덱스를 찾아서. 어떤 유저, 어떤 아이디, 어떤 포트, 어떤 파일인지 갖는다.
                            p2p_stream = fopen("tmp/user1_tmp.lst", "r+");
                            parse_file_info(file_info, recv_idx, p2p_stream);
                            printf("result ... \n");
                            printf("user : %s\n", file_info->user_name);
                            printf("port : %d \n", file_info->port);
                            printf("file name : %s\n", file_info->file_name);
                            printf("ip : %s \n", file_info->ip);
                            //어떤 파일을 읽을건지 받아온다.
                            //tmp 라는 파일에 갖고있다 파일목록쓰
                            //port와 ip, file_name을 전달해준다.
                            strcpy(buf, file_info->file_name);
                            send(new_fd, buf, BUFSIZE, 0);
                            bzero(buf, BUFSIZE);

                            strcpy(buf, file_info->ip);
                            send(new_fd, buf, BUFSIZE, 0);
                            bzero(buf, BUFSIZE);

                            *(int *)&buf[0] = file_info->port;
                            send(new_fd, buf, BUFSIZE, 0);
                            bzero(buf, BUFSIZE);
                        }

                        else
                        {
                            printf("thre is no command like that ! : %s \n", buf);
                            strcpy(buf, "exit");
                            send(new_fd, buf, BUFSIZE, 0);
                            break;
                        }
                    }
                    close(new_fd);
                    break;
                }
                else if (token == USER2_LOGIN)
                {
                    bzero(buf, BUFSIZE);
                    *(int *)&buf[0] = 2;
                    send(new_fd, buf, sizeof(buf), 0);
                    read(new_fd, file_name, BUFSIZE);
                    printf("client will sent this file %s \n", file_name);
                    strcat(server_file_path, file_name);                           //server_user1_file_list.lst 라는 이름으로 파일 관리.
                    int fd = 0;                                                    //fd를 열어놓음
                    fd = open(server_file_path, O_CREAT | O_RDWR | O_TRUNC, 0644); //읽기쓰기 및 덮어쓰기, 실행권한
                    write_file_to_fd(new_fd, fd);                                  //파일 수신
                    for (;;)
                    {
                        printf("waiting for client's command \n");
                        bzero(buf, BUFSIZE);
                        read(new_fd, buf, BUFSIZE);
                        printf("client send this command %s and length %d \n", buf, rb);

                        if (strcmp("exit", buf) == 0)
                        {
                            printf("client conenct close !\n");
                            strcpy(buf, "exit");
                            send(new_fd, buf, BUFSIZE, 0);
                            break;
                        }

                        else if (strcmp("show", buf) == 0)
                        {
                            int rd_bytes = 0;
                            printf("user%d requested file list ... \n", token);
                            if (make_tmp_file(token) != 1)
                            {
                                printf("error occured on server.");
                                send(new_fd, "exit", BUFSIZE, 0); //for exit client
                            }
                            bzero(buf, BUFSIZE);
                            strcpy(buf, "list"); //list 파일을 보내줄것이라고 이야기 해주기.
                            send(new_fd, buf, BUFSIZE, 0);
                            bzero(buf, BUFSIZE);
                            getcwd(tmp_file_path, BUFSIZE);
                            strcat(tmp_file_path, "/tmp/");
                            strcat(tmp_file_path, "user2_tmp.lst");
                            FILE *server_list_file = fopen(tmp_file_path, "r");
                            send_file(server_list_file, new_fd);
                            printf("server send done !! \n");
                            fclose(server_list_file);
                            continue;
                        }

                        else if (strcmp("FTP", buf) == 0)
                        {
                            //FTP를 위한 모드 진입
                            strcpy(buf, "data");
                            send(new_fd, buf, BUFSIZE, 0);
                            recv(new_fd, buf, BUFSIZE, 0);
                            recv_idx = *(int *)&buf[0];
                            //여기서 tmp 파일 인덱스를 찾아서. 어떤 유저, 어떤 아이디, 어떤 포트, 어떤 파일인지 갖는다.
                            p2p_stream = fopen("tmp/user2_tmp.lst", "r+");
                            parse_file_info(file_info, recv_idx, p2p_stream);
                            printf("result ... \n");
                            printf("user : %s\n", file_info->user_name);
                            printf("port : %d \n", file_info->port);
                            printf("file name : %s\n", file_info->file_name);
                            printf("ip : %s \n", file_info->ip);
                            //어떤 파일을 읽을건지 받아온다.
                            //tmp 라는 파일에 갖고있다 파일목록쓰
                            //port와 ip, file_name을 전달해준다.
                            strcpy(buf, file_info->file_name);
                            send(new_fd, buf, BUFSIZE, 0);
                            bzero(buf, BUFSIZE);

                            strcpy(buf, file_info->ip);
                            send(new_fd, buf, BUFSIZE, 0);
                            bzero(buf, BUFSIZE);

                            *(int *)&buf[0] = file_info->port;
                            send(new_fd, buf, BUFSIZE, 0);
                            bzero(buf, BUFSIZE);
                        }
                        else if (strcmp("hello", buf) == 0)
                        {
                            printf("got hello from client \n");
                            bzero(buf, BUFSIZE);
                            strcpy(buf, "hello !!");
                            send(new_fd, buf, BUFSIZE, 0);
                            continue;
                        }

                        else
                        {
                            printf("thre is no command like that ! : %s \n", buf);
                            strcpy(buf, "exit");
                            send(new_fd, buf, BUFSIZE, 0);
                            break;
                        }
                    }
                    close(new_fd);
                    break;
                }
                else
                {
                    printf("there is no such that inofrmation id : %s pw : %s\n", id, pw);
                    send(new_fd, "LOGIN FAIL \n", 512, 0);
                    printf("Disconnected from %s\n", inet_ntoa(their_addr.sin_addr));
                    close(new_fd);
                    break;
                }
            }
        }
    }
    close(new_fd);
    close(sockfd);
}

int make_tmp_file(int token)
{
    int n_bytes;
    int idx = 0;
    int tmp_fd;   //순회하면서 돌 fd
    FILE *tmp_fp; //fd를 file pointer로 열어버리기.
    int fd;
    FILE *fp;
    char user_token[10];
    char buf[BUFSIZE]; //유용하게 사용할 버퍼
    char *path = (char *)malloc(BUFSIZE);
    char *tmp_file_path = (char *)malloc(BUFSIZE);
    char static_path[512];
    memset(path, 0x00, BUFSIZE);
    char *tmp_file_name = (char *)malloc(BUFSIZE);
    memset(tmp_file_name, 0x00, BUFSIZE);
    DIR *dir = NULL;
    struct dirent *entry = NULL; //디럭터리에서 파일만 찾을려고 함
    struct stat info;            //파일의 속성을 파악하기 위한 구조체

    sprintf(user_token, "%d", token); //int형 user token을 문자열로 변환
    strcpy(buf, "user");              //user
    strcat(buf, user_token);          //user1
    strcat(buf, "_tmp.lst");          //user1_tmp.lst
    strcpy(tmp_file_name, buf);       //tmp_file_name에 user1_tmp.lst 문자열 저장
    //tmp_file_name == "user1_tmp.lst"
    //최초 실행 이후 원래 작업 디렉토리로 다시 이동해줘야 함.
    getcwd(buf, BUFSIZE); //서버코드가 실행중인 경로를 얻음.
    strcpy(static_path, buf);
    // /Users/gimsehwan/Desktop/ingkle/studying_C/2020_fall_embeded_os/assignmnet_2
    strcpy(tmp_file_path, buf);
    strcat(tmp_file_path, "/tmp"); // 하위 디렉터리 tmp
    // strcat(buf, "/data"); // 하위 디렉터리 /data 의 절대경로를 얻음
    // printf("this is cat buf + /data :: %s \n", buf);
    // // buf == "/Users/gimsehwan/Desktop/ingkle/studying_C/2020_fall_embeded_os/assignmnet_2/data"
    // // strcat(buf, tmp_file_name);
    // strcpy(path, buf); //path에 buf에 담긴 경로 문자열 담아놓음
    strcat(tmp_file_path, "/");
    strcat(tmp_file_path, tmp_file_name);
    fd = open(tmp_file_path,
              O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("open error fd");
        printf("this is path : %s \n", path);
        return -1;
    }
    fp = fdopen(fd, "w");
    //다시 ./data 경로를 지정하기 위해서 문자열 처리
    getcwd(buf, BUFSIZE); //서버코드가 실행중인 경로를 얻음.
    strcat(buf, "/data"); // 하위 디렉터리 /data 의 절대경로를 얻음
    // strcat(buf, tmp_file_name);
    strcpy(path, buf); //path에 buf에 담긴 경로 문자열 담아놓음
    // /Users/gimsehwan/Desktop/ingkle/studying_C/2020_fall_embeded_os/assignmnet_2/data
    if ((dir = opendir(path)) == NULL)
    {
        printf("open dir error \n");
    }

    chdir(path);
    while ((entry = readdir(dir)) != NULL)
    {
        lstat(entry->d_name, &info);
        if (S_ISREG(info.st_mode))
        {
            if ((strcmp(entry->d_name, tmp_file_name)) == 0)
            {
                continue; //임시파일 자체 이름은 패스 TODO: 다만 여러 유저의 임시파일이 있을 수 있다.
                          //디렉터리를 분리하는 편이 나아보임.
            }

            if ((tmp_fd = open(entry->d_name, O_RDWR)) < 0)
            {
                perror("open error tmp_fd");
            }

            tmp_fp = fdopen(tmp_fd, "r+");
            while (fgets(buf, BUFSIZE, tmp_fp) != NULL)
            {
                fprintf(fp, "%d : %s", idx, buf);
                fflush(fp);
                idx++;
            }
            //각 파일의 맨 앞에 인덱스를 붙여주는 로직
            fclose(tmp_fp);
            close(tmp_fd);
        }
    }
    printf("making file done...\n");
    free(path);
    free(tmp_file_name);
    free(tmp_file_path);
    closedir(dir);
    lseek(fd, 0, SEEK_SET); //파일 오프셋을 맨 앞으로 땡겨놓기
    close(fd);
    close(tmp_fd);
    chdir(static_path);
    return 1;
}

//입력받은 구조체 포인터에 체크한 값을 돌려준다.
void parse_file_info(struct p2p_file *file_info, int find_idx, FILE *stream)
{
    int idx;
    char user_name[20];
    char ip[40];
    int port;
    char file_name[50];
    while (fscanf(stream, "%d : %s %s %d %s", &idx, user_name, ip, &port, file_name) > 0)
    {
        if (find_idx == idx)
        {
            break;
        }
        else
        {
            continue;
        }
    }
    file_info->idx = idx;
    file_info->port = port;
    strcpy(file_info->user_name, user_name);
    strcpy(file_info->ip, ip);
    strcpy(file_info->file_name, file_name);

    fclose(stream);
}