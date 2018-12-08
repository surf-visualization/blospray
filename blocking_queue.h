#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <queue>
#include <pthread.h>
#include <cstdio>

// XXX pthread -> <mutex> and friends

template <class T>
class BlockingQueue
{
public:
    // capacity 0 means no limit
    BlockingQueue(int capacity=0)
    {
        this->m_capacity = capacity;
        m_size = 0;
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_cond_full, NULL);
        pthread_cond_init(&m_cond_empty, NULL);
    }

    void push(T value)
    {
        pthread_mutex_lock(&m_mutex);

        if (m_capacity > 0)
        {
            while (m_size == m_capacity)
                pthread_cond_wait(&m_cond_full, &m_mutex);
        }

        m_queue.push(value);
        m_size++;

        pthread_mutex_unlock(&m_mutex);
        pthread_cond_broadcast(&m_cond_empty);
    }

    T pop()
    {
        pthread_mutex_lock(&m_mutex);

        while (m_size == 0)
            pthread_cond_wait(&m_cond_empty, &m_mutex);

        T value = m_queue.front();
        m_queue.pop();
        m_size--;

        pthread_mutex_unlock(&m_mutex);
        pthread_cond_broadcast(&m_cond_full);

        return value;
    }
    
    // Peeking only works reliable if the thread that does the peek() is
    // *the only* thread that does a pop() on the same thread. Otherwise
    // a peek()ed item may disappear because a different thread does a pop().
    bool peek(T* t)
    {
        bool res;
        
        pthread_mutex_lock(&m_mutex);
        
        if (m_size == 0)        
            res = false;
        else
        {
            res = true;
            *t = m_queue.front();
        }
        
        pthread_mutex_unlock(&m_mutex);
        
        return res;
    }

    int size()
    {
        pthread_mutex_lock(&m_mutex);

        int res = m_size;

        pthread_mutex_unlock(&m_mutex);

        return res;
    }

protected:

    int                 m_capacity, m_size;
    std::queue<T>       m_queue;

    pthread_mutex_t     m_mutex;
    pthread_cond_t      m_cond_full;
    pthread_cond_t      m_cond_empty;
};

#endif
