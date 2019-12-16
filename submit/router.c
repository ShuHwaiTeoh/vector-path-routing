/*testing command:
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

pthread_mutex_t lock;

void init_pkt_request(struct pkt_INIT_REQUEST *pkt, unsigned int router_id) {
  //printf("[in] router id: %d\n", router_id);
  pkt->router_id = htonl(router_id);
  //printf("[in] router id: %d\n", pkt->router_id);
}

void init_pkt_response(struct pkt_INIT_RESPONSE *pkt) {
  pkt = (struct pkt_INIT_RESPONSE *)malloc(sizeof(struct pkt_INIT_RESPONSE));
}

char init_network_emulator_connection(struct sockaddr_in *serveraddr,
                                      char *ne_hostname, int ne_port,
                                      int *sockfd) {
  // entry of host database
  struct hostent *hp;

  // get ne information
  /* Fill in the server's IP address and port (host name to IP+port*/
  if ((hp = gethostbyname(ne_hostname)) == NULL) {
    fprintf(stderr, "fail to get host.\n");
    return EXIT_FAILURE;
  }

  if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
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

int main(int argc, char **argv) {
  if (argc < 5) {
    printf("Arguments are missing\n");
    printf(
        "router <router id> <ne hostname> <ne UDP port> <router UDP port>\n");
    exit(EXIT_FAILURE);
  }

  // 0~MAX_ROUTERS-1
  unsigned int routerID = (unsigned int)(strtoul(argv[1], NULL, 0));
  char *ne_hostname = argv[2];
  int ne_port = atoi(argv[3]);
  int routerPort = atoi(argv[4]);

  //unsigned int sourceAddrlen;
  int clientfd;//, flag = 0, i;

  struct pkt_INIT_REQUEST initRequest;
  struct pkt_INIT_RESPONSE initResponse;

  init_pkt_request(&initRequest, routerID);
  init_pkt_response(&initResponse);

  /* struct pkt_RT_UPDATE updatePkt; */
  /* int failTimer[MAX_ROUTERS] = {0};  // timer for each neighbor */
  /* int convergeTimer = 0; */
  /* int selfUpdateTimer = 0; */
  /* FILE *Logfile; */
  /* /\* ----- INIT THREADS ----- *\/ */
  /* pthread_t UDP_thread_id; */
  /* pthread_t timer_thread_id; */

  /* // init lock */
  /* pthread_mutex_init(&lock, NULL); */
  /* /\* ----- INIT THREADS ----- *\/ */
  /* if (pthread_create(&UDP_thread_id, NULL, UpdateRoutes, NULL)) { */
  /*   perror("Error creating thread for UpdateRoutes!"); */
  /*   return EXIT_FAILURE; */
  /* } */
  /* if (pthread_create(&timer_thread_id, NULL, ConvertTabletoPkt, NULL)) { */
  /*   perror("Error creating thread for ConvertTabletoPkt!"); */
  /*   return EXIT_FAILURE; */
  /* } */
  /* if (pthread_create(&timer_thread_id, NULL, UninstallRoutesOnNbrDeath,
   * NULL)) { */
  /*   perror("Error creating thread for UninstallRoutesOnNbrDeath!"); */
  /*   return EXIT_FAILURE; */
  /* } */
  /* /\* ----- WAIT FOR THREADS TO FINISH ----- *\/ */
  /* pthread_join(UDP_thread_id, NULL); */
  /* pthread_join(timer_thread_id, NULL); */

  // store address of internet protocol family
  struct sockaddr_in network_emulator_info = {};

  if (init_network_emulator_connection(&network_emulator_info, ne_hostname,
                                       ne_port, &clientfd) != EXIT_SUCCESS) {
    printf("Failed to init udp server binding port %d\n", routerPort);
    exit(EXIT_FAILURE);
  }

  ssize_t err_num = sendto(clientfd, &initRequest, sizeof(initRequest), 0,
                           (struct sockaddr *)&network_emulator_info,
                           sizeof(struct sockaddr_in));

  if (err_num < 0) {
    fprintf(stderr, "fail to send init request.\nerr_no %zd\n", err_num);
    printf("error sending alive message: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  socklen_t init_response_length = sizeof(network_emulator_info);

  err_num = recvfrom(clientfd, &initResponse, sizeof(initResponse), 0,
                     (struct sockaddr *)&network_emulator_info,
                     &init_response_length);

  if (err_num < 0) {
    fprintf(stderr, "fail to receive init request.\nerr_no %zd\n", err_num);
    exit(1);
  }

  /* clock_t before = clock(); */
  ntoh_pkt_INIT_RESPONSE(&initResponse);

  // Initialize rounting entry of router
  InitRoutingTbl(&initResponse, routerID);

  char filename[15];
  FILE *Logfile;
  struct pkt_RT_UPDATE resultpkt;

  // create file name of log file
  strcpy(filename, "router");
  strcat(filename, argv[1]);
  strcat(filename, ".log");

  //write initialized table
  ConvertTabletoPkt(&resultpkt, routerID);
  Logfile = fopen(filename, "w");
  //fprintf(Logfile, "Routing Table:\n");
  PrintRoutes(Logfile, routerID);
  fclose(Logfile);

  // FIXME remove the two function after testing is done

  /* struct pkt_RT_UPDATE resultpkt; */
  /* ConvertTabletoPkt(&resultpkt, routerID); */
  /* printPacket(&resultpkt); */

  /* while (1) { */
  /*   // wait to receive update packet */
  /*   if (recvfrom(clientfd, updatePkt, strlen(updatePkt), 0, */
  /*                (struct sockaddr *)&sourceAddr, &sourceAddrlen) { */
  /*         ntoh_pkt_RT_UPDATE(updatePkt); */
  /*         // updates update-timer for each neighbor */
  /*         failTimer[updatePkt.sender_id] = */
  /*             (clock() - before - failTimer[updatePkt.sender_id]) / */
  /*             CLOCK_PER_SEC; */
  /*         // updates routes and converge-timer */
  /*         if (UpdateRoutes(updatePkt, routingTable[updatePkt.sender_id].cost,
   */
  /*                          routerID)) { */
  /*           convergeTimer = (clock() - before - convergeTimer) /
   * CLOCK_PER_SEC; */
  /*           Logfile = fopen(filename, "w"); */
  /*           PrintRoutes(Logfile, routerID); */
  /*           fclose(Logfile); */
  /*         } */
  /*       } */
  /*       // if update interval expires, send update-packet to neighbors */
  /*       selfUpdateTimer = (clock() - before - convergeTimer) / CLOCK_PER_SEC)
   */
  /*     ; */
  /*   if (selfUpdateTimer > CONVERGE_TIMEOUT) { */
  /*     ConvertTabletoPkt(updatePkt, routerID); */
  /*     if (sendto(clientfd, updatePkt, strlen(updatePkt), 0, */
  /*                (struct sockaddr *)&serveraddr, */
  /*                sizeof(struct sockaddr_in)) < 0) */
  /*       error("send update pkt"); */
  /*   } */
  /*   // check dead */
  /*   for (i = 0; i < NumRoutes; i++) { */
  /*     if (failTimer[i] > FAILURE_DETECTION) UninstallRoutesOnNbrDeath(i); */
  /*   } */
  /*   // check convergence */
  /*   if (selfUpdateTimer > CONVERGE_TIMEOUT) { */
  /*     Logfile = fopen(filename, "w"); */
  /*     PrintRoutes(Logfile, routerID); */
  /*     i = (clock() - before) / CLOCK_PER_SEC; */
  /*     fprintf(Logfile, "%d:Converged", i); */
  /*     fclose(Logfile); */
  /*   } */
  /* } */
  close(clientfd);
  exit(0);
}
