#include<errno.h>
#include<signal.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<stdbool.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<pthread.h>
#include<arpa/inet.h>

struct 
{
  int sockid;
  int interval;
}CurrInterval[20];

struct sensor_device
{
  char IP[16];
  char Port[7];
  char type[20];
  int sockid;
};

int caller = 0;

pthread_t tid;
int clnt,clnt2;
char GPort[7], GIP[30],SensPort[7],SensArea[20],SensIP[16]; 
char GatewayPort[7], GatewayIP[30];
int SensorCount = 0;
struct sensor_device ConnectionList[20];
int ConnectionCount = 0;
int selfcounter = 0;
int vc[3] = {0,0,0};
int my_index = -1;;
char *outFile;
FILE *Output;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


void InitConfiguration(char *);
int ConnectToGateway();
void DeviceRegister(int );
int max(int, int);
void  KillHandler(int);
void registerDevice(int, char*);
int MakeConnection();

int main(int argc, char *argv[])
{
  
  if(argc < 3)
  {
    printf("Incorrect Arguments!\nFormat: .\\a.out <KeyChain Configuration File> <KeyChain State File> <KeyChain Output File>\n");
    return 0;
  }
  
  printf("Motion\n");
  
  Output = fopen(argv[3],"w");  
  
  if(!Output)
  {
    printf("File not found\n");
    exit(1);
  }

  signal(SIGINT, KillHandler); 
  signal(SIGTSTP, KillHandler);
  
  InitConfiguration(argv[1]);

  clnt2 = ConnectToGateway();
  DeviceRegister(clnt2);

  clnt = MakeConnection();
  registerDevice(clnt, argv[2]);

  return 0;
}

void* InitParams(void*);

int ConnectToGateway()
{
  struct sockaddr_in sock;
  int clnt; //Client FD
  clnt = socket(AF_INET,SOCK_STREAM,0);
  if(clnt < 0)  //Socket Connetion Failed
  {
    perror("Socket Create Failed");
    exit(0);
  }
  
  sock.sin_family = AF_INET;
  sock.sin_addr.s_addr = inet_addr(GatewayIP);
  sock.sin_port = htons(atoi(GatewayPort));
  
  if(connect(clnt, (struct sockaddr*)&sock, sizeof(sock)) < 0)
  { 
    perror("Connetion Failed");
    close(clnt);
    exit(-1);
  }
  
 return clnt;
}


void DeviceRegister(int clnt)
{

  char msg[256];
  char client_message[256];

  CurrInterval[SensorCount].sockid = clnt;
  CurrInterval[SensorCount].interval = 5;

  SensorCount += 1;

  sprintf(msg,"Type:register;Action:%s-%s-%s",SensIP,SensPort,SensArea);

  if(send(clnt,msg,strlen(msg),0)<0)
  {
    perror("Message Sent Failed");
  }
}


void InitConfiguration(char *filename)
{
  FILE *cfg;
  cfg = fopen(filename,"r");
  if(!cfg)
  {
    printf("Configuration File not found\n");
    exit(1);
  }
  
  fscanf(cfg,"%[^:]:%s\nsensor:%[^:]:%[^:]:%s\n%[^:]:%s",GIP,GPort,SensIP,SensPort,SensArea,GatewayIP,GatewayPort);

  if(!strcmp(SensArea,"Door"))
    caller = 1;
  else if(!strcmp(SensArea,"Motion"))
    caller = 2;
  else if(!strcmp(SensArea,"KeyChain"))
    caller = 3;

  fclose(cfg);
}

int max(int n1, int n2)
{
  if(n1 > n2)
    return n1;
  return n2;
}

void updateVectors(char *buffer)
{
  char temp_type[20];
  char temp_device[20];
  char temp_value[10];
  int temp_vector[3];
  int i;

  //printf("In updateVectors\n");

  sscanf(buffer,"Type:%[^;];Device:%[^;];Value:%[^#]#%d:%d:%d",temp_type,temp_device,temp_value,&temp_vector[0],&temp_vector[1],&temp_vector[2]);
  
  //printf("Old Vector\n");
  // 

  for(i=0;i<3;i++)
  {
    //printf("Vector updated\n");
    vc[i] = max(vc[i],temp_vector[i]);
  }
  // 
}

