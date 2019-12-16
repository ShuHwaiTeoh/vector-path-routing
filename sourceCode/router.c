/*testing command:/
Network Emulator:
./ne 2100 4_routers.conf
Routers:
./router 0 localhost 2100 3100
./router 1 localhost 2100 4100
./router 2 localhost 2100 5100
./router 3 localhost 2100 6100
# compile with:
gcc -o router router.c -lpthread
*/

#include "router.h"
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "ne.h"

unsigned int routerID;
char filename[15];
FILE *Logfile;
int clientfd;
struct pkt_INIT_RESPONSE initResponse;
struct sockaddr_in network_emulator_info = {};
pthread_mutex_t lock;
int iniTime;
struct pkt_RT_UPDATE updatePkt;
int failTimer[MAX_ROUTERS]; // timer for each neighbor
int convergeTimer;
int selfUpdateTimer =0;
int convergeFlag = 0;

void printPacket(struct pkt_RT_UPDATE *updpkt) {
  printf("******************************\n");
  printf("sender id: %d \nno. routers: %d\n", updpkt->sender_id,
         updpkt->no_routes);

  int i, j;
  for (i = 0; i < updpkt->no_routes; i++) {
    printf("Route[%d]:\n", i);
    printf("dest id: %d\nnext hop: %d\ncost: %d\npath len: %d\n",
           updpkt->route[i].dest_id, updpkt->route[i].next_hop,
           updpkt->route[i].cost, updpkt->route[i].path_len);
    // memcpy(UpdatePacketToSend->route[i].path, routingTable[i].path,
    // MAX_ROUTERS);
    printf("path: ");
    for (j = 0; j < updpkt->route[i].path_len - 1; j++) {
      printf("R%d ->", updpkt->route[i].path[j]);
    }
    printf("R%d\n", updpkt->route[i].path[updpkt->route[i].path_len - 1]);
    printf("--------------------\n");
  }
  printf("******************************\n");
}

void *UpdateHandel(void *arg)
{
  while (1)
  {
    pthread_mutex_lock(&lock);
    //wait for update packet
    socklen_t addrlen = sizeof(network_emulator_info);
    ssize_t errnum = recvfrom(clientfd, &updatePkt, sizeof(updatePkt), 0,
                              (struct sockaddr *)&network_emulator_info,
                              &addrlen);
    if (errnum < 0)
    {
      pthread_mutex_unlock(&lock);
      continue;
    }
    ntoh_pkt_RT_UPDATE(&updatePkt);
    //Fix
    //printf("%d\n",updatePkt.sender_id);
    //Fixme
    //printPacket(&updatePkt);
    failTimer[updatePkt.sender_id] = time(NULL);

    int i = 0;
    while (initResponse.nbrcost[i].nbr != updatePkt.sender_id)
      i++;
    unsigned int cost2neighboor = initResponse.nbrcost[i].cost;
    i = UpdateRoutes(&updatePkt, cost2neighboor, routerID);
    if (i==1)
    {
      // updates routes and converge-timer
      convergeFlag = 0;
      convergeTimer = time(NULL);
      Logfile = fopen(filename, "a+");
      PrintRoutes(Logfile, routerID);
      fflush(Logfile);
      fclose(Logfile);
    }
    pthread_mutex_unlock(&lock);
    }

  return NULL;
}

void *TimeOutHandel(void *arg)
{
  int i,j;
  while (1)
  {

    pthread_mutex_lock(&lock);
    // if update interval expires, send update-packet to neighbors
    if ((time(NULL)-selfUpdateTimer) >= UPDATE_INTERVAL)
      {
printf("00000000000000000000000000");
	ConvertTabletoPkt(&updatePkt, routerID);
	for(j=0; j<initResponse.no_nbr; j++){
	  if (initResponse.nbrcost[j].cost !=INFINITY){
	    updatePkt.dest_id = initResponse.nbrcost[j].nbr;
	    hton_pkt_RT_UPDATE(&updatePkt);
	    ssize_t err_num = sendto(clientfd, &updatePkt, sizeof(updatePkt), 0,
				     (struct sockaddr *)&network_emulator_info, sizeof(struct sockaddr_in));

	    if (err_num < 0)
	      {
		fprintf(stderr, "fail to send update pkt.\nerr_no %zd\n", err_num);
		exit(1);
	      }
	    ntoh_pkt_RT_UPDATE(&updatePkt);
	  }
	}
	selfUpdateTimer = time(NULL);
      }
    // check dead
    for (i = 0; i < initResponse.no_nbr; i++)
    {
      unsigned int neighborID = initResponse.nbrcost[i].nbr;
      int t = ((time(NULL)-failTimer[neighborID]));
      if (t >= FAILURE_DETECTION){
        UninstallRoutesOnNbrDeath(neighborID);
	failTimer[neighborID] = 0;
      }
    }
    // check convergence
    //printf("%f\n",(double)(clock()-convergeTimer)/CLOCKS_PER_SEC);
    if ((time(NULL)-convergeTimer)> CONVERGE_TIMEOUT && convergeFlag == 0)
    {
      convergeFlag = 1;
      Logfile = fopen(filename, "a+");
      //PrintRoutes(Logfile, routerID);
      i = time(NULL) - iniTime;
      fprintf(Logfile, "%d:Converged\n", i);
      fflush(Logfile);
      fclose(Logfile);
      convergeTimer = time(NULL);
    }
    pthread_mutex_unlock(&lock);
    }
  return NULL;
}

