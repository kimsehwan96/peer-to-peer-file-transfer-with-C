#include <stdio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>

// 인자로 들어온 문자열 포인터에 현재 접속한 Linux server PC의 ip 주소 리터럴의 첫번째 주소를 넣어줌
// with strcpy
void myIp(char *buf)
{
    //인자로 들어온 buf의 사이즈가 너무 작을경우 에러 반환 로직 필요.
    struct ifreq ifr;
    char ipstr[40];
    int s;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    //for our kpu linux machine, we use interface name with enp5s0.
    strncpy(ifr.ifr_name, "enp5s0", IFNAMSIZ);

    if (ioctl(s, SIOCGIFADDR, &ifr) < 0)
    {
        printf("Error\n");
        printf("check you network interface.\n");
    }
    else
    {
        inet_ntop(AF_INET, ifr.ifr_addr.sa_data + 2,
                  ipstr, sizeof(struct sockaddr));
        //no return, copy string to the argument's address.
        strcpy(buf, ipstr);
    }
}