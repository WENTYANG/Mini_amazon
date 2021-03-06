#include "server.h"
#include <cmath>
#include <cstdlib>
#include <thread>
#include "UpsHandle.h"
#include "WorldHandle.h"
#include "proto.h"
#include "socket.h"

#define PRODUCT_INITIAL_AMOUNT 10
#define fzq_host "vcm-24717.vm.duke.edu"
#define my_host "vcm-24273.vm.duke.edu"

using namespace std;

/*-----------------------------Server constructor-----------------------------*/
Server::Server()
    : frontHostName("0.0.0.0"),
      frontPortNum("2104"),
      // worldHostName("vcm-24717.vm.duke.edu"),
      worldPortNum("23456"),
      // upsHostName("vcm-24717.vm.duke.edu"),
      upsPortNum("8888"),
      seqNum(0),
      dbName("MINI_AMAZON"),
      userName("postgres"),
      password("passw0rd"),
      withUPS(true),
      withFrontEnd(true) {
    cout << "Initializing server configuration...." << endl;

    // Initialize threadpool
    threadPool = threadPoolObj.get_pool();
    cout << "Initialized thread pool\n";

    // Init frontend_fd
    try {
        frontend_fd = initializeServer(frontPortNum);
        cout << "Listening for connections from front end." << endl;
    } catch (const std::exception& e) {
        std::cerr << "Fail to listen on front end because " << e.what() << endl;
    }
}

Server::~Server() {
    close(frontend_fd);
    close(ups_fd);
    close(world_fd);
}

/*-----------------------------Server run-----------------------------------*/
void Server::run(string ups_host) {
    try {
        // init from DB
        if (withFrontEnd) {
            cout << "WithFrontEnd==True.\n";
            while (1) {
                try {
                    initFromDB();
                    cout << "Initialized from DB.\n";
                    break;
                } catch (Uninitialize& e) {
                    cout << e.what() << endl;
                    continue;
                } catch (std::exception& e) {
                    cerr << e.what() << endl;
                    return;
                }
            }
        } else {
            num_wh = 5;
        }

        // get worldHostName and upsHostName from env variable
        worldHostName = ups_host;
        upsHostName = ups_host;
        cout << "ups host=" << ups_host << endl;
        // Connect to UPS, receive world ID
        if (withUPS) {
            connectUPS();
            // Spawn threads to receive responses from ups and world
            thread t_RecvFromUps(RecvFromUps, ups_in);
            cout << "Thread RecvFromUps created\n";
            thread t_SendToUps(SendToUps, ups_out);
            cout << "Thread SendToUps created\n";
            t_RecvFromUps.detach();
            t_SendToUps.detach();
        }
        // Connect to world, when developing, set withUPS=false to initialize a
        // new world
        connectWorld();
        cout << "World connected\n";

        thread t_RecvFromWorld(RecvFromWorld, world_in);
        cout << "Thread RecvFromWorld created\n";
        thread t_SendToWorld(SendToWorld, world_out);
        cout << "Thread SendToWorld created\n";

        t_RecvFromWorld.detach();
        cout << "Thread RecvFromWorld detached\n";
        t_SendToWorld.detach();
        cout << "Thread SendToWorld detached\n";

        // Spawn a thread for each warehouse to process incoming orders
        for (int i = 0; i < num_wh; i++) {
            cout << "the " << i << " warehouse of " << num_wh << endl;
            cout << "whlist[i] = " << whlist[i]->w_id << endl;
            thread t_checkOrder(checkOrder, i);
            t_checkOrder.detach();
            cout << "Spawned one thread to checkOrder\n";
        }
        cout << "CheckOrder thread assigned\n";
        // Accept order from front end
        cout << "Starting to acceptOrder.\n";
        acceptOrder();
    } catch (const std::exception& e) {
        std::cerr << e.what() << endl;
    }
}

/*--------------------------Server connect----------------------------------*/
/*
  Connect to UPS and receive worldID
*/
void Server::connectUPS() {
    ups_fd = clientRequestConnection(upsHostName, upsPortNum);
    string world_id = socketRecvMsg(ups_fd);

    cout << "Receiving: " << world_id << endl;
    worldID = stoi(world_id);
    cout << "Receiving world id = " << worldID << " from UPS" << endl;

    ups_in = new proto_in(ups_fd);
    ups_out = new proto_out(ups_fd);
}

