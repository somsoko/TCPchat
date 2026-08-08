#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 100
#define MAX_CLNT 256
#define NAME_SIZE 20

// 닉네임과 소켓을 담는 구조체 선언
typedef struct {
	int socknum;
	char name[NAME_SIZE];
} Clnt_info;

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
void error_handling(char * msg);

int clnt_cnt=0;
Clnt_info clnt_socks[MAX_CLNT];   // 구조체 배열로 바꿈
pthread_mutex_t mutx;

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	pthread_t t_id;
	if(argc!=2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
  
	pthread_mutex_init(&mutx, NULL);
	serv_sock=socket(PF_INET, SOCK_STREAM, 0);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET; 
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1]));
	
	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1)
		error_handling("bind() error");
	if(listen(serv_sock, 5)==-1)
		error_handling("listen() error");
	
	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz);
		
		pthread_mutex_lock(&mutx);
		clnt_socks[clnt_cnt++].socknum = clnt_sock;
		pthread_mutex_unlock(&mutx);
	
		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
		pthread_detach(t_id);
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
	}
	close(serv_sock);
	return 0;
}
	
void * handle_clnt(void * arg)
{
	int clnt_sock=*((int*)arg);
	int str_len=0, i;
	char msg[BUF_SIZE];
	char name[NAME_SIZE];

	str_len = read(clnt_sock, name, sizeof(name));				 // 클라이언트와 연결이 되고 첫 메시지는 클라이언트의 닉네임 받기
	name[str_len] = '\0';

	pthread_mutex_lock(&mutx);
	for (i = 0; i < clnt_cnt; i++) {
		if (clnt_socks[i].socknum == clnt_sock) {
			sprintf(clnt_socks[i].name, "%s", name);			 // 구조체 배열에서 해당 소켓 번호의 닉네임 설정
			write(clnt_sock, "welcome\n", strlen("welcome\n"));  // 환영 인사 보내기
		}
	}
	pthread_mutex_unlock(&mutx);


	while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
		int i;

		if (msg[0] == '@') {									// @로 시작하는 멘션 메시지라면
			char* mention = strtok(msg + 1, " ");				// 멘션 닉네임과 메시지 분리
			char* message = strtok(NULL, "");

			int find = 0;
			pthread_mutex_lock(&mutx);
			for (i = 0; i < clnt_cnt; i++) {					// 멘션 닉네임이 구조체 배열에서 존재하는지 확인
				if (strcmp(clnt_socks[i].name, mention) == 0) {	
					char mention_msg[BUF_SIZE];
					memset(mention_msg, 0, sizeof(mention_msg));
					sprintf(mention_msg, "(mention)[%s] %s", name, message);	
					write(clnt_socks[i].socknum, mention_msg, strlen(mention_msg));		// 존재한다면 그 닉네임의 소켓번호로 멘션 메시지 보내기
					find = 1;
					break;
				}
			}
			pthread_mutex_unlock(&mutx);

			if (!find) {																// 존재하지 않는다면 오류 메시지 전달
				write(clnt_sock, "not found\n", strlen("not found\n"));
			}
		}
		else {													// 멘션 메시지가 아니라면
			char name_msg[BUF_SIZE];
			memset(name_msg, 0, sizeof(name_msg));
			sprintf(name_msg, "[%s] %s", name, msg);

			pthread_mutex_lock(&mutx);
			for (i = 0; i < clnt_cnt; i++)
				write(clnt_socks[i].socknum, name_msg, strlen(name_msg));				// 구조체 배열의 모든 사용자에게 일반 메시지 보내기
			pthread_mutex_unlock(&mutx);
		}
		memset(msg, 0, sizeof(msg));
	}
	

	pthread_mutex_lock(&mutx);
	for(i=0; i<clnt_cnt; i++)   // remove disconnected client
	{
		if(clnt_sock==clnt_socks[i].socknum)
		{
			while(i <clnt_cnt-1)
			{
				clnt_socks[i]=clnt_socks[i+1];
				  i++;

			}

			break;
		}
	}
	clnt_cnt--;
	pthread_mutex_unlock(&mutx);
	close(clnt_sock);
	return NULL;
}

void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}