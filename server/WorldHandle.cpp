#include "WorldHandle.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <mutex>
#include <string>
#include <vector>

#include "./protobuf/AUprotocolV3.pb.h"
#include "./protobuf/world_amazon.pb.h"
#include "ThreadSafe_queue.h"
#include "exception.h"
#include "server.h"
#include "sql_functions.h"
#include "working_threads.h"

using namespace std;
#define SEND_BATCH 3

// use a global unordered_set to keep track of all seq_number we received
// Guarantee an idiompotent behaviour
unordered_set<int64_t> seq_nums;

// send an ack for every message with an seqnum
<<<<<<< HEAD
bool sendAck(AResponses& response) {
    ACommands cmd;
    for (int i = 0; i < response.arrived_size(); ++i) {
        int64_t seq = response.arrived(i).seqnum();
        cmd.add_acks(seq);
    }
    for (int i = 0; i < response.ready_size(); ++i) {
        int64_t seq = response.ready(i).seqnum();
        cmd.add_acks(seq);
    }
    for (int i = 0; i < response.loaded_size(); ++i) {
        int64_t seq = response.loaded(i).seqnum();
        cmd.add_acks(seq);
    }
    for (int i = 0; i < response.error_size(); ++i) {
        int64_t seq = response.error(i).seqnum();
        cmd.add_acks(seq);
    }
    for (int i = 0; i < response.packagestatus_size(); ++i) {
        int64_t seq = response.packagestatus(i).seqnum();
        cmd.add_acks(seq);
    }
    Server& s = Server::get_instance();
    s.world_output_queue.push(cmd);
    return true;
}
=======
>>>>>>> be208320dec1ee056067f65d6be3d953980314f1

