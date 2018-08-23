#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string.h>
#include <iostream>
#include <fstream>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

#define BUFFERSIZE 1024


typedef struct 
{
	char destination[30]; /* address of the destination */
	char nexthop[30]; /* address of the next hop */
	int cost; /* distance metric */
	unsigned short ttl; /* time to live in seconds */
} Route_entry;

struct Node
{
	char address[30];
	int dist;
};

struct MyReceive
{
	char recvbuf[BUFFERSIZE];
	char srcIP[30];
};

vector <Route_entry> RoutingTable;
vector<Node> nodes;

vector < vector<Node> > Gragh;
char configfile[100];
static int portnum,TTL,INF,period;
static bool posion_reverse;

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER; 

void Write_Up()
{
	
	cout<<"RoutingTable :"<<endl;
	cout<<"destination\tnexthop\tcost\tTTL"<<endl;
	for(int i=0;i<RoutingTable.size();i++)
	{
		if(strcmp(RoutingTable[i].nexthop,"")==0)
			printf("%s\tNULL\t%d\t%d\n",RoutingTable[i].destination,RoutingTable[i].cost,RoutingTable[i].ttl);
		else printf("%s\t%s\t%d\t%d\n",RoutingTable[i].destination,RoutingTable[i].nexthop,RoutingTable[i].cost,RoutingTable[i].ttl);
	}
	cout<<endl;
	cout<<"Gragh :"<<endl;
	cout<<"From/to\t";
	for(int i=0;i<Gragh.size();i++)cout<<Gragh[0][i].address<<"\t";
	cout<<endl;
	for(int i=0;i<Gragh.size();i++)
	{
		cout<<Gragh[0][i].address<<"\t";
		for(int j=0;j<Gragh[i].size();j++)
		{
			printf("%d\t",Gragh[i][j].dist);
		}
		cout<<endl;
	}
	cout<<endl;
	//system("pause");
}

int ReadConfigFile(char* filename)
{
	Node node_t;
	
	Route_entry route_t;
	vector<Node> nodes_t;
	int num=0;
	ifstream file;
	file.open(filename);
	if(!file.is_open())
	{
		return -1;
	}

	
	
	char ipaddr[BUFFERSIZE],is_neighbor[BUFFERSIZE];	
	
	file>>ipaddr>>is_neighbor;
	strcpy(node_t.address,ipaddr);
	node_t.dist=0;
	nodes_t.push_back(node_t);
	num++;
	
	while(file.peek()!=EOF)
	{
		memset(ipaddr,0,sizeof(char)*BUFFERSIZE);
		memset(is_neighbor,0,sizeof(char)*BUFFERSIZE);

		file>>ipaddr>>is_neighbor;
		if(strcmp(ipaddr,"")==0)break;
		strcpy(node_t.address,ipaddr);
		
		if(is_neighbor[0]=='y')
		{
			node_t.dist=1;
		}
		else node_t.dist=INF;
		nodes_t.push_back(node_t);
		nodes.push_back(node_t);		
		num++;
	}

	Gragh.push_back(nodes_t);
	file.close();
	
	for(int i=1;i<num;i++)
	{
		nodes_t.clear();
		for(int j=0;j<num;j++)
		{
			strcpy(node_t.address,Gragh[0][j].address);
			node_t.dist=INF;
			nodes_t.push_back(node_t);
		}
		Gragh.push_back(nodes_t);
	}
	for(int i=0;i<num;i++)
	{
		strcpy(route_t.destination,Gragh[0][i].address);
		route_t.cost=Gragh[0][i].dist;
		if(route_t.cost==INF)strcpy(route_t.nexthop,"");
		else strcpy(route_t.nexthop,route_t.destination);
		route_t.ttl=TTL;
		RoutingTable.push_back(route_t);
	}
	
}

int getRoute(char dst[])
{
	
	for(int i=0;i<RoutingTable.size();i++)
	{
		if(strcmp(RoutingTable[i].destination,dst)==0)return i;
	}
	
	return -1;
}

int getDstNode(char dst[])
{
	
	for(int i=0;i<Gragh[0].size();i++)
	{
		if(strcmp(Gragh[0][i].address,dst)==0)return i;
	}
	
	return -1;
}

int getSrcNode(char dst[])
{
	
	for(int i=0;i<Gragh[0].size();i++)
	{
		if(strcmp(Gragh[0][i].address,dst)==0)return i;
	}
	
	return -1;
}

