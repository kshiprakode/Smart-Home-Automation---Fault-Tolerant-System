#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct
{
	int sock;
	struct sockaddr address;
	int addr_len;
} connection_t;

bool reg_flag = false;

FILE *fp;

void * WriteToFile(void * ptr)
{
	char * buffer;
	int len;
	connection_t * conn;
	long addr = 0;

	/* To Store the data in this file*/
	
	
	
	if (!ptr) pthread_exit(0); 
	conn = (connection_t *)ptr;

	/* read length of message */
	while(1)
	{
		read(conn->sock, &len, sizeof(int));
		if (len > 0)
		{
			addr = (long)((struct sockaddr_in *)&conn->address)->sin_addr.s_addr;
			buffer = (char *)malloc((len+1)*sizeof(char));
			buffer[len] = 0;

			/* read message */
			if(!(read(conn->sock, buffer, len) > 0))
				break;


			if (!reg_flag)
			{
				reg_flag = true;
				
				continue;
			}
			
			fprintf(fp, "%s\n",buffer);
			fflush(fp);
			free(buffer);
		}
	}

	/* close socket and clean up */
	close(conn->sock);
	free(conn);
	fclose(fp);
	pthread_exit(0);
}

int main(int argc, char * argv[])
{
	int sock = -1;
	struct sockaddr_in address;
	int port;
	connection_t * connection;
	pthread_t thread;

	printf("Database\n");

	/* check for command line arguments */
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s outputFile port\n", argv[0]);
		return -1;
	}

	/* obtain port number */
	if (sscanf(argv[2], "%d", &port) <= 0)
	{
		fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
		return -2;
	}

	fp = fopen(argv[1],"w+");

	if(!fp)
	{
		printf("outputFile Can't open\n");
		exit(0);
	}

	/* create socket */
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock <= 0)
	{
		fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
		return -3;
	}

	/* bind socket to port */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
	{
		fprintf(stderr, "%s: error: cannot bind socket to port %d\n", argv[0], port);
		return -4;
	}

	/* listen on port */
	if (listen(sock, 5) < 0)
	{
		fprintf(stderr, "%s: error: cannot listen on port\n", argv[0]);
		return -5;
	}
	
	while (1)
	{
		/* accept incoming connections */
		connection = (connection_t *)malloc(sizeof(connection_t));
		connection->sock = accept(sock, &connection->address, &connection->addr_len);
		if (connection->sock <= 0)
		{
			free(connection);
		}
		else
		{
			/* start a new thread but do not wait for it */
			pthread_create(&thread, 0, WriteToFile, (void *)connection);
			pthread_detach(thread);
		}
	}
	return 0;
}