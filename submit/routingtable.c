#include "ne.h"
#include "router.h"

/* ----- GLOBAL VARIABLES ----- */
struct route_entry routingTable[MAX_ROUTERS];
int NumRoutes;

////////////////////////////////////////////////////////////////
void InitRoutingTbl(struct pkt_INIT_RESPONSE *InitResponse, int myID) {
  unsigned int i, neighbor_id;
  NumRoutes = 0;
  // initialize for all entries

  for (i = 0; i < MAX_ROUTERS; i++) {
    routingTable[i].dest_id = INFINITY;
    routingTable[i].next_hop = INFINITY;
    routingTable[i].cost = INFINITY;
    routingTable[i].path_len = 0;
    routingTable[i].path[0] = myID;
  }
  // initialize for entry of myID
  routingTable[NumRoutes].dest_id = myID;
  routingTable[NumRoutes].next_hop = myID;
  routingTable[NumRoutes].cost = 0;
  routingTable[NumRoutes].path_len = 1;
  routingTable[NumRoutes].path[0] = myID;
  NumRoutes++;

  // initialize for entries of neighbors
  for (i = 0; i < InitResponse->no_nbr; i++) {
    neighbor_id = InitResponse->nbrcost[i].nbr;
    if (neighbor_id == myID) continue;

    routingTable[NumRoutes].dest_id = neighbor_id;
    routingTable[NumRoutes].next_hop = neighbor_id;
    routingTable[NumRoutes].cost = InitResponse->nbrcost[i].cost;
    routingTable[NumRoutes].path_len = 2;
    routingTable[NumRoutes].path[0] = myID;
    routingTable[NumRoutes].path[1] = neighbor_id;
    NumRoutes++;
  }
}

////////////////////////////////////////////////////////////////
// if myID is in the path from sender to destination, ignore update,
// else update with forced update rule and split horizon rule
int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr,
                 int myID) {
  int i, j, k, split_horizon_check = 0, inPathFlag, changeFlag = 0,
               temp_routing_idx;
  unsigned int c, sender_id, dest_id;

  sender_id = RecvdUpdatePacket->sender_id;

  if (myID != RecvdUpdatePacket->dest_id) return changeFlag;

  // // pthread_mutex_lock(&lock);
  for (i = 0; i < RecvdUpdatePacket->no_routes; i++) {
    dest_id = RecvdUpdatePacket->route[i].dest_id;

    if (dest_id == myID || dest_id == sender_id) continue;

    // check whether myID is in the path
    inPathFlag = 0;
    for (j = 0; j < RecvdUpdatePacket->route[i].path_len; j++) {
      if (RecvdUpdatePacket->route[i].path[j] != myID) continue;
      inPathFlag = 1;
      break;
    }

    if (inPathFlag) continue;
    temp_routing_idx = NumRoutes;
    for (j = 0; j < NumRoutes; j++) {
      if (routingTable[j].dest_id != dest_id) continue;
      temp_routing_idx = j;
      break;
    }
    // New record, increase NumRoutes by 1
    if (temp_routing_idx == NumRoutes) NumRoutes++;

    // cost from myID to destination with first hop is sender
    c = costToNbr + RecvdUpdatePacket->route[i].cost;
    split_horizon_check = c < routingTable[temp_routing_idx].cost &&
                          RecvdUpdatePacket->route[i].next_hop != myID;

    if ((routingTable[temp_routing_idx].next_hop ==
         sender_id) ||            // forced update rule
        (split_horizon_check)) {  // split horizon rule

      routingTable[temp_routing_idx].dest_id = dest_id;
      routingTable[temp_routing_idx].next_hop = sender_id;
      routingTable[temp_routing_idx].cost = c;
      routingTable[temp_routing_idx].path_len =
          RecvdUpdatePacket->route[i].path_len + 1;

      routingTable[temp_routing_idx].path[0] = myID;
      for (k = 1; k <= RecvdUpdatePacket->route[i].path_len; k++) {
        routingTable[temp_routing_idx].path[k] =
            RecvdUpdatePacket->route[i].path[k - 1];
      }
      changeFlag = 1;
    }
  }

  // pthread_mutex_unlock(&lock);
  return changeFlag;
}

////////////////////////////////////////////////////////////////
void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID) {
  int i, j;

  // pthread_mutex_lock(&lock);
  UpdatePacketToSend->sender_id = myID;
  UpdatePacketToSend->no_routes = NumRoutes;
  for (i = 0; i < NumRoutes; i++) {
    UpdatePacketToSend->route[i].dest_id = routingTable[i].dest_id;
    UpdatePacketToSend->route[i].next_hop = routingTable[i].next_hop;
    UpdatePacketToSend->route[i].cost = routingTable[i].cost;
    UpdatePacketToSend->route[i].path_len = routingTable[i].path_len;
    for (j = 0; j < UpdatePacketToSend->route[i].path_len; j++) {
      UpdatePacketToSend->route[i].path[j] = routingTable[i].path[j];
    }
  }
  // pthread_mutex_unlock(&lock);
  return;
}

////////////////////////////////////////////////////////////////
// It is highly recommended that you do not change this function!
void PrintRoutes(FILE *Logfile, int myID) {
  /* ----- PRINT ALL ROUTES TO LOG FILE ----- */
  int i;
  int j;
  for (i = 0; i < NumRoutes; i++) {
    fprintf(Logfile, "<R%d -> R%d> Path: R%d", myID, routingTable[i].dest_id,
            myID);

    /* ----- PRINT PATH VECTOR ----- */
    for (j = 1; j < routingTable[i].path_len; j++) {
      fprintf(Logfile, " -> R%d", routingTable[i].path[j]);
    }
    fprintf(Logfile, ", Cost: %d\n", routingTable[i].cost);
  }
  fprintf(Logfile, "\n");
  fflush(Logfile);
}

////////////////////////////////////////////////////////////////
void UninstallRoutesOnNbrDeath(int DeadNbr) {
  int i;
  // pthread_mutex_lock(&lock);
  for (i = 0; i < NumRoutes; i++) {
    if (routingTable[i].next_hop == DeadNbr) {
      routingTable[i].cost = INFINITY;
    }
  }
  // pthread_mutex_unlock(&lock);
}
