#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<string.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<stdbool.h>
#include<signal.h>
#include<sys/types.h>
#include<unistd.h>

#define maximum_make_connection 20
#define BACKEND_SERVER "localhost"
#define BACKEND_PORT 5678
#define NUMBER_OF_DEVICES 3
#define HANDLER_TIMER 20
#define DEBUG 0


pthread_mutex_t m_curr = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m_op = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m_bck = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread, t_setValue;
int sockfd;
int prdr, pwrtr;
int srdr, swrtr;
int t_gateway= 0;
int t_cnt  = 0;
char *status_list[] = {"off", "on"};

typedef struct
{
	int sockid;
	struct sockaddr addr;
	int addr_len;
}make_connection_ds;

struct node
{
	int vector[3];
	struct node *next;
	char msg[100];
};

typedef struct 
{
	char IP[16], Port[7], Area[5];
	char type[15];
	int sockid;
	int id;
}sensor_device;


struct node *start = NULL;
int c_cnt = 0; 
int s_cnt = 0;
char sIP[20],sPort[7];
int sStatus = 0, sSockID = -1;
sensor_device clist[maximum_make_connection];

char GIP[20],gprt[7];
FILE *fpointer;
FILE *fp_gateway;


int motion_detected = 2;	//Initial value (to reset)
int key_detected = 2;	//Initial Value (to reset)
make_connection_ds *current_c = NULL;
bool kill_bool = false;
int backend_port = BACKEND_PORT;
int back_socket = -1;
struct sockaddr_in back_addr;
struct hostent * back_host;
int backend_len;
bool m_key = false;
bool m_motion = false;

int curr_vec[3];
void * make_connection(void *clnt);
void * Protocol_2PC(void*);
void bckend_write(char *str);

void load_q(int v[3],char *msg)
{
	struct node *temp,*traverse;
	int i;
	
	temp = (struct node *) malloc(sizeof(struct node));
	temp->next = NULL;
	strcpy(temp->msg,msg);

	for(i=0;i<3;i++)
	{
		temp -> vector[i] = v[i];	
	}
	if (!start)
	{
		start = temp;
		return;
	}
	traverse = start;
	while(traverse->next != NULL)
		traverse = traverse->next;
	traverse->next = temp;
}

bool isQueue()
{
	if(start)
		return true;
	return false;
}

int maximum(int a, int b)
{
	if(a>b)
		return a;
	return b;
}

void Initialize_Vector()
{
	int i;
	for (i = 0; i < 3; ++i)
	{
		curr_vec[i] = 0;
	}
}


int isExpected(int c[3])
{
	bool flag = false;
	int i;

	for(i=0;i<3;i++)
	{
		if (c[i] > curr_vec[i] + 1)
		{
			return 0;
		}
		else if ((c[i] == curr_vec[i] + 1) && flag == false)
		{
			flag = true;
		}
		else if ((c[i] == curr_vec[i] + 1) && flag == true)
		{
			return 0;
		}
	}
	return true;
}



struct node* get_q()
{
	struct node *traverse = NULL;
	int i;
	struct node *temp = NULL, *prev;

	if(!start)
	{
		return NULL;	
	}
	if(isExpected(start->vector))
	{
		//printf("Retrieved Message: %s\n", start->msg);

		temp = start;
		start = start -> next;
		return temp;
	}
	traverse = start->next;
	prev = start;
	while(traverse)
	{
		//printf("Message: %s\n", traverse->msg);

		if(isExpected(traverse->vector))
		{
			prev -> next = traverse -> next;
			return traverse;	
		}
		prev = traverse;
		traverse = traverse->next;
	}	
	return NULL;
}


void* call_second(void* arg)
{
	char message[200], commit_msg[10];
	char msg_value[10];


	//printf("Secondary callback\n");
	
	while(1)
	{
		bzero(message,200);
		recv(swrtr , message , 200 , 0);
		
		printf("Prepare Message : \n%s\n", message);

		send(swrtr , "OK" , strlen("OK") , 0);
		printf("Vote Commit\n");

		//printf("Message of OK sent\n");

		bzero(commit_msg,10);
		
		recv(swrtr , commit_msg , 10 , 0);
		printf("Vote Commit Received\n");

		if (strstr(commit_msg,"COMMIT"))
		{
			
			pthread_mutex_lock(&m_bck);
			
			sscanf(message,"MSG:%s",msg_value);

			//printf("Writing to BACKEND in CALLBACK: %s\n", msg_value );
			printf("Primary Gateway Commited Message to Database: \n%s\n", msg_value);
			printf("--------------------------------------------------------------------------------------------\n");
			bckend_write(msg_value);

			pthread_mutex_unlock(&m_bck);	

		}
	}
}

