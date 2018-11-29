// Small utility class to make TCP usage a bit easier. Only a single
// header, everything inline.
//
// Doesn't do state checking, so e.g. calling bind() twice is not caught
//
// Paul Melis <paul.melis@surfsara.nl>, SURFsara

#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

class TCPSocket
{
public:

    TCPSocket(bool verbose=false)
    {
        this->verbose = verbose;
        destination_addr = NULL;
        
        errno_for_last_fail = 0;
        
        // Doing stuff that can fail in a constructor is usually not
        // a good idea, but socket creation like this shouldn't fail in the
        // general case.
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == -1)
        {
            errno_for_last_fail = errno;
            perror("Socket creating failed:");
            exit(-1);
        }
    }

    TCPSocket(int fd)
    {
        sock = fd;
    }

    ~TCPSocket()
    {
        close();

        if (destination_addr)
            freeaddrinfo(destination_addr);
    }
    
    void set_option(int level, int optname, bool optval)
    {
        int val = optval;
        ::setsockopt(this->sock, level, optname, &val, sizeof(val));
    }

    int bind(unsigned short port, const char *node=NULL)
    {
        char portstring[16];
        sprintf(portstring, "%d", port);            

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        // node == NULL -> bind to all interfaces
        struct addrinfo *receiving_addr;
        getaddrinfo(node, portstring, &hints, &receiving_addr);

        if (receiving_addr == NULL)
        {
            // XXX doesn't use errno :-/
            //errno_for_last_fail = errno;
            perror("getaddrinfo failed:");
            return -1;
        }

        if (verbose)
        {
            // Print the found addresses
            struct addrinfo *rp;
            sockaddr_in     *addr;
            char            address[INET_ADDRSTRLEN];

            for (rp = receiving_addr; rp != NULL; rp = rp->ai_next)
            {
                addr = (sockaddr_in *)rp->ai_addr;
                printf("receiving_addr %s\n", inet_ntop(AF_INET, &addr->sin_addr, address, INET_ADDRSTRLEN));
            }
        }

        // Bind socket
        int on = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

        if (::bind(sock, receiving_addr->ai_addr, receiving_addr->ai_addrlen) == -1)
        {
            errno_for_last_fail = errno;
            perror("::bind() failed:");
            return -1;
        }

        freeaddrinfo(receiving_addr);
        
        return 0;
    }

    int listen(int backlog)
    {
        int res = ::listen(sock, backlog);
        
        if (res == -1)
        {
            errno_for_last_fail = errno;
            perror("::listen() failed:");
            return -1;
        }
        
        return 0;
    }

    TCPSocket *accept()
    {
        int res = ::accept(sock, NULL, NULL);

        if (res == -1)
        {
            errno_for_last_fail = errno;
            return NULL;
        }

        return new TCPSocket(res);
    }


    int connect(const char *node, unsigned short port)
    {
        char portstring[16];
        sprintf(portstring, "%d", port);

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;            

        getaddrinfo(node, portstring, &hints, &destination_addr);

        if (destination_addr == NULL)
        {
            errno_for_last_fail = errno;
            perror("getaddrinfo failed:");
            return -1;
        }

        if (verbose)
        {
            // Print the found addresses
            struct addrinfo *rp;
            sockaddr_in     *addr;
            char            address[INET_ADDRSTRLEN];

            for (rp = destination_addr; rp != NULL; rp = rp->ai_next)
            {
                addr = (sockaddr_in *)rp->ai_addr;
                printf("destination_addr %s\n", inet_ntop(AF_INET, &addr->sin_addr, address, INET_ADDRSTRLEN));
            }
        }

        if (::connect(sock, destination_addr->ai_addr, destination_addr->ai_addrlen) == -1)
        {
            errno_for_last_fail = errno;
            perror("::connect failed");
            return -1;
        }

        return 0;
    }

    // Just like ::send()
    inline int send(const uint8_t *buf, size_t len, int flags=0)
    {
        int res = ::send(sock, buf, len, flags);

        if (res == -1)
        {
            errno_for_last_fail = errno;
            perror("::send() failed:");
            return -1;
        }

        return res;
    }

    // Loops to get all data sent
    inline int sendall(const uint8_t *buf, size_t len, int flags=0)
    {
        int sent = 0;

        while (len > 0)
        {
            int res = ::send(sock, buf+sent, len, flags);
            
            if (res > 0)
            {
                sent += res;
                len -= res;
            }
            else if (res == -1)
            {
                errno_for_last_fail = errno;
                perror("::send() failed:");
                return -1;
            }
            else if (res == 0)
            {
                errno_for_last_fail = errno;
                printf("::send() returned 0\n");
                return -1;                
            }
        }

        return sent;
    }

    // Returns number of bytes received, or -1 on error
    inline ssize_t recv(void *buf, ssize_t buflen, int flags=0)
    {
        ssize_t res = ::recv(sock, buf, buflen, flags);

        if (res == -1)
        {
            errno_for_last_fail = errno;
            perror("::recv() failed:");
            return -1;
        }

        /*
        if (verbose)
            printf("Received %d bytes\n", res);
        */

        return res;
    }

    // Like recv() above, but loop until all data (buflen bytes) is received (or error happens)
    inline ssize_t recvall(void *buf, ssize_t buflen, int flags=0)
    {
        ssize_t res;
        ssize_t received = 0;

        while (received < buflen)
        {
            res = ::recv(sock, (uint8_t*)buf+received, buflen-received, flags);

            if (res <= 0)
            {
                errno_for_last_fail = errno;
                perror("::recv() failed:");
                return -1;
            }

            received += res;
        }

        /*
        if (verbose)
            printf("Received %ld bytes\n", res);
        */

        return received;
    }

    // Polls
    inline bool is_readable()
    {
        fd_set   fs;

        FD_ZERO(&fs);
        FD_SET(sock, &fs);

        /*
        If  both fields  of the timeval structure are zero, then
        select() returns immediately.  (This is useful for polling.)
        If timeout is NULL (no timeout), select() can block indefinitely.*/

        timeval tv;
        int     res;

        tv.tv_sec = tv.tv_usec = 0;
        res = ::select(sock+1, &fs, NULL, NULL, &tv);
        
        // XXX handle error?

        return res == 1;
    }

    inline bool is_writable()
    {
        fd_set   fs;

        FD_ZERO(&fs);
        FD_SET(sock, &fs);

        /*
        If  both fields  of the timeval structure are zero, then
        select() returns immediately.  (This is useful for polling.)
        If timeout is NULL (no timeout), select() can block indefinitely.*/

        timeval tv;
        int     res;

        tv.tv_sec = tv.tv_usec = 0;
        res = ::select(sock+1, NULL, &fs, NULL, &tv);
        
        // XXX handle error?

        return res == 1;
    }
    
    // Waits
    inline bool wait_for_readable(float timeout=0.0f)
    {
        fd_set   fs;

        FD_ZERO(&fs);
        FD_SET(sock, &fs);

        int res;
        
        // XXX handle error?

        if (timeout > 0.0f)
        {
            timeval tv;
            tv.tv_sec = int(timeout);
            tv.tv_usec = int(timeout*1000000);
            res = ::select(sock+1, &fs, NULL, NULL, &tv);
        }
        else
            res = ::select(sock+1, &fs, NULL, NULL, NULL);

        return res == 1;
    }

    inline bool wait_for_writable(float timeout=0.0f)
    {
        fd_set   fs;

        FD_ZERO(&fs);
        FD_SET(sock, &fs);

        int res;
        
        // XXX handle error?

        if (timeout > 0.0f)
        {
            timeval tv;
            tv.tv_sec = int(timeout);
            tv.tv_usec = int(timeout*1000000);
            res = ::select(sock+1, NULL, &fs, NULL, &tv);
        }
        else
            res = ::select(sock+1, NULL, &fs, NULL, NULL);

        return res == 1;
    }
    
    inline int close()
    {
        errno_for_last_fail = errno;
        
        int res = ::close(sock);
        if (res == -1)
        {
            errno_for_last_fail = errno;
            perror("::close() failed");
            return -1;
        }
        
        return 0;
    }
    
    // Result only valid directly after failing method call
    inline int get_errno() const
    {
        return errno_for_last_fail;
    }

protected:
    bool            verbose;
    int             sock;
    struct addrinfo *destination_addr;
    int             errno_for_last_fail;
};

#endif
