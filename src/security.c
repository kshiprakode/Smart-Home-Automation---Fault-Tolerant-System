#include<sys/types.h>
#include<string.h>
#include<stdbool.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>

#define OFF "off"
#define ON "on"

#define DEBUG 1

char GatewayPort[7];
char GatewayIP[30],SecurityPort[7],SecurityArea[5],SecurityIP[16]; 
char SecondaryGatewayPort[7], SecondaryGIP[30];
char action[4] = OFF;
char status[4] = ON;

void registerDevice(int);
void DeviceRegister(int);
void *SendData(void*s,char*);
void ReadConfig(char *filename);
int MakeConnection();

int main(int argc, char *argv[])
{
	int clnt,clnt2;

	if(argc < 3)
	{
		printf("Incorrect Arguments!\nFormat: .\\a.out <Security Configuration File> <Security Output File>\n");
    	return 0;
	}
	
	ReadConfig(argv[1]);	//Read Configuration first
	

	clnt2 = MakeConnectionToGateway();
	DeviceRegister(clnt2);


	clnt = MakeConnection();	//Establish Connection with server first
	registerDevice(clnt);	//Register Smart Device 

	SendData((void*)&clnt,argv[2]);

	return 0;
}

int MakeConnection()
{
	struct sockaddr_in sock;	//To connect to the Gateway
	int clnt;	//client socket FD
	clnt = socket(AF_INET,SOCK_STREAM,0);
	sock.sin_addr.s_addr = inet_addr(GatewayIP);
	sock.sin_family = AF_INET;
	sock.sin_port = htons(atoi(GatewayPort));
	if(clnt < 0)	//Socket Connetion Failed
	{
		perror("Socket Create Failed");
		exit(0);
	}

	
	//Making connection to the socket
	if(connect(clnt,(struct sockaddr*)&sock,sizeof(sock))<0)
	{	
		perror("Connection Error");
		close(clnt);	//Close Socket FD
		exit(0);
	}
	return clnt;
}

/* Configuration File Parsing */
void ReadConfig(char *filename)
{
	FILE *output;
	output=fopen(filename,"r");

	if(!output)
	{
		printf("Configuration File not found\n");
		exit(1);
	}
	
	//Tokenize configuration with : separator
	fscanf(output,"%[^:]:%s\ndevice:%[^:]:%[^:]:%s\n%[^:]:%s",GatewayIP,GatewayPort,SecurityIP,SecurityPort,SecurityArea,SecondaryGIP,SecondaryGatewayPort);
	
	fclose(output);
}


int MakeConnectionToGateway()
{
	struct sockaddr_in sock;
	int clnt;	//Client FD
	clnt = socket(AF_INET,SOCK_STREAM,0);
	if(clnt < 0)	//Socket Connetion Failed
	{
		perror("Socket Create Failed");
		exit(0);
	}
	
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = inet_addr(SecondaryGIP);
	sock.sin_port = htons(atoi(SecondaryGatewayPort));
	
	if(connect(clnt, (struct sockaddr*)&sock, sizeof(sock)) < 0)
	{	
		perror("Connetion Failed");
		close(clnt);
		exit(-1);
	}

	return clnt;
}


void* SendData(void* cl,char *outFile)
{
	int clnt;
	char msg[256];
	char msg_type[10];
	int msg_size;

	clnt= *(int*)cl; 
	FILE  *fp;
	fp = fopen(outFile,"w");
	if(!fp)
	{
		printf("File Not Opened\n");
		exit(1);
	}
	while(1)
	{	
		bzero(msg,256);
		
		if((msg_size=read(clnt,msg,256)) < 0)
		{
			perror("Received Error");
		}
		else if(msg_size > 0)
		{
			fprintf(fp,"%s",msg);
			printf("%s",msg);
			fflush(fp);
		}
		else
		{
			printf("Error\n");
			exit(1);
		}
		
	}
}


void registerDevice(int clnt)
{
	char Message[256];
	char client_message[256];
	
	sprintf(Message,"Type:register;Action:Security-%s-%s",SecurityIP,SecurityPort);
	printf("%s\n",Message );
	if(send(clnt,Message,strlen(Message),0) < 0)
	{
		printf("Message Sent Failed\n");
	}
	strcpy(status,ON);
	
}

void DeviceRegister(int clnt)
{
	char Message[256];
	char client_message[256];
	
	sprintf(Message,"Type:register;Action:Security-%s-%s",SecurityIP,SecurityPort);
	printf("Security Device\n");
	if(send(clnt,Message,strlen(Message),0) < 0)
	{
		printf("Message Sent Failed\n");
	}


}