void init_pkt_update(struct pkt_RT_UPDATE *pkt)
{
  pkt = (struct pkt_RT_UPDATE *)malloc(sizeof(struct pkt_RT_UPDATE));
}

void init_pkt_request(struct pkt_INIT_REQUEST *pkt, unsigned int router_id)
{
  //printf("[in] router id: %d\n", router_id);
  pkt->router_id = htonl(router_id);
  //printf("[in] router id: %d\n", pkt->router_id);
}

void init_pkt_response(struct pkt_INIT_RESPONSE *pkt)
{
  pkt = (struct pkt_INIT_RESPONSE *)malloc(sizeof(struct pkt_INIT_RESPONSE));
}

char init_network_emulator_connection(struct sockaddr_in *serveraddr,
                                      char *ne_hostname, int ne_port,
                                      int *sockfd)
{
  // entry of host database
  struct hostent *hp;

  // get ne information
  /* Fill in the server's IP address and port (host name to IP+port*/
  if ((hp = gethostbyname(ne_hostname)) == NULL)
  {
    fprintf(stderr, "fail to get host.\n");
    return EXIT_FAILURE;
  }

  if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("socket creation failed");
    return EXIT_FAILURE;
  }

  // fill with zeros
  bzero(serveraddr, sizeof(*serveraddr));

  // IPv4
  serveraddr->sin_family = AF_INET;
  // copy h_addr to s_addr
  bcopy((char *)hp->h_addr, (char *)&serveraddr->sin_addr.s_addr, hp->h_length);
  // convert host byte order to network byte order
  serveraddr->sin_port = htons(ne_port);

  return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
  if (argc < 5)
  {
    printf("Arguments are missing\n");
    printf(
        "router <router id> <ne hostname> <ne UDP port> <router UDP port>\n");
    exit(EXIT_FAILURE);
  }

  // init lock
  pthread_mutex_init(&lock, NULL);
  pthread_mutex_lock(&lock);

  routerID = (unsigned int)(strtoul(argv[1], NULL, 0));
  char *ne_hostname = argv[2];
  int ne_port = atoi(argv[3]);
  int routerPort = atoi(argv[4]);

  struct pkt_INIT_REQUEST initRequest;
  init_pkt_request(&initRequest, routerID);
  init_pkt_response(&initResponse);

  // store address of internet protocol family
  if (init_network_emulator_connection(&network_emulator_info, ne_hostname,
                                       ne_port, &clientfd) != EXIT_SUCCESS)
  {
    printf("Failed to init udp server binding port %d\n", routerPort);
    exit(EXIT_FAILURE);
  }

  //send init request
  ssize_t err_num = sendto(clientfd, &initRequest, sizeof(initRequest), 0,
                           (struct sockaddr *)&network_emulator_info,
                           sizeof(struct sockaddr_in));
  if (err_num < 0)
  {
    fprintf(stderr, "fail to send init request.\nerr_no %zd\n", err_num);
    printf("error sending alive message: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  //receive init response
  socklen_t ne_addr_length = sizeof(network_emulator_info);

  err_num = recvfrom(clientfd, &initResponse, sizeof(initResponse), 0,
                     (struct sockaddr *)&network_emulator_info, &ne_addr_length);
  if (err_num < 0)
  {
    fprintf(stderr, "fail to receive init request.\nerr_no %zd\n", err_num);
    exit(1);
  }
  //keep init time for convergence time calculation
  iniTime = time(NULL);
  ntoh_pkt_INIT_RESPONSE(&initResponse);

  // Initialize rounting entry of router
  InitRoutingTbl(&initResponse, routerID);

  // create file name of log file
  strcpy(filename, "router");
  strcat(filename, argv[1]);
  strcat(filename, ".log");

  //write initialized table
  Logfile = fopen(filename, "w");
  PrintRoutes(Logfile, routerID);
  fflush(Logfile);
  fclose(Logfile);

  init_pkt_update(&updatePkt);
  /* ----- INIT THREADS ----- */
  pthread_t UDP_thread_id;
  pthread_t timer_thread_id;
  convergeTimer = 0;
  selfUpdateTimer = 0;

  //init failTimer array
  int i;
  for (i = 0; i < MAX_ROUTERS; i++)
  {
    failTimer[i] = 0;
  }

  pthread_mutex_unlock(&lock);
  /*------ CREATE THREADS------*/
  if (pthread_create(&timer_thread_id, NULL, TimeOutHandel, NULL))
  {
    perror("Error creating thread for ConvertTabletoPkt!");
    return EXIT_FAILURE;
  }
  if (pthread_create(&UDP_thread_id, NULL, UpdateHandel, NULL))
  {
    perror("Error creating thread for UpdateRoutes!");
    return EXIT_FAILURE;
  }

  /* ----- WAIT FOR THREADS TO FINISH ----- */
  pthread_join(UDP_thread_id, NULL);
  pthread_join(timer_thread_id, NULL);
  

  close(clientfd);
  exit(0);
}
