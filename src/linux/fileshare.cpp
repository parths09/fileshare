#include <asio.hpp>
#include<iostream>
#include<cstring>
#include <csignal>

#include "cancellation.h"
#include "utils.h"
#include "receiver.h"
#include "sender.h"

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

int main(int argc, char* argv[])
{
    if(argc<2){
        print_usage();
        return 1;
    }

    std::signal(SIGINT, signal_handler);
    
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