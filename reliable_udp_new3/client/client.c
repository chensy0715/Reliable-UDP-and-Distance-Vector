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
#include <time.h>
#include <fcntl.h>
  

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
	int sendwnd;  //sender window
	int cwnd = 1*MSS;  //conjestion window
	int recvwnd = 20*MSS;  //the server's window, from command line parameter
	char sendbuf[21*MSS+1];  //sender buffer
   

	int file_byte_ptr = 1; //point the start byte of the unsent data

	int ssthresh = 16*MSS;

	time_t pack_time_start[31];
	time_t pack_time_end[31];

	int flag;

	char file_name[1025];
	memset(file_name,0,1025);
	
	/* Server Address*/
	struct sockaddr_in server_addr; 
    memset(&server_addr, 0, sizeof(server_addr)); 
    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    server_addr.sin_port = htons(SERVER_PORT); 
    socklen_t server_addr_length = sizeof(server_addr); 
  
    /* create socket */
    int client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
    if(client_socket_fd < 0) 
    { 
        perror("Create Socket Failed:"); 
        exit(1); 
    } 

    flag = fcntl(client_socket_fd, F_GETFL, 0);  
    if (flag < 0)  
    {  
        perror("fcntl failed.\n");  
        exit(1);  
    }  
    flag |= O_NONBLOCK;  
    if (fcntl(client_socket_fd, F_SETFL, flag) < 0)  
    {  
        perror("fcntl failed.\n");  
        exit(1);  
    }  

    
    printf("please input the name of the to-be-send file:\n");
	scanf("%s", file_name);
    /*open the file and start to read*/
    FILE *fp = fopen(file_name, "r"); 
    if(NULL == fp) 
    { 
        printf("File:%s Not Found.\n",file_name);
        exit(1); 
    } 
    
   
	int len = 0;
	int recv_len = 0;
	int before_confirm = 0;

	time_t RTT = 0;
	time_t SRTT = 0;
	time_t DRTT = 0;
	time_t RTO = 0;

	int last_bytes=0;

	int times=0;
    while(1)
    {
    	PackHead confirm_pack; //comfirmation packet

        //sending data
    	//while(((len = fread(sendbuf+1, sizeof(char), 21*MSS, fp)) > 0) || (sb_p1 != sb_p2))
    	 /*read data from the file*/
    	while(((len = fread(sendbuf+1, sizeof(char), 21*MSS, fp)) > 0))
    	{
    		times++;

			memset(sendbuf, 0, 21*MSS+1);
			int sb_p1 = 1;  //point to the start of the sender window
			int sb_p2 = 1;  //point to the byte confirmed
			int sb_p3 = 1;  //point to the end of the sender window  

			int packet_num = 0;
			memset(pack_time_start, 0, 31);
			memset(pack_time_end, 0, 31);

			int special_packet = 0;
			int special_bytes = 0;

    		while(len != 0 || last_bytes > 0) //if last bytes>0,there are still packets unreceived
			{
				sendwnd = cwnd<recvwnd?cwnd:recvwnd;//sendwnd = the smaller window
    		    //sb_p3 = (sb_p1+sendwnd)%(21*MSS); // the end of the sender window
    		    sb_p3 = sb_p1+sendwnd; // the end of the sender window

    		    while((sb_p2 != sb_p3)&&(len != 0))
    		    {

					/*create the data packet*/
    		        data.head.seq = file_byte_ptr;
    		        data.head.ACK = 0; //useless
    		        data.head.ack_num = 0; //useless
    		        data.head.pack_size = len<MSS?len:MSS;

    		        strncpy(data.pack+1, sendbuf+sb_p2, data.head.pack_size); 
    		    

    		        if(sendto(client_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0) 
                    { 
						perror("Send File Failed:"); 
						break; 
                    }

					//move the pointer of the send buffer
					sb_p2 = sb_p2+data.head.pack_size;

					file_byte_ptr = file_byte_ptr+data.head.pack_size;

					len = len-data.head.pack_size;

					//count the time
					packet_num++;
					pack_time_start[packet_num]=time(NULL);

					if(data.head.pack_size<MSS)
					{
						special_packet = packet_num;
						special_bytes = data.head.pack_size;
					}

				}

				/* try to receive confirmation */
				
    			recv_len = recvfrom(client_socket_fd, (char*)&confirm_pack, sizeof(confirm_pack), 0, (struct sockaddr*)&server_addr, &server_addr_length); 
				sleep(1);
        		if (recv_len == -1 && errno != EAGAIN)//time out
        		{
        			perror("recv failed.\n");  
            		exit(1);
        		}
        		else if (recv_len == 0 || (recv_len == -1 && errno == EAGAIN))
        		{
        			
        		}  
        		else
        		{
        			
        			if(confirm_pack.ack_num > (sb_p1+((times-1)*21*MSS)))
					{
						before_confirm = sb_p1;
						sb_p1 = confirm_pack.ack_num-((times-1)*21*MSS);

						if(cwnd <= ssthresh)
						{
							cwnd = cwnd+(sb_p1-before_confirm);
						}
						else if(sb_p1 == sb_p3)
						{	
							cwnd = cwnd+1*MSS;
						
						}
						else
						{

						}
						pack_time_end[(sb_p1-1)/MSS] = time(NULL);//NULL;
						RTT = pack_time_end[(sb_p1-1)/MSS] - pack_time_start[(sb_p1-1)/MSS];
						SRTT = 0.875*SRTT + 0.125*RTT;
						DRTT = 0.75*DRTT + abs(SRTT-RTT);
						RTO = SRTT +4*DRTT;


						for(int i = before_confirm; i<sb_p1; i+=MSS)
						{
							pack_time_start[((i-1)/MSS)+1] = 0;//NULL;
							pack_time_end[((i-1)/MSS)+1] = 0;//NULL;
						}	
					}
					else//drop the useless confirmation
					{

					}		
        		}
        		for(int i=1; i <= packet_num;i++)//last packet!
				{
					if(pack_time_start[i] != 0)
					{
						pack_time_end[i]=time(NULL);
					}
					
					if(pack_time_end[i] - pack_time_start[i] > RTO)//no response
					{
						/*create the data packet*/
						data.head.seq = MSS*(i-1)+1+(21*MSS)*(times-1);
						data.head.ACK = 0; //useless
						data.head.ack_num = 0; //useless
						if(i == special_packet)
						{
							data.head.pack_size = special_bytes;
						}
						else
						{
							data.head.pack_size = MSS;//if the last packet is lost ,cannot support
						}
						strncpy(data.pack+1, sendbuf+data.head.seq, data.head.pack_size);
						 
						//time out and send again
						
						if(sendto(client_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0) 
						{ 
							perror("Send File Failed:"); 
							break; 
						}
						ssthresh = ssthresh/2;
						cwnd = 1*MSS;
						RTO = 2*RTO;
					} 
				}
				last_bytes = sb_p2 - sb_p1;	
			}
    	}
    	/*create the data packet*/
    	data.head.seq = -1;
    	data.head.ACK = 0; //useless
    	data.head.ack_num = 0; //useless
    	data.head.pack_size = 0;
    		    

    	if(sendto(client_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0) 
        { 
			perror("Send File Failed:"); 
			break; 
        }
        /* try to receive confirmation*/
    	while(1)
    	{
    		recv_len = recvfrom(client_socket_fd, (char*)&confirm_pack, sizeof(confirm_pack), 0, (struct sockaddr*)&server_addr, &server_addr_length); 
			sleep(1);
        	if (recv_len == -1 && errno != EAGAIN)//time out
        	{
        		perror("recv failed.\n");  
            	exit(1);
        	}
        	else if (recv_len == 0 || (recv_len == -1 && errno == EAGAIN))
        	{

        	}
        	else
        	{
        		if(confirm_pack.ack_num == -1)
        		{
        			break;
        		}	
        	}
    	} 
    	break;  	
    }//wihle(1)
    
    /*close the file*/
    fclose(fp);
    printf("File Transfer Successful!\n"); 
    close(client_socket_fd);
    
    return 0;
}