void selection3(char *fname)
{
  int newsockfd[2], portno, n;
  int i;
   struct sockaddr_in serv_addr;
   struct hostent *server;
   
   char buffer[256];
  
  //printf("Entered in Selection 3\n");

   for(i = 0 ;i < 2 ;i++)
   {
      /* Create a socket point */
     newsockfd[i] = socket(AF_INET, SOCK_STREAM, 0);
     
     if (newsockfd[i] < 0) {
        perror("ERROR opening socket");
        exit(1);
     }
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = inet_addr(ConnectionList[i].IP);
     serv_addr.sin_port = htons(atoi(ConnectionList[i].Port));

      

     /* Now connect to the server */
     if (connect(newsockfd[i], (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connect failed\n");
        printf("ERROR connecting: %s\n",strerror(errno));
        exit(1);
     }

     //printf("Type[%d]: %s\n", i, SensArea );
     //SEND A MESSAGE ABOUT TYPE
     write(newsockfd[i], SensArea, sizeof(SensArea));
     ConnectionList[i].sockid = newsockfd[i];  
   }
  
    

  //printf("Thread creation\n");
  if(pthread_create(&tid,NULL,InitParams,(void*)fname) != 0)
  {
    perror("Thread creation failed");
    exit(1);
  }

   char c;
   fd_set rfds, alternative;
   struct timeval tv;
   int retval,ind;

   /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&alternative);
    //FD_ZERO(&rfds);
   FD_SET(newsockfd[0],&alternative);
   FD_SET(newsockfd[1],&alternative);
  
  while(1)
  {
    rfds = alternative;
    //printf("Before select\n");
    retval = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
    //printf("After Select\n");

    if(retval == -1)
    {
      printf("Select Error\n");
      exit(1);
    }
    else if(retval > 1)
    {
      /*printf("Greater than 1\n");*/
      for(ind=0;ind<2;ind++)
      {
        bzero(buffer,256);
        read(newsockfd[ind],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
    else if(retval == 1)
    {
      if(FD_ISSET(newsockfd[0],&rfds))
      {
        //printf("newsockfd0\n");
        bzero(buffer,256);
        read(newsockfd[0],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);

      }
      else if(FD_ISSET(newsockfd[1],&rfds))
      {
       
        bzero(buffer,256);
        read(newsockfd[1],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
  }
   
}

void selection2(char *fname)
{
  int master_sockfd, newsockfd[2], portno, clilen;
   char buffer[256];
   struct sockaddr_in serv_addr, cli_addr;
   int  n,i, temp_index;
   char message[256];
   int message_size;
   
   /* First call to socket() function */
   master_sockfd = socket(AF_INET, SOCK_STREAM, 0);
   
   if (master_sockfd < 0) {
      perror("ERROR opening socket");
      exit(1);
   }
   
   /* Initialize socket structure */
   bzero((char *) &serv_addr, sizeof(serv_addr));
   portno = atoi(SensPort);
   
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);
   
   /* Now bind the host address using bind() call.*/
   if (bind(master_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      perror("ERROR on binding");
      exit(1);
   }
      
   /* Now start listening for the clients, here process will
      * go in sleep mode and will wait for the incoming connection
   */
   
   listen(master_sockfd,5);
   clilen = sizeof(cli_addr);
   
   /* Accept actual connection from the client */
   newsockfd[0] = accept(master_sockfd, (struct sockaddr *)&cli_addr, &clilen);
 
   bzero(message,256);

   //printf("Waiting for read\n");
  if((message_size = read(newsockfd[0],message,256))<0)
  {
    perror("Received Message Failed");
    exit(1);
  }
  
  for(i=0; i< 2; i++)
  {
    //printf("Before comparison\n");
    if(strcmp(message,ConnectionList[i].type) == 0)
    {
      ConnectionList[i].sockid = newsockfd[0];
      break;
    }
  }
  /* Create a socket point */
   newsockfd[1] = socket(AF_INET, SOCK_STREAM, 0);
   
   if(i == 0)
    temp_index = 1;
   else
    temp_index = 0;

   //printf("temp_index: %d\n", temp_index );

  ConnectionList[temp_index].sockid = newsockfd[1];

    

   if (newsockfd[1] < 0) {
      perror("ERROR opening socket");
      exit(1);
   }

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = inet_addr(ConnectionList[temp_index].IP);
   serv_addr.sin_port = htons(atoi(ConnectionList[temp_index].Port));

   /* Now connect to the server */
   if (connect(newsockfd[1], (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      perror("ERROR connecting");
      exit(1);
   }

    //printf("Type[%d]: %s\n", i, SensArea );
    //SEND A MESSAGE ABOUT TYPE
    write(newsockfd[1], SensArea, sizeof(SensArea));

    //printf("After accept and connect\n");
     
   
   if (newsockfd[0] < 0) {
      perror("ERROR on accept");
      exit(1);
   }

   if (newsockfd[1] < 0) {
      perror("ERROR on accept");
      exit(1);
   }
   
   /* If connection is established then start communicating */
   //bzero(buffer,256);
  
  //printf("Thread creation\n"); 
  if(pthread_create(&tid,NULL,InitParams,(void*)fname) != 0)
  {
    perror("Thread creation failed");
    exit(1);
  }

   char c;
   fd_set rfds, alternative;
   struct timeval tv;
   int retval,ind;

   /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&alternative);
    //FD_ZERO(&rfds);
   FD_SET(newsockfd[0],&alternative);
   FD_SET(newsockfd[1],&alternative);
  
  while(1)
  {
    rfds = alternative;
    retval = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
    
    if(retval == -1)
    {
      printf("Select Error\n");
      exit(1);
    }
    else if(retval > 1)
    {
      for(ind=0;ind<2;ind++)
      {
        bzero(buffer,256);
        read(newsockfd[ind],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

      
      
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
    else if(retval == 1)
    {
      if(FD_ISSET(newsockfd[0],&rfds))
      {
        bzero(buffer,256);
        read(newsockfd[0],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }
       
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
      else if(FD_ISSET(newsockfd[1],&rfds))
      {
        bzero(buffer,256);
        read(newsockfd[1],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

       
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
  }
}

/* 2 accept - server */
void selection1(char* fname)
{

   int newsockfd[2], portno, clilen;
   char buffer[256];
   struct sockaddr_in serv_addr, cli_addr;
   int  n, i , j;
   int master_port = atoi(SensPort);
   int master_sockfd = -1;
   char message[256];
   int message_size;

   //printf("Filename : %s\n", fname );

   /* First call to socket() function */
   master_sockfd = socket(AF_INET, SOCK_STREAM, 0);
   
   if (master_sockfd < 0) {
      perror("ERROR opening socket");
      exit(1);
   }
   
   /* Initialize socket structure */
   bzero((char *) &serv_addr, sizeof(serv_addr));
   //portno = atoi(argv[1]);
   
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(master_port);
   
   /* Now bind the host address using bind() call.*/
   if (bind(master_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      perror("ERROR on binding");
      exit(1);
   }
      
   /* Now start listening for the clients, here process will
      * go in sleep mode and will wait for the incoming connection
   */
   
   listen(master_sockfd,5);
   clilen = sizeof(cli_addr);
   
   /* Accept actual connection from the client */
   newsockfd[0] = accept(master_sockfd, (struct sockaddr *)&cli_addr, &clilen);
   newsockfd[1] = accept(master_sockfd, (struct sockaddr *)&cli_addr, &clilen);

   if (newsockfd[0] < 0) {
      perror("ERROR on accept");
      exit(1);
   }

   if (newsockfd[1] < 0) {
      perror("ERROR on accept");
      exit(1);
   }
   
   //printf("Accept done successful\n");

   for(i=0;i<2;i++)
   {
    
    bzero(message,256);

    if((message_size = read(newsockfd[i],message,256))<0)
    {
      perror("read");
      //return 0;
    }

    if(strstr(message,"exit"))
    {
      printf("Exit Message\n");
      exit(0);
    }

    /*
    compare with all the connection type and link socket to that particular device 
    so that we can use those sockfd later.

    */
    for(j=0;j<2;j++)
    {
      if(strcmp(ConnectionList[j].type,message) == 0)
      {
        //printf("Both are same : %s\n", ConnectionList[j].type );
        ConnectionList[j].sockid  = newsockfd[i];

        break;
      }
    }

  }
   
  //printf("Thread Created\n");
 
  if(pthread_create(&tid,NULL,InitParams,(void*)fname) != 0)
  {
    perror("Thread creation failed");
  }

   /* If connection is established then start communicating */
   //bzero(buffer,256);
   char c;
   fd_set rfds, alternative;
   struct timeval tv;
   int retval,ind;

   /* Watch stdin (fd 0) to see when it has input. */
   FD_ZERO(&alternative);
    //FD_ZERO(&rfds);
   FD_SET(newsockfd[0],&alternative);
   FD_SET(newsockfd[1],&alternative);
  
   
  while(1)
  {
    rfds = alternative;
    //printf("Before select\n");
    retval = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
    
    if(retval == -1)
    {
      printf("Select Error\n");
      exit(1);
    }
    else if(retval > 1)
    {
      //printf("Greater than 1\n");
      for(ind=0;ind<2;ind++)
      {

        bzero(buffer,256);
        read(newsockfd[ind],buffer,sizeof(buffer));

        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[ind], buffer );
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
    else if(retval == 1)
    {
      if(FD_ISSET(newsockfd[0],&rfds))
      {
        //printf("newsockfd[0]\n");
        bzero(buffer,256);
        read(newsockfd[0],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[0], buffer);
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
      else if(FD_ISSET(newsockfd[1],&rfds))
      {
        //printf("newsockfd1\n");
        bzero(buffer,256);
        read(newsockfd[1],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[1], buffer );
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
  }

}

void* InitParams(void *filename)
{
  FILE *file;
  char msg[100];
  int start_time; 
  char val[20];
  int current_time = 0, end_time = -1;  //to set 0 to check initial condition
  char temp_str[20];
  int tmp_time = 1, i;
  char *fname = (char *) filename;
  bool flag_remain = false;
  bool door_file_end = false;

  //printf("outFile: %s\n", outFile);

  file = fopen(fname,"r");  

  if(!file)
  {
    printf("Sensor Input not found\n");
    exit(1);
  }   

  while(true)
  { 
    if(!strcmp(SensArea, "Door"))
    {
      my_index = 0;
      fscanf(file,"%d,%s\n",&end_time,val);

      sleep(end_time-current_time);

      if(feof(file))
      {
        //printf("End  of File\n");
        door_file_end = true;
        current_time = 0;
        fseek(file, 0, SEEK_SET);
      }
      else
      {
        current_time = end_time;
      }
    }
    else 
    {
      if(!strcmp(SensArea, "Motion"))
      {
        //printf("Motion\n");
        my_index = 1;
      }
      else
      {
        my_index = 2;
      }

      //printf("Before flag_remain condition: %d\n", flag_remain );
      if(!flag_remain)
      {
        fscanf(file,"%d,%d,%s\n",&start_time,&end_time,val);
        //printf("start_time: %d, end_time: %d, current_time: %d\n", start_time,end_time,current_time);
        
        if (current_time == 0 && start_time != 0)
        {
          /*printf("Sleep due to start_time\n");*/
          sleep(start_time);
          current_time = start_time;
        }
      }
    } 
    
    pthread_mutex_lock(&mutex);
    vc[my_index] = vc[my_index] + 1;
     
    pthread_mutex_unlock(&mutex);

    //Msg sent to Gateway
    bzero(msg,100);

    pthread_mutex_lock(&mutex);

    sprintf(msg, "Type:currValue;Device:%s;Time:%d;Value:%s;SensIP:%s;SensPort:%s#%d:%d:%d",SensArea,(int)time(NULL),val,SensIP,SensPort,vc[0],vc[1],vc[2]);
    
    printf("Type:currValue;Device:%s;Time:%d;Value:%s;SensIP:%s;SensPort:%s;Vector:[%d,%d,%d,0]\n",SensArea,(int)time(NULL),val,SensIP,SensPort,vc[0],vc[1],vc[2]);
    
    if(send(clnt,msg,strlen(msg),0) < 0)
    {
      perror("Message Sent Failed");
    }
    //Msg sent to other devices
    bzero(msg,100);
    sprintf(msg, "Type:currValue;Device:%s;Value:%s#%d:%d:%d",SensArea,val,vc[0],vc[1],vc[2]);
    pthread_mutex_unlock(&mutex);
    
    for(i =0 ;i < ConnectionCount; i++)
    {
      if(send(ConnectionList[i].sockid,msg,strlen(msg),0) < 0)
      {
        perror("Message Sent Failed");
      }
      
    }
   
    fprintf(Output, "Type:currValue;Device:%s;Time:%d;Value:%s;SensIP:%s;SensPort:%s;Vector:[%d,%d,%d,0]\n",SensArea,(int)time(NULL),val,SensIP,SensPort,vc[0],vc[1],vc[2]);
    fflush(Output);

    if(my_index != 0)
    {

      sleep(5); 
      current_time += 5;

      if(current_time <= end_time)
          flag_remain = true;

      else if(feof(file))
      {

        fseek(file, 0, SEEK_SET);
        current_time = 0;
        flag_remain = false;
      }
      else
      {
        flag_remain = false;
      }
    }
    else
    {
      if(door_file_end)
      {
        sleep(1);
        door_file_end = false;
      }
    }
  }
}

int MakeConnection()
{
  struct sockaddr_in sock;
  int clnt; //Client FD
  clnt = socket(AF_INET,SOCK_STREAM,0);
  if(clnt < 0)  //Socket Connetion Failed
  {
    perror("Socket Create Failed");
    exit(0);
  }
  
  sock.sin_family = AF_INET;
  sock.sin_addr.s_addr = inet_addr(GIP);
  sock.sin_port = htons(atoi(GPort));
  
  if(connect(clnt, (struct sockaddr*)&sock, sizeof(sock)) < 0)
  { 
    perror("Connetion Failed");
    close(clnt);
    exit(-1);
  }
  
  return clnt;
}

void registerDevice(int clnt, char* fname)
{

  char msg[256];
  int i;
  char temp_IP[20];
  char temp_Port[7];
  char temp_type[20];
  int message_size;

  CurrInterval[SensorCount].sockid = clnt;
  CurrInterval[SensorCount].interval = 5;

  SensorCount += 1;

  sprintf(msg,"Type:register;Action:%s-%s-%s",SensIP,SensPort,SensArea);

  printf("%s\n",msg );

  if(send(clnt,msg,strlen(msg),0)<0)
  {
    perror("Message Sent Failed");
  }
  for(i=0;i<3;i++)
  {
    bzero(msg,256);
    
    if((message_size = read(clnt,msg,256))<0)
    {
      perror("Received Message Failed");
    }

    sscanf(msg,"%[^-]-%[^-]-%s",temp_type,temp_IP,temp_Port);
    if(strcmp(SensPort,temp_Port) == 0 && strcmp(SensIP,temp_IP) == 0)
    {
      continue;
    }
    strcpy(ConnectionList[ConnectionCount].type,temp_type);
    strcpy(ConnectionList[ConnectionCount].IP,temp_IP);
    strcpy(ConnectionList[ConnectionCount].Port,temp_Port);
    ConnectionCount++;
  }

   

  if(caller == 1)
   selection1(fname);
  else if(caller == 2)
    selection2(fname);
  else
    selection3(fname);
}

void  KillHandler(int sig)
{
  int i;
  signal(sig, SIG_IGN);
  for(i =0 ;i < ConnectionCount; i++)
    {
      if(send(ConnectionList[i].sockid,"exit",strlen("exit"),0) < 0)
      {
        perror("Message Sent Failed");
      }
      printf("Exit\n");
    }
  kill(getpid(),SIGKILL);
}



