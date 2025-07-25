// file_receiver.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>

#include <ctime>
// #include <filesystem> //included in utils.h
// #include "utils.h"

#pragma once
#include "cancellation.h"

uint64_t be64toh(uint64_t be_64) {
    // Reverse the conversion
    return (((uint64_t)ntohl(be_64 & 0xFFFFFFFF)) << 32) | ntohl(be_64 >> 32);
}

#define PORT 12345
// #define BUFFER_SIZE 128 * 1024

using asio::ip::tcp;

int Receiver(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cout <<"Usage: fileshare receive <file_destination>\n";
        return 1;
    }

    std::string path = argv[2];
    std::cout << "Destination: " << path << std::endl;

    asio::io_context io_context;

    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 12345));
    std::cout << "Server listening on port 12345...\n";

    tcp::socket socket(io_context);
    acceptor.set_option(tcp::acceptor::reuse_address(true));

    acceptor.accept(socket);
    connection_accepted = true;
    asio::error_code ec;

    
    // 1. Receive filename length
    uint32_t fn_len = 0;
    asio::read(socket, asio::buffer(&fn_len, sizeof(fn_len)), ec);
    if (ec) {
        std::cerr << "Failed to read filename length: " << ec.message() << "\n";
        return 1;
    }
    fn_len = ntohl(fn_len); // network to host
    

    // 2. Receive filename
    std::vector<char> fname_buffer(fn_len);
    asio::read(socket, asio::buffer(fname_buffer.data(), fn_len), ec);
    if (ec) {
        std::cerr << "Failed to read filename: " << ec.message() << "\n";
        return 1;
    }

    std::string filename(fname_buffer.begin(), fname_buffer.end());
    std::cout << "Receiving file: " << filename << std::endl;
    filename = path + "\\" + filename; // Add path to the filename
    std::cout<<"filename: "<<filename<<std::endl;

    // 3. Receive filesize
    uint64_t filesize = 0;
    asio::read(socket, asio::buffer(&filesize, sizeof(filesize)), ec);
    if (ec) {
        std::cerr << "Failed to read filename length: " << ec.message() << "\n";
        return 1;
    }
    filesize = be64toh(filesize);
    std::cout << "File Size " << get_size_str(filesize) << std::endl;

    // 4. Accept or Decline file
    char accept = 'x';
    while (true)
    {
        std::cout << "Accept file? Enter [y/n]: ";
        std::cin >> accept;
        if (accept != 'y' && accept != 'n')
        {
            std::cout << "Inappropriate Input. Please enter either 'y' or 'n'" << std::endl;
        }
        else
            break;
    }
    if (accept == 'n')
    {
        std::cout << "File transfer rejected." << std::endl;
        // Send a response to sender
        asio::write(socket, asio::buffer(&accept, sizeof(accept)), ec);
        if (ec)
        {
            std::cerr << "File size send failed: " << ec.message() << "\n";
            return 1;
        }
        return 1;
    }
    else
    {
        asio::write(socket, asio::buffer(&accept, sizeof(accept)), ec);
        if (ec)
        {
            std::cerr << "File size send failed: " << ec.message() << "\n";
            return 1;
        }
    }
    
    std::cout << "\nPress Ctrl+C to cancel transfer."<<std::endl;

    // 5. Open output file
    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile)
    {
        std::cerr << "Failed to open output file.\n";
        return 1;
    }


    // 6. Receive file content in chunks

    time_t start_time = time(NULL);
    uint64_t transferred = 0;

    int timer_count = 0;
    uint32_t estimated_time;

    while (true)
    {
        if(cancel_requested) break;

        uint32_t chunk_size = 0;
        asio::read(socket, asio::buffer(&chunk_size, sizeof(chunk_size)), ec);
        if (ec) {
            std::cerr << "\nFailed to read chunk size: " << ec.message() << "\n";
            cancel_requested = true;
            break;
        }
        chunk_size = ntohl(chunk_size);
        if (chunk_size == 0) break; // End of file

        std::vector<char> buffer(chunk_size);
        asio::read(socket, asio::buffer(buffer.data(), chunk_size), ec);
        if (ec) {
            std::cerr << "\nFailed to read chunk data: " << ec.message() << "\n";
            cancel_requested = true;
            break;
        }

        outfile.write(buffer.data(), chunk_size);

        transferred += chunk_size;
        time_t curr_time = time(NULL);
        double elapsed = difftime(curr_time, start_time);
        double percent = (double)transferred / filesize * 100.0;
        double speed = elapsed > 0 ? ((transferred / (1024 * 1024)) / elapsed) : 0;
        if (timer_count == 0)
        {
            estimated_time = speed > 0 ? ((filesize - transferred) * elapsed) / transferred : 0;
            timer_count = 10;
        }
        else
            timer_count--;

        std::cout << "\rTransferred: [" << std::fixed << std::setprecision(2)
                  << percent << "%] - Speed: " << speed << " MB/s - " << "Estimated Time: " << get_time_str(estimated_time) << std::flush;
    }

    outfile.close();
    if(cancel_requested){
        std::remove(filename.c_str());
        std::cout<<"\n\nFile transfer cancelled."<<std::endl;
        return 1;

    }

    std::cout << "\n\nFile received and saved.\n";
    time_t end_time = time(NULL);
    int diff = difftime(end_time,start_time);
    std::cout<<"Time taken: "<<get_time_str(diff)<<std::endl;

    return 0;
}
