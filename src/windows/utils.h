#include <filesystem>
#include <sys/types.h>

#define BUFFER_SIZE 128*1024

// 1. Conversion codes

// Convert time in seconds to "Hr:Min:Sec" string representation
std::string get_time_str(uint32_t seconds){
    int s = seconds%60;
    seconds/=60;
    int m = seconds%60;
    seconds/=60;
    int h = seconds;
    std::ostringstream time_str;
    time_str << h << "hr:"<<m<<"min:"<<s<<"s";
    return time_str.str();
}

// Convert Bytes to GB/MB/KB representation
std::string get_size_str(uint64_t filesize){
    if(filesize>=(1<<30)){
        float size = (float)filesize/(1<<30);
        return std::to_string(size) + " GB";
    }
    else if(filesize>=(1<<20)){
        float size = (float)filesize/(1<<20);
        return std::to_string(size) + " MB";
    }
    else if(filesize>=(1<<10)){
        float size = (float)filesize/(1<<10);
        return std::to_string(size) + " KB";
    }
    else{
        return std::to_string(filesize) + " B";
    }
}


