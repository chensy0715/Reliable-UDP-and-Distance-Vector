#include <sys/types.h> 
#include <sys/socket.h> 
#include <unistd.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <netdb.h> 
#include <stdarg.h> 
#include <string.h> 


#define MSS 1024
#define SERVER_PORT 7998


/*UDP Head*/
typedef struct
{
	int seq;
	int ACK;
	int ack_num;
	int pack_size;
}PackHead;

/*UDP Packet*/
struct DataPack
{
	PackHead head;
	char pack[MSS+1];
}data;


int main(int argc, char **argv)
{
    
    int recvwnd = 20*MSS;  //receiver window
	//char recvbuf[21*MSS+1];  //receiver buffer
    //memset(recvbuf, 0, 21*MSS+1);
	int rb_p1 = 1;  //point to the start of the receiver window
	int rb_p2 = rb_p1+recvwnd;  //point to the end of the receiver window    

	char file_name[1025];
	memset(file_name,0,1025);
   
    /*Create UDP Socket*/
    struct sockaddr_in server_addr; 
    memset(&server_addr, 0, sizeof(server_addr)); 
    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(SERVER_PORT); 
  
    /*Create Socket*/
    int server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
    if(server_socket_fd == -1) 
    { 
    	perror("Create Socket Failed:"); 
        exit(1); 
    } 
    
    /*bind*/
    if(-1 == (bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)))) 
    { 
        perror("Server Bind Failed:"); 
        exit(1); 
    }
	
    /*waiting for connection*/
    while(1)
    {
    	/*to obtain the address of the client */
		struct sockaddr_in client_addr; 
		socklen_t client_addr_length = sizeof(client_addr); 
		
		printf("please input the name of the to-be-receive file:\n");
		scanf("%s", file_name);
		/*open the file and start to write*/
	    FILE *fp = fopen(file_name, "w"); 
	    if(NULL == fp) 
	    { 
	    	printf("File:\t%s Can Not Open To Write\n",file_name);  
        	exit(1); 
		} 
		
		int len = 0;
		while(1)
		{
			PackHead confirm_pack;
			
			//accept the data
			if((len = recvfrom(server_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&client_addr,&client_addr_length)) > 0) 
			{
				if(data.head.seq == -1)//data transmission ends
				{
					confirm_pack.seq = 0;//useless
					confirm_pack.ACK = 1;
					confirm_pack.ack_num = -1;//sign
					confirm_pack.pack_size = 0;//useless

					//send confirmation
					if(sendto(server_socket_fd, (char*)&confirm_pack, sizeof(confirm_pack), 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) 
                    { 
                        printf("Send confirm information failed!"); 
                    } 
                    break;

				}
				else if(data.head.seq-rb_p1 == 0)//the packet the server wants exactly
				{
					confirm_pack.seq = 0;//useless
					confirm_pack.ACK = 1;
					confirm_pack.ack_num = data.head.seq+data.head.pack_size;
					confirm_pack.pack_size = 0;//useless

					//send confirmation
					if(sendto(server_socket_fd, (char*)&confirm_pack, sizeof(confirm_pack), 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) 
                    { 
                        printf("Send confirm information failed!"); 
                    } 

                    //move the pointer
                    rb_p1 = rb_p1+data.head.pack_size;
                    rb_p2 = rb_p2+data.head.pack_size;

                    /*write to the file*/
                    if(fwrite(data.pack+1, sizeof(char), data.head.pack_size, fp) < data.head.pack_size) 
                    { 
                        printf("File:\t%s Write Failed\n",file_name); 
                        break; 
                    } 
				}
				else if(data.head.seq-rb_p1 < 0)
				{
					confirm_pack.seq = 0;//useless
					confirm_pack.ACK = 1;
					confirm_pack.ack_num = rb_p1;
					confirm_pack.pack_size = 0;//useless

					//send confirmation
					if(sendto(server_socket_fd, (char*)&confirm_pack, sizeof(confirm_pack), 0, (struct sockaddr*)&client_addr, client_addr_length) < 0) 
                    { 
                        printf("Send confirm information failed!"); 
                    } 
				}
				else//drop out-of-order packet
				{

				}
			}
		}
		printf("Receive File:\t%s From Client Successfully!\n", file_name); 
		fclose(fp); 
		break;
	}//while(1)

	close(server_socket_fd); 
    
	return 0;
}