/*
  Connect to world server.
  When withUPS=false, init a new world, else connect using the worldID.
*/
void Server::connectWorld() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;  // use macro to check environment

    // connect to world
    try {
        world_fd = clientRequestConnection(worldHostName, worldPortNum);
    } catch (const std::exception& e) {
        std::cerr << "Fail to connect to world because " << e.what() << endl;
    }

    // build AConnect Command
    AConnect acon;
    if (withUPS) {
        acon.set_worldid(worldID);
    } else {
    }

    // Initialize warehouses
    if (withFrontEnd) {
        for (auto const& warehouse : whlist) {
            AInitWarehouse* initWarehouse = acon.add_initwh();
            initWarehouse->set_id(warehouse.get()->w_id);
            initWarehouse->set_x(warehouse.get()->x);
            initWarehouse->set_y(warehouse.get()->y);
        }
    } else {
        // setWh_circle(acon);
    }

    acon.set_isamazon(true);

    // send AConnect Command
    world_out = new proto_out(world_fd);
    if (sendMesgTo<AConnect>(acon, world_out) == false) {
        throw MyException("Send AConnect failed.");
    }
    cout << "Sent AConnect Command: " << acon.DebugString() << endl;
    // Receive Aconnected Response
    AConnected aced;
    world_in = new proto_in(world_fd);
    if (recvMesgFrom<AConnected>(aced, world_in) == false) {
        throw MyException("Receive AConnected failed.");
    }
    cout << "Received AConnected Command: " << aced.DebugString() << endl;

    // Verify Aconnected Response
    if (aced.result() != "connected!") {
        string description = "Connect to world failed " + aced.result();
        throw MyException(description);
    }
    worldID = aced.worldid();
    cout << "Connect to world with world ID=" << worldID << " successfully."
         << endl;

    // Initialize Products
    ACommands acommand;
    for (auto const& warehouse : whlist) {
        for (auto const& product : productList) {
            purchaseMore(warehouse.get()->w_id, product.p_id, product.name,
                         PRODUCT_INITIAL_AMOUNT);
        }
    }
}

/*
    Connect to database
*/
connection* Server::connectDB() {
    connection* C =
        new connection("host=db port=5432 dbname=" + dbName +
                       " user=" + userName + " password=" + password);
    if (C->is_open()) {
        // cout << "Opened database successfully: " << C->dbname() << endl;
    } else {
        throw MyException("Can't open database.");
    }
    return C;
}

/*
  close the connection to database.
*/
void Server::disConnectDB(connection* C) {
    C->disconnect();
}

/*---------------------Interaction with world--------------------------*/
/*
    Keep receiving connection from front end, and receive an int as order number
*/
void Server::acceptOrder() {
    // Continuously receiving connection from front end
    while (1) {
        string clientIP;
        try {
            int client_fd = serverAcceptConnection(frontend_fd, clientIP);
            cout << "accept a connection\n";
            string request = socketRecvMsg(client_fd);
            cout << "received a request\n";
            close(client_fd);
            int order_id = stoi(request);
            cout << "Received an incomming order from front end, o_id="
                 << order_id << endl;
            // handle
            threadPool->assign_task(bind(readOrder, order_id));
        } catch (const std::exception& e) {
            std::cerr << e.what() << endl;
            continue;
        }
    }
}

/*--------------------------Testing & Developing funcs---------------*/

/*
    Initialize Warehouses, one on center, others evenly distributed on circle
      x = r*sin(theta)
      y = r*cos(theta)
*/
// void Server::setWh_circle(AConnect& acon) {
//     for (int i = 0; i < num_wh; i++) {
//         AInitWarehouse* wh = acon.add_initwh();
//         wh->set_id(i);
//         if (i == 0) {
//             wh->set_x(0);
//             wh->set_y(0);
//             whlist.push_back(Warehouse(i, 0, 0));
//         } else {
//             double theta = (2 * 3.14159 / num_wh) * i;
//             int wh_distance = 10;
//             int x = wh_distance * sin(theta);
//             int y = wh_distance * cos(theta);
//             wh->set_x(x);
//             wh->set_y(y);
//             whlist.push_back(Warehouse(i, x, y));
//         }
//     }
// }
