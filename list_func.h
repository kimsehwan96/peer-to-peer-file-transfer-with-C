#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#define LIST_FILE_NAME "_file_list.lst"

// for now it just print
// we will make a file with name file_list.lst for server side use.
//with arguments username & ip address

void mklistf(const char *username, const char *ipinfo, int portnum)
{
    FILE *fp;
    char *cwd = (char *)malloc(sizeof(char) * 1024);
    char filename[256];
    DIR *dir = NULL;
    struct dirent *entry = NULL;

    //파일 내용을 채울 임시 버퍼
    char buf[100];
    char portnumc[10];
    //파일 정보 구조체
    struct stat info;
    getcwd(cwd, 1024);
    //for making list information file
    strcpy(filename, username);//user1
    strcat(filename, LIST_FILE_NAME); //user1_file_list.lst
    fp = fopen(filename, "w+");       //with overwriting mode
    if ((dir = opendir(cwd)) == NULL)
    {
        printf("current directory error\n");
        exit(1);
    }
    while ((entry = readdir(dir)) != NULL)

    {
        lstat(entry->d_name, &info);
        if (S_ISREG(info.st_mode)) //파일일때
        {
            printf("FILE : %s\n", entry->d_name);
            strcat(buf, username); // buf -> user1
            strcat(buf, " "); // buf -> user1 
            strcat(buf, ipinfo); // buf -> user1 192.168.0.1
            strcat(buf, " "); //
            sprintf(portnumc, "%d", portnum);
            strcat(buf, portnumc); 
            strcat(buf, " "); //
            strcat(buf, entry->d_name);//user1 192.168.0.1 fileName
            strcat(buf, "\n");
            fputs(buf, fp);
            memset(buf, 0, 100);
        }
        else if (S_ISDIR(info.st_mode)) // 디렉토리(폴더)일때
        {
            printf("DIR : %s\n", entry->d_name);
        }
        else //X
        {
            printf("this is not a file or directory !\n");
        }
    }
    free(cwd);
    closedir(dir);
    fclose(fp);
}