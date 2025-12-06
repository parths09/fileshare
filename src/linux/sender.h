#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <ctime>
// #include <filesystem>

// #pragma once
// #include "utils.h"

#pragma once
#include "cancellation.h"

#define PORT 12345
// #define BUFFER_SIZE 128*1024

namespace fs = std::filesystem;

using asio::ip::tcp;

int Sender(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cout << "Usage: fileshare send <server_ip> <file_to_send>\n";
        return 1;
    }

    std::string server_ip = argv[2];
    std::string filepath = argv[3];
    std::string filename = filepath; // You may want to strip path here for just basename.
    uint64_t filesize = fs::file_size(filename);
    std::cout << "Sending File : " << filename << std::endl;
    std::cout << "File Size " << get_size_str(filesize) << std::endl;

    // Just keep name of file and strip other things -
    int n = filename.size();
    for (int i = n - 1; i >= 0; i--)
    {
        if (filename[i] == '/' || filename[i] == '\\')
        {
            filename = filename.substr(i + 1, n - i);
            break;
        }
    }
    
    asio::io_context io_context;
    tcp::socket socket(io_context);
    tcp::resolver resolver(io_context);
    try{
        asio::connect(socket, resolver.resolve(server_ip, "12345"));
    }
    catch(std::system_error e){
        perror(e.what());
        return 1;
    }
    asio::error_code ec;

    // 1. Send filename length
    uint32_t fn_len = filename.size();
    u_int32_t fn_len_net = htonl(fn_len);
    asio::write(socket, asio::buffer(&fn_len_net, sizeof(fn_len)), ec);
    if (ec)
    {
        std::cerr << "Filename length send failed: " << ec.message() << "\n";
        return 1;
    }

    // 2. Send filename
    asio::write(socket, asio::buffer(filename,fn_len), ec);
    if (ec)
    {
        std::cerr << "Filename send failed: " << ec.message() << "\n";
        return 1;
    }

    // 3. Send file size
    uint64_t filesize_net = htobe64(static_cast<uint64_t> (filesize));
    asio::write(socket, asio::buffer(&filesize_net, sizeof(filesize)), ec);
    if (ec)
    {
        std::cerr << "File size send failed: " << ec.message() << "\n";
        return 1;
    }

    // 4. Check if File Accepted
    char response = 'x';
    std::cout<<"Waiting for receiver to accept..."<<std::endl;
    asio::read(socket, asio::buffer(&response, sizeof(response)), ec);
    if (ec)
    {
        std::cerr << "Failed to get response from receiver: " << ec.message() << "\n";
        return 1;
    }
    // else std::cout << "Response " << response << std::endl;
    if (response != 'y')
    {
        std::cerr << "File transfer rejected by receiver.\n";
        return 1; // Gracefully exit
    }

    std::cout << "\nPress Ctrl+C to cancel transfer."<<std::endl;

    // 5. Open and send the file in chunks
    std::ifstream infile(filepath, std::ios::binary);
    if (!infile)
    {
        std::cerr << "Cannot open file for reading.\n";
        return 1;
    }

    time_t start_time = time(NULL);
    u_int64_t transferred = 0;

    int timer_count = 0;
    u_int32_t estimated_time;
    
    std::vector<char> buffer(BUFFER_SIZE);
    while (true)
    {
        if(cancel_requested) break;
        infile.read(buffer.data(), BUFFER_SIZE);
        uint32_t bytesRead = infile.gcount();

        uint32_t bytesRead_net = htonl(bytesRead);

        // Send chunk size first
        asio::write(socket, asio::buffer(&bytesRead_net, sizeof(bytesRead_net)), ec);
        if (ec)
        {
            std::cerr << "Chunk size send failed: " << ec.message() << "\n";
            cancel_requested = true;
            break;
        }

        if(bytesRead<=0) break;

        asio::write(socket, asio::buffer(buffer.data(), bytesRead), ec);
        if (ec)
        {
            std::cerr << "\nChunk data send failed: " << ec.message() << "\n";
            cancel_requested = true;
            break;
        }

        transferred += bytesRead;
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

    if(cancel_requested){
        std::cout<<"\n\nFile transfer cancelled."<<std::endl;
        return 1;
    }

    std::cout << "\n\nFile sent successfully.\n";
    time_t end_time = time(NULL);
    int diff = difftime(end_time, start_time);
    std::cout << "Time taken: " << get_time_str(diff) << std::endl;
    return 0;
}