int conn_find(int sockid)
{
	int i;
	for(i=0;i<c_cnt;i++)
	{
		if(clist[i].sockid == sockid)
	
			return i;
	}
	return -1;
}

void update_vector(int v[3])
{
	int j;
	for (j = 0; j < 3; ++j)
	{
		curr_vec[j] = maximum(curr_vec[j],v[j]);
	}
	
	printf("\n");
}

void register_primary()
{
	struct sockaddr_in server, client;
	int primary_listener;
	char secondary_message[2000];
	int c,read_size,return_sent;

	
	primary_listener = socket(AF_INET , SOCK_STREAM , 0);
	
	if (primary_listener == -1)
    {
        printf("error");
    }

    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 2234 );

    //Bind
    if( bind(primary_listener,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        exit(0);
    }

	//Listen
    listen(primary_listener , 3);

    c = sizeof(struct sockaddr_in);
     
    //accept make_connection from an incoming client
    prdr = accept(primary_listener, (struct sockaddr *)&client, (socklen_t*)&c);
    if (prdr < 0)
    {
        perror("accept failed");
        exit(0);
    }

	bzero(secondary_message,2000);
	read_size = recv(prdr , secondary_message , 2000 , 0);

	if(read_size > 0)
	{
		//printf("Message Received from Secondary: %s\n", secondary_message);
	}

	//Second socket starts here
	sleep(1);

	pwrtr = socket(AF_INET , SOCK_STREAM , 0);
	
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(2235 );
 
 	if (connect(pwrtr , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        exit(0);
    }
     
	return_sent =  send(pwrtr , "Hello" , 5 , 0);


	//11-12
	if((pthread_create(&thread,NULL,make_connection,(void*)&prdr)) != 0)
	{
			perror("Thread Creation Failed");
	}
}

void register_backend(char *host, char *p)
{
		/* create socket */
	back_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (back_socket <= 0)
	{
		fprintf(stderr, "error: cannot create socket\n");
		exit(1);
	}
		/* connect to server */
	backend_port = atoi(p);
	
	back_addr.sin_family = AF_INET;
	
	back_addr.sin_port = htons(backend_port);
	back_host = gethostbyname(host);
	
	if (!back_host)
	{
		fprintf(stderr, "error: unknown host \n");
		exit(1);
	}

	memcpy(&back_addr.sin_addr, back_host->h_addr_list[0], back_host->h_length);
	if (connect(back_socket, (struct sockaddr *)&back_addr, sizeof(back_addr)))
	{
		perror("connect");
		exit(1);
	}

	/* send text to server */
	backend_len = 8;	/* length of Register word*/
	write(back_socket, &backend_len, sizeof(int));
	write(back_socket, "Register", backend_len);
}


void register_secondary()
{

	int secondary_listener;
	char primary_message[2000];
	int c,read_size,return_sent;
	struct sockaddr_in server , client;


	srdr = socket(AF_INET , SOCK_STREAM , 0);
	
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( 2234 );
 
 	if (connect(srdr , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        exit(0);
    }
     
	return_sent =  send(srdr , "Hello1" , 6 , 0);

	secondary_listener = socket(AF_INET , SOCK_STREAM , 0);
	
	if (secondary_listener == -1)
    {
        perror("create");
    }

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 2235 );

    //Bind
    if( bind(secondary_listener,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("bind failed. Error");
        exit(0);
    }

	//Listen
    listen(secondary_listener , 3);

    c = sizeof(struct sockaddr_in);
     
    //accept make_connection from an incoming client
    swrtr = accept(secondary_listener, (struct sockaddr *)&client, (socklen_t*)&c);
    if (swrtr < 0)
    {
        perror("accept failed");
        exit(0);
    }
	bzero(primary_message,2000);
	read_size = recv(swrtr , primary_message , 2000 , 0);

	if(read_size > 0)
	{
		
	}


}


void monitor_res()
{
	//Reseting initial conditions
	m_motion = false;
	m_key = false;
	motion_detected = 2;
	key_detected = 2;
}

void bckend_write(char *str)
{
	backend_len = strlen(str);


	write(back_socket, &backend_len, sizeof(int));
	write(back_socket, str, backend_len);	
}



int detect_user(char *device, char *value)
{
	if(!(strcmp(device,"Door")) && (!strcmp(value,"Open")))
	{
		monitor_res();		
		return 2;
	}
	else if(!(strcmp(device,"Door")) && (!strcmp(value,"Close")))
	{
		m_motion = true;
		m_key = true;
		return 2;
	}

	else if(m_motion && m_key)
	{
		if(!(strcmp(device,"Motion")) && (!strcmp(value,"True")))
		{
			motion_detected = 1;
		}

		else if(!(strcmp(device,"Motion")) && (!strcmp(value,"False")))
		{
			printf("User Left Home! System ON\n");
			motion_detected = 0;
			return 1;
		}
		else if(!(strcmp(device,"KeyChain")) && (!strcmp(value,"True")))
		{
			key_detected = 1;
		}	
		else if(!(strcmp(device,"KeyChain")) && (!strcmp(value,"False")))
		{
			key_detected = 0;
		}
		if(motion_detected != 2 && key_detected != 2)
		{
			if(motion_detected)
			{
				if(key_detected)
				{
					printf("User Back Home! System OFF\n");
					return 0;	//turn off security system
				}
				else
				{
					printf("Intruder Alarm\n");
					return 3;	//turn on security system
				}

			}
			else
			{
				//printf("No one in the house\n");
			}
			monitor_res();
		}
	}
	return 2;
}

void SecurityHandler(char *msg)
{
	char temp[5];
	sscanf(msg,"Security-%[^-]-%s",sIP,sPort);
	while(true)
	{
		sleep(HANDLER_TIMER);
	}
}

void * make_connection(void *clnt)
{
	int client = *(int*)clnt;
		
	int message_size;
	char message[256];
	char status[4];
	char message_type[10];
	char action[100];
	char type_temp[20];
	char devmsg[100];
	int i,j,ind;
	char temp_area[3];
	bool flag_to_on = false;
	char value[10], device[10];
	int temp_vector[3];
	int msgtime;

	char strToBackend[100];
	char SensIP[20], SensPort[7];


	int expect = 0;
	
	strcpy(devmsg,"");

	while(true)
	{
		if(kill_bool)
			break;

		bzero(message,256);

		if((message_size = read(client,message,256))<0)
		{
			perror("read");
			return 0;
		}

		if(message_size != 0)
		{
			pthread_mutex_lock(&m_op);
			fprintf(fp_gateway, "Received: %s\n", message );
			fflush(fp_gateway);
			pthread_mutex_unlock(&m_op);

			bzero(action,100);
			if(strstr(message,"register") != NULL)
			{
				sscanf(message, "Type:%[^;];Action:%s", message_type,action);
				if(strstr(message,"Security") != NULL)
				{
					sSockID = client;
					SecurityHandler(action);
				}
			}
			else
			{
				bzero(device,10);
				bzero(value,10);
				sscanf(message, "Type:%[^;];Device:%[^;];Time:%d;Value:%[^;];SensIP:%[^;];SensPort:%[^#]#%d:%d:%d", message_type,device,&msgtime,value,SensIP,SensPort,&temp_vector[0],&temp_vector[1],&temp_vector[2]);
			}
			
			if(strcmp("currValue", message_type) == 0)	//to check type of a msg 
			{
				//printf("Reached in currValue\n");
				sscanf(action, "Device:%[^;];Value:%s", device,value);

				
				
				if(client == prdr)
				{

				sprintf(strToBackend,"%s--%s--%d--%s--%s--[%d,%d,%d,0]",device,value,msgtime,SensIP,SensPort,temp_vector[0],temp_vector[1],temp_vector[2]);	
				}
				else
				{
					int index = conn_find(client);
					sprintf(strToBackend,"%s--%s--%d--%s--%s--[%d,%d,%d,0]",clist[index].type,value,msgtime,clist[index].IP,clist[index].Port, temp_vector[0], temp_vector[1], temp_vector[2]);	
				}


				if(t_gateway == 2)
				{
					pthread_mutex_lock(&m_curr);
					if(send(srdr,message,strlen(message),0) < 0)
					{
						perror("Message Sent Failed");
					}
					sleep(1);
					pthread_mutex_unlock(&m_curr);
					
				}
				else
				{
					pthread_mutex_lock(&m_curr);
		
					load_q(temp_vector,strToBackend);
					pthread_mutex_unlock(&m_curr);
				}			
			}
			else if(strcmp("register", message_type) == 0)
			{
				sscanf(action, "%[^-]-%[^-]-%s", clist[c_cnt].IP, clist[c_cnt].Port, clist[c_cnt].type);
				clist[c_cnt].sockid = client;
				clist[c_cnt].id = c_cnt + 1;
				
				c_cnt++;

				if(c_cnt == NUMBER_OF_DEVICES)
				{
					if(t_gateway== 1)
					{	
						if((pthread_create(&thread,NULL,Protocol_2PC,NULL))!=0)
						{
							perror("Thread Creation Failed");
						}
						//printf("Protocol_2PC Thread Creation\n");
					}
					for (i = 0; i < c_cnt; ++i)
					{
						for(j=0;j < c_cnt;j++)
						{
							sprintf(devmsg,"%s-%s-%s",clist[j].type,clist[j].IP,clist[j].Port);
					
							write(clist[i].sockid, devmsg, sizeof(devmsg));	
							sleep(1);
					
						}
					}
				}
			}
		}
		else
		{
			break;
		}
	}
	
	return 0;
}

void* Protocol_2PC(void* arg)
{
	
	struct node *traverse = NULL;
	char ok_msg[5];
	char commit_msg[10];
	char vote_msg[100];
	char write_msg[100];
	int i;
	char device[20], value[10];
	char arg_str[100];
	int msgtime;
	char securityString[20];

	while(1)
	{
		pthread_mutex_lock(&m_curr);
		if(!isQueue())
		{
			pthread_mutex_unlock(&m_curr);
			usleep(250);
			continue;
		}
		//printf("Waiting for a lock in get_q\n");
		
		if((traverse = get_q()) == NULL)
		{
			pthread_mutex_unlock(&m_curr);	
			usleep(250);
			continue;
		}
		
		pthread_mutex_unlock(&m_curr);

		t_cnt++;
		sprintf(vote_msg,"MSG:T%d--",t_cnt);
		strcat(vote_msg,traverse->msg);
		sprintf(write_msg,"T%d--%s",t_cnt,traverse->msg);
		send(pwrtr,vote_msg,strlen(vote_msg) , 0);
		printf("Prepare Message : \n%s\n", vote_msg);

		bzero(ok_msg,10);
		recv(pwrtr, ok_msg , 10 , 0);
		printf("Commit Received\n");

		strcpy(commit_msg,"COMMIT");
		send(pwrtr,commit_msg,strlen(commit_msg),0);
		printf("%s\nSecondary Gateway Commited Message to Database\n", write_msg);
		printf("--------------------------------------------------------------------------------------------\n");

		bckend_write(write_msg);
		update_vector(traverse->vector);
		
		sscanf(traverse->msg,"%[^-]--%[^-]--%d--%s",device,value,&msgtime,arg_str);
		
		sStatus = detect_user(device,value);
		if (sStatus != 2)
		{
			if (sStatus == 1 || sStatus == 3)
				strcpy(value,"On");
			else
				strcpy(value,"Off");
			printf("sStatus:%s\n",value);

			t_cnt++;
			sprintf(write_msg,"T%d--SecuritySystem--%s--%d--%s--%s",t_cnt,value,msgtime,sIP,sPort);
			strcpy(vote_msg,"MSG:");
			strcat(vote_msg,write_msg);
			
			//printf("Write to Security\n");
			write(sSockID,write_msg,sizeof(write_msg));
			//printf("Written to Security\n");

			send(pwrtr,vote_msg,strlen(vote_msg) , 0);
			printf("Prepare Message : \n%s\n", vote_msg);
			bzero(ok_msg,10);
			recv(pwrtr, ok_msg,10,0);
			printf("Commit Received\n");
			strcpy(commit_msg,"COMMIT");
			send(pwrtr,commit_msg,strlen(commit_msg),0);
			printf("%s\nMessage Commited to Database\n", write_msg);
			bckend_write(write_msg);

			if(sStatus == 1)
				strcpy(securityString,"User_Left_SYSTEM_ON");
			else if(sStatus == 0)
				strcpy(securityString,"User_Back_SYSTEM_OFF");
			else
				strcpy(securityString,"Intruder_Alarm");

			t_cnt++;
			sprintf(write_msg,"T%d--%s--%d",t_cnt,securityString,msgtime);
			strcpy(vote_msg,"MSG:");
			strcat(vote_msg,write_msg);
			
			send(pwrtr,vote_msg,strlen(vote_msg) , 0);
			printf("Prepare Message : \n%s\n", vote_msg);
			bzero(ok_msg,10);
			recv(pwrtr, ok_msg,10,0);
			printf("Commit Received\n");
			strcpy(commit_msg,"COMMIT");
			send(pwrtr,commit_msg,strlen(commit_msg),0);
			printf("%s\nMessage Commited to Database\n", write_msg);
			bckend_write(write_msg);
		}

		free(traverse);
	}
	
}

void ConnectionTry()
{
	struct sockaddr_in server;
	//initialize Socket
	int userThread = 0;
	int yes=1,clnt=0;
	int temp_sockfd;

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	//Socket Creation

	if(sockfd < 0)
	{
		perror("Socket Creation Failed");
		exit(1);
	}	

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(atoi(gprt));	

	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1)
	{
		perror("Socket opt Failed");
		close(sockfd);
		exit(1);
	}

	if(bind(sockfd,(struct sockaddr*)&server,sizeof(server))<0)
	{
		perror("Binding Failed");
		close(sockfd);
		exit(1);
	}
	
	listen(sockfd,3);
	printf("Gateway Registered\n");

	current_c = (make_connection_ds*) malloc (sizeof(make_connection));
	while((current_c->sockid = accept(sockfd, &current_c->addr, (socklen_t*)&current_c->addr_len)))
	{

		temp_sockfd = current_c->sockid;
		if((pthread_create(&thread,NULL,make_connection,(void*)&temp_sockfd))!=0)
		{
			perror("Thread Creation Failed");
		}
	}
	if(clnt < 0)
	{
		perror("Accept Failed");
		close(sockfd);
		exit(1);
	}
	fclose(fp_gateway);
	close(sockfd);
}



void  KillHandler(int sig)
{
  signal(sig, SIG_IGN);
  pthread_mutex_lock(&m_bck);
  pthread_mutex_unlock(&m_bck);
  kill(getpid(),SIGKILL);
}

void InitConfig(char *fname)
{
	FILE *cfg;
	char temp[15];
	cfg = fopen(fname,"r");
	if(!cfg)
	{
		printf("Configuration File not found\n");
		exit(1);
	}
	fscanf(cfg,"%s\n%[^:]:%s\n",temp,GIP,gprt);
	
	if(strcmp(temp,"Primary") == 0)
		t_gateway= 1;
	else
		t_gateway= 2;

	//printf("Which Gateway value: %d\n", which_gateway);
	fclose(cfg);					
}


int main(int argc, char *argv[])
{
	pthread_t thread_secondaryCallback;
	if(argc < 5)
	{
		printf("Incorrect Arguments!\nFormat: .\\a.out <Gateway Configuration File> <Gateway Output File> <Database IP> <Database port>\n");
		return 0;
	}
	signal(SIGINT, KillHandler); 
	signal(SIGTSTP, KillHandler);
	fp_gateway = fopen(argv[2],"w");  
	if(!fp_gateway)
  	{
	    printf("File not opened!\n");
	    exit(1);
  	}
  
	register_backend(argv[3],argv[4]);
	sleep(1);

	Initialize_Vector();
	InitConfig(argv[1]);


	if(t_gateway== 1)
		register_primary();
	else
		register_secondary();
 
	Initialize_Vector();
	InitConfig(argv[1]);

	if(t_gateway== 2)
	{
		if((pthread_create(&thread_secondaryCallback,NULL,call_second,NULL)!=0))
		{
			perror("Thread Creation Failed");
		}
	}
	ConnectionTry();
	getchar();
	return 0;
}
