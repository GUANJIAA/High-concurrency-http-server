#include<stdio.h>
#include<pthread.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<ctype.h>
#include<arpa/inet.h>
#include<errno.h>
#include<string.h>
#include<sys/stat.h>

#define SERVER_PORT 80
static int debug = 1;

int get_line(int socket,char *buf,int size);
void *do_http_request(void *pclient_sock);
void do_http_response(int client_sock,const char *path);
void do_http_response1(int client_sock);
void not_found(int client_sock);//404
void unimplemented(int client_sock);
void bad_request(int client_sock);
int headers(int client_sock,FILE *resource);
void cat(int client_sock,FILE *resource);
void inner_error(int client_sock);
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
		pthread_t id;
		int *pclient_sock=NULL;

		socklen_t socklen;
		socklen=sizeof(client);
		cfd = accept(lfd,(struct sockaddr*)&client,&socklen);
		
		printf("client ip:%s\t port : %d\n",
				inet_ntop(AF_INET,&client.sin_addr.s_addr,client_ip,sizeof(client_ip)),
				ntohs(client.sin_port));

		//do_http_request(cfd);
		pclient_sock=(int *)malloc(sizeof(int));
		*pclient_sock=cfd;
		pthread_create(&id,NULL,do_http_request,(void *)pclient_sock);
	}
	close(lfd);
	return 0;
}

