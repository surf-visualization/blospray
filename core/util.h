#ifndef UTIL_H
#define UTIL_H

#include <sys/time.h>
#include <arpa/inet.h>
#include <boost/uuid/detail/sha1.hpp>
#include <iostream>
#include <fstream>
#include "messages.pb.h"
#include "tcpsocket.h"

inline double
time_diff(struct timeval t0, struct timeval t1)
{
    return t1.tv_sec - t0.tv_sec + (t1.tv_usec - t0.tv_usec) * 0.000001;
}

inline float 
float_swap(float value)
{
    union v {
        float       f;
        uint32_t    i;
    };

    v val;
    val.f = value;
    val.i = htonl(val.i);                 
    return val.f;
};

template<typename T>
bool
receive_protobuf(TCPSocket *sock, T& protobuf)
{
    std::vector<char> receive_buffer;
    uint32_t message_size;
    
    if (sock->recvall(&message_size, 4) == -1)
        return false;
    
    receive_buffer.reserve(message_size);
        
    if (sock->recvall(&receive_buffer[0], message_size) == -1)
        return false;
    
    // XXX this probably makes a copy?
    std::string message(&receive_buffer[0], message_size);
    
    protobuf.ParseFromString(message);
    
    return true;
}

template<typename T>
bool
send_protobuf(TCPSocket *sock, T& protobuf)
{
    std::string message;
    uint32_t message_size;
    
    protobuf.SerializeToString(&message);
    
    message_size = message.size();
    
    if (sock->send((uint8_t*)&message_size, 4) == -1)
        return false;
    
    if (sock->sendall((uint8_t*)&message[0], message_size) == -1)
        return false;
    
    return true;
}

// https://stackoverflow.com/a/39833022/9296788
std::string 
get_sha1(const std::string& p_arg)
{
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(p_arg.data(), p_arg.size());
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

    // Back to string
    char buf[41] = {0};

    for (int i = 0; i < 5; i++)
    {
        std::sprintf(buf + (i << 3), "%08x", hash[i]);
    }

    return std::string(buf);
}

// Linux only
float
memory_usage()
{
    // Based on https://stackoverflow.com/a/12675172/9296788
    int tSize = 0, resident = 0, share = 0;
    std::ifstream buffer("/proc/self/statm");
    buffer >> tSize >> resident >> share;
    buffer.close();
    
    float page_size_mb = 1.0f * sysconf(_SC_PAGE_SIZE) / (1024*1024); 
    float rss = resident * page_size_mb;
    /*
    cout << "RSS - " << rss << " kB\n";

    double shared_mem = share * page_size_kb;
    cout << "Shared Memory - " << shared_mem << " kB\n";

    cout << "Private Memory - " << rss - shared_mem << "kB\n";*/
    
    return rss;
}

#endif