bool Bellman_Ford(vector<vector<Node> > g,int src)
{
	//Write_Up();
	for(int i=0;i<RoutingTable.size();i++)
		RoutingTable[i].cost=INF;
	RoutingTable[src].cost=0;
	
	for(int i=0;i<RoutingTable.size()-1;i++)
	{
		for(int j=0;j<g.size();j++)
		{
			for(int k=0;k<g[j].size();k++)
			{
				if(g[j][k].dist<INF)
				{
					if(RoutingTable[getRoute(g[j][k].address)].cost >RoutingTable[getRoute(g[j][j].address)].cost + g[j][k].dist)
					{
						RoutingTable[getRoute(g[j][k].address)].cost=RoutingTable[getRoute(g[j][j].address)].cost + g[j][k].dist;
						
						memset(RoutingTable[getRoute(g[j][k].address)].nexthop,0,sizeof(RoutingTable[getRoute(g[j][k].address)].nexthop));
						if(RoutingTable[getRoute(g[j][k].address)].cost==1)strcpy(RoutingTable[getRoute(g[j][k].address)].nexthop,RoutingTable[getRoute(g[j][k].address)].destination);
						else strcpy(RoutingTable[getRoute(g[j][k].address)].nexthop,RoutingTable[getRoute(g[j][j].address)].nexthop);
						RoutingTable[getRoute(g[j][k].address)].ttl=TTL;
					}
				}
			}
		}
	}
	RoutingTable[0].ttl=TTL;
	for(int i=0;i<g.size();i++)
	{
		for(int j=0;j<g[i].size();j++)
		{
			if(g[i][j].dist<INF)
			{
				if(RoutingTable[getRoute(g[i][j].address)].cost>RoutingTable[getRoute(g[i][i].address)].cost + g[i][j].dist)
				{
					
					return false;
				}
			}
		}
	}
	
	return true;
}

void Send_Advertisement()
{
	char sendbuf[BUFFERSIZE],tempbuf[BUFFERSIZE];
	memset(sendbuf,0,sizeof(char)*BUFFERSIZE);
	bool is_first=1;
	
	for(int i=0;i<RoutingTable.size();i++)
	{
		if(is_first)
			sprintf(tempbuf,"%s,%d",RoutingTable[i].destination,RoutingTable[i].cost);
		else sprintf(tempbuf,",%s,%d",RoutingTable[i].destination,RoutingTable[i].cost);
		strcat(sendbuf,tempbuf);
	}
	//send with udp
	
	struct sockaddr_in host;
	int sockfd, len = 0;   
	int host_len = sizeof(struct sockaddr_in); 
	
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}	
	
	bzero(&host, sizeof(host));
	host.sin_family = AF_INET;
	host.sin_port = htons(portnum);
	
	for(int i=0;i<RoutingTable.size();i++)
	{
		if(RoutingTable[i].cost==1)
		{
			host.sin_addr.s_addr = inet_addr(RoutingTable[i].destination);
			if(sendto(sockfd, sendbuf,  strlen(sendbuf), 0, (struct sockaddr *)&host, host_len)<0) 
			{
			    printf("sendto error\n");
			    exit(1);
			}
		}
	}	
	close(sockfd);
	
	
}

struct MyReceive* Receive()
{
	struct MyReceive* ret;
	struct sockaddr_in server;
	int server_len = sizeof(struct sockaddr_in); 
	int recv_len=0,sockfd;
	char addr_p[30];
	char message[BUFFERSIZE];
	
	bzero(&server,sizeof(server));
	server.sin_family=AF_INET;
	server.sin_addr.s_addr=htonl(INADDR_ANY);
	server.sin_port=htons(portnum);
	
	if(-1 == (sockfd=socket(AF_INET, SOCK_DGRAM,0)))    
	{
	    perror("create socket failed");
	    exit (1);
	}	
	
	/*if(-1 == ( bind( sockfd, ( struct sockaddr * )&server, sizeof(server) )) )
	{
	    perror("bind error");
	    exit (1);    
	}*/

	recv_len=recvfrom(sockfd,message,sizeof(message),0,(struct sockaddr *)&server,(socklen_t*)&server_len); 
	if(recv_len <0)
	{
	   printf("recvfrom error\n");
	   exit(1);
	}
	
	inet_ntop(AF_INET,&server.sin_addr,addr_p,sizeof(addr_p));
	printf("client IP is %s, port is %d\n",addr_p,ntohs(server.sin_port));
	message[recv_len]='\0';
	
	printf("server received %d:%s\n", recv_len, message);
	close(sockfd);
	strcpy(ret->recvbuf,message);
	strcpy(ret->srcIP,addr_p);
	return ret;
}

void Initial()
{
	if(ReadConfigFile(configfile)<0)
	{
		cout<<"Config file open failed"<<endl;
		exit(1);
	}
	
	if(!Bellman_Ford(Gragh,0))
	{
		cout<<"Negative edge exist!"<<endl;
		exit(1);
	}
	
	Write_Up();
	Send_Advertisement();

}