void *do_http_request(void *pclient_sock){
	
	int len=0;
	char buf[256];
	char method[64];
	char url[256];
	char path[256];
	int client_sock=*(int *)pclient_sock;

	struct stat st;
	memset(buf,0x00,sizeof(buf));
	len=get_line(client_sock,buf,sizeof(buf));

	if(len>0){
		int i=0,j=0;
		while(!isspace(buf[j])&&(i<sizeof(method)-1)){
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
			while(!isspace(buf[j])&&(i<sizeof(url)-1)){
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
			
			if(stat(path,&st)==-1){
				fprintf(stderr,"stat %s failed.reason:\n",path);
				not_found(client_sock);
			}else {
				if(S_ISDIR(st.st_mode)){
					strcat(path,"/index.html");
				}

				do_http_response(client_sock,path);	
			}
		}else {
			fprintf(stderr,"warning other request [%s]\n",method);
			do{
				len=get_line(client_sock,buf,sizeof(buf));
				if(debug)
					printf("read:%s\n",buf);
			}while(len>0);
			unimplemented(client_sock);			
		}
	}else{
		bad_request(client_sock);		
	}
	close(client_sock);
	if(pclient_sock)
		free(pclient_sock);
	return NULL;
}

void do_http_response(int client_sock,const char *path){
	int ret=0;
	FILE *resource=NULL;

	resource=fopen(path,"r");

	if(resource==NULL){
		not_found(client_sock);
		return ;
	}

	ret=headers(client_sock,resource);
	
	if(!ret)
		cat(client_sock,resource);

	fclose(resource);
}
int headers(int client_sock,FILE *resource){
	struct stat st;
	int fileid=0;
	char tmp[64];
	char buf[1024]={0};
	strcpy(buf,"HTTP/1.0 200 OK\r\nServer: Martin Server\r\nContent-Type: text/html\r\nConnection:Close\r\n");

	fileid=fileno(resource);

	if(fstat(fileid,&st)==-1){
		inner_error(client_sock);
		return -1;
	}

	snprintf(tmp,64,"Content_Length:%ld\r\n\r\n",st.st_size);
	strcat(buf,tmp);
	if(debug)
		fprintf(stdout,"header:%s\n",buf);

	if(send(client_sock,buf,strlen(buf),0)<0){
		fprintf(stderr,"send failed.data:%s,reason:\n",buf);
		return -1;
	}
	return 0;
}
void cat(int client_sock,FILE *resource){
	char buf[1024];

	fgets(buf,sizeof(buf),resource);

	while(!feof(resource)){
		int len = write(client_sock,buf,strlen(buf));
		if(len<0){
			fprintf(stderr,"send body error. reason:%s\n",strerror(errno));
			break;
		}
		if(debug)
			fprintf(stdout,"%s",buf);
		fgets(buf,sizeof(buf),resource);
	}
}

void do_http_response1(int client_sock){
	const char * main_header = "HTTP/1.0 200 OK\r\nServer: Martin Server\r\nContent-Type: text/html\r\nConnection: Close\r\n";
	const char * welcome_content = "\
<html lang=\"zh-CN\">\n\
<head>\n\
<meta content=\"text/html;charset=utf-8\"http-equiv=\"Content-Type\">\n\
<title>This is a test</title>\n\
</head>\n\
<body>\n\
<div align=center height=\"500px\">\n\
<br/><br/><br/>\n\
<h2>GUANJIAA!</h2><br/><br/>\n\
<from action=\"commit\"method=\"post\">\n\
name:<input type=\"text\" name=\"name\"/>\n\
<br/>age:<input type=\"password\" name=\"age\"/>\n\
<br/><br/><br/><input type=\"submit\" value=\"submit\"/>\n\
<input type=\"reset\" value=\"reset\"/>\n\
</form>\n\
</div>\n\
</body>\n\
</html>\n\
";
	
	int len=write(client_sock,main_header,strlen(main_header));

	if(debug)
		fprintf(stdout,"...do_http_response...\n");
	if(debug)
		fprintf(stdout,"write[%d]:%s",len,main_header);

	char send_buf[64];
	int wc_len=strlen(welcome_content);
	len = snprintf(send_buf,64,"Content-Length:%d\r\n\r\n",wc_len);
	len =write(client_sock,send_buf,len);

	if(debug)
		fprintf(stdout,"write[%d]:%s",len,send_buf);

	len = write(client_sock,welcome_content,wc_len);
	
	if(debug)
		fprintf(stdout,"write[%d]:%s",len,welcome_content);

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

void not_found(int client_sock){
	const char * reply = "HTTP/1.0 404 NOT FOUND\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html;charset=utf-8\"http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>NOT FOUND</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>文件不存在！！！\r\n\
	<P>The server could not fulfill your request becase the resource specified is unavailable or nonexitent.\r\n\
</BODY>\r\n\
</HTML>";

	int len=write(client_sock,reply,strlen(reply));
	if(debug)
		fprintf(stdout,reply);
	if(len<=0)
		fprintf(stderr,"send reply failed.reason:%s\n",strerror(errno));

}

void inner_error(int client_sock){
	const char *reply="HTTP/1.0 500 Internal Server Error\r\n\
Content_Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html;charset=utf-8\"http-equiv=\"Content-Type\">\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>Inner Error</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>Server internal error.\r\n\
</BODY>\r\n\
</HTML>";
	int len=write(client_sock,reply,strlen(reply));
	if(debug)
		fprintf(stdout,reply);
	if(len<=0){
		fprintf(stderr,"send reply failed.reason:%s\n",strerror(errno));
	}
}

void unimplemented(int client_sock){
	const char * reply = "HTTP/1.0 501 Method Implemented\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>Method Not Implemented</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>HTTP REQUEST METHOD NOT SUPPORTED.\r\n\
</BODY>\r\n\
</HTML>";

	int len=write(client_sock,reply,strlen(reply));
	if(debug)
		fprintf(stdout,reply);
	if(len<=0){
		fprintf(stderr,"send reply failed.reason:%s\n",strerror(errno));
	}
}

void bad_request(int client_sock){
	const char * reply = "HTTP/1.0 400 BAD REQUEST\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>BAD REQUEST</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>your browser sent a bad request! \r\n\
</BODY>\r\n\
</HTML>";
	int len=write(client_sock,reply,strlen(reply));
	if(debug)
		fprintf(stdout,reply);
	if(len<=0){
		fprintf(stderr,"send reply failed.reason:%s\n",strerror(errno));
	}
}