void RecvFromWorld(proto_in* world_in) {
    while (1) {
        try {
            AResponses response;
            if (recvMesgFrom<AResponses>(response, world_in) == false) {
                throw MyException(
                    "Error occured when receiving AResponse from World");
            }
            // Parse AResponses and handle
            cout << "Received from world: " << response.DebugString()
                 << std::endl;
            Server& s = Server::get_instance();
<<<<<<< HEAD
            sendAck(response);
            for (int i = 0; i < response.arrived_size(); ++i) {
                // update database, add more inventory
                const APurchaseMore& apm = response.arrived(i);
                int64_t seq = apm.seqnum();
                if (seq_nums.find(seq) != seq_nums.end()) {
                    continue;
                } else {
                    seq_nums.insert(seq);
                }
                int whnum = apm.whnum();
                for (int j = 0; j < apm.things_size(); ++j) {
                    const AProduct& product = apm.things(j);
                    add_inventory(whnum, (int)product.id(), product.count());
                }
            }
            for (int i = 0; i < response.ready_size(); ++i) {
                // updata database and check if ups truck also arrived, if
                // arrived, spawn load thread
                const APacked& aped = response.ready(i);
                int64_t seq = aped.seqnum();
                if (seq_nums.find(seq) != seq_nums.end()) {
                    continue;
                } else {
                    seq_nums.insert(seq);
                }
                bool truck_arrived = false;
                try {
                    truck_arrived = packed_and_check_ups_truck(aped.shipid());
                } catch (MyException& e) {
                    cout << e.what() << endl;
                    continue;
                }
                if (truck_arrived) {
                    int i_id = aped.shipid();
                    s.threadPool->assign_task(bind(load, i_id));
                }
            }
            for (int i = 0; i < response.loaded_size(); ++i) {
                // update database and spwan ready to deliver thread
                const ALoaded& aled = response.loaded(i);
                int64_t seq = aled.seqnum();
                if (seq_nums.find(seq) != seq_nums.end()) {
                    continue;
                } else {
                    seq_nums.insert(seq);
                }
                try {
                    change_status_to_delivering(aled.shipid());
                } catch (MyException& e) {
                    cout << e.what() << endl;
                    continue;
                }
                int i_id = aled.shipid();
                s.threadPool->assign_task(bind(ready_to_deliver, i_id));
=======
            s.threadPool->assign_task(bind(sendAck, response));
            for (int i = 0; i < response.arrived_size(); ++i) {
              // update database, add more inventory
              const APurchaseMore& apm = response.arrived(i);
              int64_t seq = apm.seqnum();
              if (seq_nums.find(seq) != seq_nums.end()) {
                continue;
              } else {
                seq_nums.insert(seq);
              }
              s.threadPool->assign_task(bind(purchase_more, apm));
            }
            for (int i = 0; i < response.ready_size(); ++i) {
              // updata database and check if ups truck also arrived, if arrived, spawn load thread
              const APacked& aped = response.ready(i);
              int64_t seq = aped.seqnum();
              if (seq_nums.find(seq) != seq_nums.end()) {
                continue;
              } else {
                seq_nums.insert(seq);
              }
              s.threadPool->assign_task(bind(load, aped));
            }
            for (int i = 0; i < response.loaded_size(); ++i) {
              // update database and spwan ready to deliver thread
              const ALoaded& aled = response.loaded(i);
              int64_t seq = aled.seqnum();
              if (seq_nums.find(seq) != seq_nums.end()) {
                continue;
              } else {
                seq_nums.insert(seq);
              }
              s.threadPool->assign_task(bind(ready_to_deliver, aled));
>>>>>>> be208320dec1ee056067f65d6be3d953980314f1
            }
            if (response.has_finished()) {
                // TODO: exit?
                cout << "World has finished processing all commands and is "
                        "ready to quit.\n";
            }
            for (int i = 0; i < response.error_size(); ++i) {
                // TODO: Just print out error info for now
                const AErr& aerr = response.error(i);
                cerr << "Got an error message for seq=" << aerr.originseqnum()
                     << " msg: " << aerr.err() << endl;
            }
            for (int i = 0; i < response.acks_size(); ++i) {
<<<<<<< HEAD
                int64_t ack = response.acks(i);
                // TODO: update ack,Timer map
=======
              int64_t ack = response.acks(i);
              // TODO: update ack, Timer map
>>>>>>> be208320dec1ee056067f65d6be3d953980314f1
            }
            for (int i = 0; i < response.packagestatus_size(); ++i) {
                // TODO:Currently just printing out info
                // Our server should never sent Aquery and never receive
                // APackage
                const APackage& ps = response.packagestatus(i);
                cout << "Get package status for package id=" << ps.packageid()
                     << " status=" << ps.status() << endl;
            }

        } catch (const std::exception& e) {
            cerr << e.what() << endl;
        }
    }
}

/*
  Dedicated output thread to pop requests out of world queue and send to world
  Send at most SEND_BATCH requests in 1 ACommands at a time
*/
void sendToWorld() {
    Server& s = Server::get_instance();
    ThreadSafe_queue<ACommands>& que = s.world_output_queue;
    proto_out* world_out = s.world_out;
    while (1) {
        try {
            ACommands cToSend;
            for (int j = 0; j < SEND_BATCH; j++) {
                if (que.empty()) {
                    break;
                }
                // pop
                ACommands request;
                que.try_pop(request);
                // parse & add seqnum & append to commandToSend
                for (int i = 0; i < request.buy_size(); ++i) {
                    const APurchaseMore& currBuy = request.buy(i);
                    APurchaseMore* buy = cToSend.add_buy();
                    buy->CopyFrom(currBuy);
                    buy->set_seqnum(s.getSeqNum());
                }
                for (int i = 0; i < request.topack_size(); ++i) {
                    const APack& currPack = request.topack(i);
                    APack* topack = cToSend.add_topack();
                    topack->CopyFrom(currPack);
                    topack->set_seqnum(s.getSeqNum());
                }
                for (int i = 0; i < request.load_size(); ++i) {
                    const APutOnTruck& currLoad = request.load(i);
                    APutOnTruck* load = cToSend.add_load();
                    load->CopyFrom(currLoad);
                    load->set_seqnum(s.getSeqNum());
                }
                for (int i = 0; i < request.queries_size(); ++i) {
                    const AQuery& currQuery = request.queries(i);
                    AQuery* queries = cToSend.add_queries();
                    queries->CopyFrom(currQuery);
                    queries->set_seqnum(s.getSeqNum());
                }
                for (int i = 0; i < request.acks_size(); ++i) {
                    cToSend.add_acks(request.acks(i));
                }
                if (request.has_disconnect()) {
                    cToSend.set_disconnect(request.disconnect());
                }
                if (request.has_simspeed()) {
                    cToSend.set_simspeed(request.simspeed());
                }
            }
            if (cToSend.buy_size() > 0 || cToSend.topack_size() > 0 ||
                cToSend.load_size() > 0 || cToSend.queries_size() > 0 ||
                cToSend.acks_size() > 0 || cToSend.has_disconnect() ||
                cToSend.has_simspeed()) {
                // send
                if (sendMesgTo<ACommands>(cToSend, world_out) == false) {
                    throw MyException(
                        "sendToWorld thread: Error occured when sending "
                        "ACommands "
                        "to World.");
                }
            }
        } catch (const std::exception& e) {
            cerr << e.what() << endl;
        }
    }
}