void Detect_Failure()
{
	
	for(int i=0;i<RoutingTable.size();i++)
	{
		if(RoutingTable[i].ttl<=0)
		{
			RoutingTable[i].cost=TTL;
			Gragh[0][getDstNode(RoutingTable[i].destination)].dist=INF;
		}
	}
	
}

void Update(char revcbuf[],char srcIP[])
{
	
	for(int i=0;i<RoutingTable.size();i++)
	{
		RoutingTable[i].ttl-=period;
	}
	if(!revcbuf)
	{	
		Detect_Failure();
		if(!Bellman_Ford(Gragh,0))

		{
			cout<<"Negative edge exist!"<<endl;
			exit(1);
		}	
		Write_Up();
		Send_Advertisement();
		return;
	}
	char *temp=strtok(revcbuf,",");
	char ipaddr[BUFFERSIZE];
	int index,src_index,cost;
	Node node_t;
	Route_entry route_t;
	vector<Node> nodes_t;

	if(strcmp(srcIP,"")!=0)
	//if(srcIP)
	{
		src_index=getSrcNode(srcIP);
		if(src_index==-1)
		{
			node_t.dist=INF;		
			strcpy(node_t.address,ipaddr);

			strcpy(route_t.destination,ipaddr);
			strcpy(route_t.nexthop,srcIP);
			route_t.cost=1;
			route_t.ttl=TTL;
			RoutingTable.push_back(route_t);

			for(int i=0;i<Gragh.size();i++)
			{				
				Gragh[i].push_back(node_t);
			}
			for(int i=0;i<Gragh[0].size();i++)
			{
				strcpy(node_t.address,Gragh[0][i].address);
				node_t.dist=INF;
				nodes_t.push_back(node_t);
			}
			Gragh.push_back(nodes_t);
			Gragh[0][Gragh[0].size()-1].dist=1;
		}
	}

	while(temp)
    	{
        	memset(ipaddr,0,sizeof(ipaddr));		
		strcpy(ipaddr,temp);
		index=getDstNode(ipaddr);
		temp = strtok(NULL,",");
		cost=atoi(temp);
		
		if(index==-1)
		{
			if(cost>=INF)node_t.dist=INF;
			else node_t.dist=cost+1;
			strcpy(node_t.address,ipaddr);

			strcpy(route_t.destination,ipaddr);
			strcpy(route_t.nexthop,srcIP);
			route_t.cost=node_t.dist;
			route_t.ttl=TTL;
			RoutingTable.push_back(route_t);

			for(int i=0;i<Gragh.size();i++)
			{				
				Gragh[i].push_back(node_t);
			}
			for(int i=0;i<Gragh[0].size();i++)
			{
				strcpy(node_t.address,Gragh[0][i].address);
				node_t.dist=INF;
				nodes_t.push_back(node_t);
			}
			Gragh.push_back(nodes_t);
			
		}
		else 
		{
			Gragh[src_index][index].dist=cost;			
		}
       
		temp = strtok(NULL,",");
    	}
    	
	Detect_Failure();
	if(!Bellman_Ford(Gragh,0))
	{
		cout<<"Negative edge exist!"<<endl;
		exit(1);
	}	
	Write_Up();
	Send_Advertisement();
}

void *receive_thread_func(void *arg)
{
	struct MyReceive* ret;
	while(true)
	{
		ret=Receive();
		pthread_mutex_lock(&mutex1); 
		Update(ret->recvbuf,ret->srcIP);
		pthread_mutex_unlock(&mutex1); 
	}
	//pthread_exit();
}

int main(int argc,char** argv)
{
	if(argc!=7)
	{
		cout<<"Invalid command argument"<<endl;
		exit(1);
	}
	strcpy(configfile,argv[1]);
	portnum=atoi(argv[2]);
	TTL=atoi(argv[3]);
	INF=atoi(argv[4]);
	period=atoi(argv[5]);
	if(strcmp(argv[6],"0")==0)posion_reverse=0;
	else posion_reverse=1;
	Initial();
	
	int res;
	pthread_t recv_thread;	

	res = pthread_create(&recv_thread, NULL, receive_thread_func, NULL);
	if (res != 0)
	{
		perror("Thread creation failed!");
		exit(EXIT_FAILURE);
	}
	while(true)
	{
		//Update("","");
		pthread_mutex_unlock(&mutex1); 
		Update(NULL,NULL);
		pthread_mutex_unlock(&mutex1); 
		sleep(period);
	}
	
	
}
