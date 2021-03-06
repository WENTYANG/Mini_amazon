#ifndef _WORKING_THREADS_H
#define _WORKING_THREADS_H
#include "./protobuf/AUprotocolV4.pb.h"
#include "./protobuf/world_amazon.pb.h"
#include "sql_functions.h"
#include "warehouse.h"

// readOrder() also use threadPool

// for communication with world
void sendAck_world(AResponses response);
void purchase_more(APurchaseMore apm);
void load_world(APacked aped);
void ready_to_deliver(ALoaded aled);
void pack(shared_ptr<SubOrder> order, int w_id);
void order_truck(shared_ptr<SubOrder> order, int w_idx);

// for communication with UPS
void sendAck_ups(UACommand response);
void load_ups(UTruckArrive uta);
void delivered(UDelivered uded);
#endif
