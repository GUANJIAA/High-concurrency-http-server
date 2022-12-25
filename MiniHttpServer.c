#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<ctype.h>
#include<arpa/inet.h>

#define SERVER_PORT 80
static int debug = 1;

void do_http_request(int client_sock);
int get_line(int socket,char *buf,int size);

int main(){
	
	int lfd=socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));

	server_addr.sin_family=AF_INET;
	server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	server_addr.sin_port=htons(SERVER_PORT);

	bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

	listen(lfd,128);

	printf("listen...\n");
	
	while(1){
		struct sockaddr_in client;
		int cfd,len,i;
		char client_ip[64];
		char buf[1024];
		
		socklen_t socklen;
		socklen=sizeof(client);
		cfd = accept(lfd,(struct sockaddr*)&client,&socklen);
		
		printf("client ip:%s\t port : %d\n",
				inet_ntop(AF_INET,&client.sin_addr.s_addr,client_ip,sizeof(client_ip)),
				ntohs(client.sin_port));

		do_http_request(cfd);
		close(cfd);
	}
	close(lfd);
	return 0;
}

void do_http_request(int client_sock){
	
	int len=0;
	char buf[256];
	char method[64];
	char url[256];
	char path[256];

	memset(buf,0x00,sizeof(buf));
	len=get_line(client_sock,buf,sizeof(buf));

	if(len>0){
		int i=0,j=0;
		while(!isspace(buf[j])&&i<sizeof(method)-1){
			method[i]=buf[j];
			i++;
			j++;
		}
		method[i]='\0';
		if(debug)
			printf("request method:%s\n",method);	
		if(strncasecmp(method,"GET",i)==0){
			if(debug){
				printf("request = GET\n");
			}	
			
			while(isspace(buf[j++]));
			i=0;
			while(!isspace(buf[j])&&i<sizeof(url)-1){
				url[i]=buf[j];
				i++;
				j++;
			}

			url[i]='\0';

			if(debug){
				printf("url:%s\n",url);
			}
			do{
				len=get_line(client_sock,buf,sizeof(buf));
				if(debug)
					printf("read:%s\n",buf);
			}while(len>0);

			{
				char *pos=strchr(url,'?');
				if(pos){
					*pos='\0';
					printf("real url:%s\n",url);
				}
			}

			sprintf(path,"./html_docs/%s",url);
			if(debug)
				printf("path:%s\n",path);

		}else {
			fprintf(stderr,"warning other request [%s]\n",method);
			do{
				len=get_line(client_sock,buf,sizeof(buf));
				if(debug)
					printf("read:%s\n",buf);
			}while(len>0);
		}
	}else{
		
	}
}

int get_line(int socket,char *buf,int size){
	int count=0;
	char ch='\0';
	int len=0;

	while((count<size-1)&&(ch!='\n')){
		len=read(socket,&ch,1);
		if(len==1){
			if(ch=='\r'){
				continue;
			}
			else if(ch=='\n'){
				break;
			}

			buf[count]=ch;
			count++;
		}else if(len==-1){
			perror("read failed");
			count=-1;
			break;
		}else{
			fprintf(stderr,"client close.\n");
			count=-1;
			break;
		}				
	}
	if(count>=0)
		buf[count]='\0';
	return count;
}