int Sender2(std::string server_ip,std::string server_port,std::string filepath)
{
    // if (argc != 4)
    // {
    //     std::cout << "Usage: fileshare send <server_ip> <file_to_send>\n";
    //     return 1;
    // }

    // std::string server_ip = argv[2];
    // std::string filepath = argv[3];
    std::string filename = filepath; // You may want to strip path here for just basename.
    uint64_t filesize = fs::file_size(filename);
    std::cout << "Sending File : " << filename << std::endl;
    std::cout << "File Size " << get_size_str(filesize) << std::endl;

    // Just keep name of file and strip other things -
    int n = filename.size();
    for (int i = n - 1; i >= 0; i--)
    {
        if (filename[i] == '/' || filename[i] == '\\')
        {
            filename = filename.substr(i + 1, n - i);
            break;
        }
    }
    
    asio::io_context io_context;
    tcp::socket socket(io_context);
    tcp::resolver resolver(io_context);
    try{
        asio::connect(socket, resolver.resolve(server_ip, "12345"));
    }
    catch(std::system_error e){
        perror(e.what());
        return 1;
    }
    asio::error_code ec;

    // 1. Send filename length
    uint32_t fn_len = filename.size();
    u_int32_t fn_len_net = htonl(fn_len);
    asio::write(socket, asio::buffer(&fn_len_net, sizeof(fn_len)), ec);
    if (ec)
    {
        std::cerr << "Filename length send failed: " << ec.message() << "\n";
        return 1;
    }

    // 2. Send filename
    asio::write(socket, asio::buffer(filename,fn_len), ec);
    if (ec)
    {
        std::cerr << "Filename send failed: " << ec.message() << "\n";
        return 1;
    }

    // 3. Send file size
    uint64_t filesize_net = htobe64(static_cast<uint64_t> (filesize));
    asio::write(socket, asio::buffer(&filesize_net, sizeof(filesize)), ec);
    if (ec)
    {
        std::cerr << "File size send failed: " << ec.message() << "\n";
        return 1;
    }

    // 4. Check if File Accepted
    char response = 'x';
    std::cout<<"Waiting for receiver to accept..."<<std::endl;
    asio::read(socket, asio::buffer(&response, sizeof(response)), ec);
    if (ec)
    {
        std::cerr << "Failed to get response from receiver: " << ec.message() << "\n";
        return 1;
    }
    // else std::cout << "Response " << response << std::endl;
    if (response != 'y')
    {
        std::cerr << "File transfer rejected by receiver.\n";
        return 1; // Gracefully exit
    }

    std::cout << "\nPress Ctrl+C to cancel transfer."<<std::endl;

    // 5. Open and send the file in chunks
    std::ifstream infile(filepath, std::ios::binary);
    if (!infile)
    {
        std::cerr << "Cannot open file for reading.\n";
        return 1;
    }

    time_t start_time = time(NULL);
    u_int64_t transferred = 0;

    int timer_count = 0;
    u_int32_t estimated_time;
    
    std::vector<char> buffer(BUFFER_SIZE);
    while (true)
    {
        if(cancel_requested) break;
        infile.read(buffer.data(), BUFFER_SIZE);
        uint32_t bytesRead = infile.gcount();

        uint32_t bytesRead_net = htonl(bytesRead);

        // Send chunk size first
        asio::write(socket, asio::buffer(&bytesRead_net, sizeof(bytesRead_net)), ec);
        if (ec)
        {
            std::cerr << "Chunk size send failed: " << ec.message() << "\n";
            cancel_requested = true;
            break;
        }

        if(bytesRead<=0) break;

        asio::write(socket, asio::buffer(buffer.data(), bytesRead), ec);
        if (ec)
        {
            std::cerr << "\nChunk data send failed: " << ec.message() << "\n";
            cancel_requested = true;
            break;
        }

        transferred += bytesRead;
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

    if(cancel_requested){
        std::cout<<"\n\nFile transfer cancelled."<<std::endl;
        return 1;
    }

    std::cout << "\n\nFile sent successfully.\n";
    time_t end_time = time(NULL);
    int diff = difftime(end_time, start_time);
    std::cout << "Time taken: " << get_time_str(diff) << std::endl;
    return 0;
}
