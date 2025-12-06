#include <asio.hpp>
#include<iostream>
#include<cstring>
#include <csignal>
#include <thread>
#include <chrono>

#include "cancellation.h"
#include "discovery.h"
#include "utils.h"
#include "receiver.h"
#include "sender.h"

using namespace std;
std::atomic<bool> cancel_requested(false);
std::atomic<bool> connection_accepted(false);


void signal_handler(int signum) {
    // std::cout << "\nTransfer interrupted. Cleaning up...\n";
    if(connection_accepted==false) exit(1);
    cancel_requested = true;
}

void print_usage(){
    std::cout<<"Usage: fileshare [send/receive] [ip] [filepath]\nTry 'fileshare --help' for more information."<<std::endl;
}

static std::vector<Peer> peers;


int main(int argc, char* argv[])
{
    std::signal(SIGINT, signal_handler);

    // One way to use CLI
    if(argc==1){
        cout<<"Welcome to fileshare\n[1] Send\n[2] Receive\nEnter option:"<<endl;
        char option='0';
        cin>>option;
        while(option!='1'&&option!='2'){
            cout<<"Wrong option entered! Enter '1' or '2':"<<endl;
            cin>>option;
        }
        if(option=='1'){
            //send
            string filepath;
            cout<<"Enter the file path:";
            cin>>filepath;
            filepath = format_path(filepath);
            asio::io_context io_context;
            peers = discover_peers(io_context);
            if(peers.empty()){
                char y='n';
                while(true){
                    cout<<"No peers found.\nRetry?[y/n] :";
                    cin>>y;
                    if(y!='y'&&y!='Y') break;
                    peers = discover_peers(io_context);
                }
            }
            else{
                cout<<"Peers found"<<endl;
                int peers_count = peers.size();
                for(int i=0;i<peers_count;i++){
                    cout<<"["<<i<<"] "<<peers[i].hostname<<endl;
                }
                cout<<"Enter the receiver index: ";
                int index;
                cin>>index;
                Peer p = peers[index];
                
                //Send response to receiver to start receiving
                Sender2(p.ip,to_string(p.tcp_port),filepath);

            }

        }
        else{
            //receive
            string directory="";
            while(directory==""){
                cout<<"Enter your directory:";
                cin>>directory;
            }
            directory = format_path(directory);
            // beacon
            static bool beacon_started = false;
            static std::unique_ptr<std::thread> beacon_thread;
            if (!beacon_started) {
                beacon_started = true;
                beacon_thread = std::make_unique<std::thread>([&]() {
                    // beacon_thread_func(io_context,TCP_PORT, get_hostname());
                    beacon_thread_func(TCP_PORT, get_hostname());
                });
                std::cout << "✅ Beacon started - announcing on UDP 5353" << std::endl;
            }

            // begin receiving
            Receiver2(directory);
            receiver_active=false;
            if (beacon_thread) {
                beacon_thread->join();  // Wait for clean shutdown
            }
        }
        return 0;

    }

    if(argc<2){
        print_usage();
        return 1;
    }

    
    if(strcmp(argv[1],"--help")==0){
        std::cout<<"Send File : fileshare send <ip> <file_path>\n"
        <<"Receive File : fileshare receive <save_path>\n"
        <<"\nTo send file use receiver's IP ADDRESS. Example : '192.168.0.1'"<<std::endl;
    }
    else if(strcmp(argv[1],"send")==0){
        // std::cout<<"Sending file"<<std::endl;
        Sender(argc,argv);
    }
    else if(strcmp(argv[1],"receive")==0){
        // std::cout<<"Receiving file"<<std::endl;
        Receiver(argc,argv);

    }
    else{
        std::cout<<"Invalid arguments!"<<std::endl;
        print_usage();
    }

    return 0;
}