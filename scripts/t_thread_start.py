#!/usr/bin/env python
import time, threading

W = 0.1
N = 100

class T(threading.Thread):
    
    def __init__(self):
        threading.Thread.__init__(self)
        
    def run(self):
        time.sleep(W)
        
t0 = time.time()

for i in range(N):
    t = T()
    t.start()
    t.join()
    
t1 = time.time()

tdiff = t1 - t0
print('%.6f for %d threads' % (tdiff,N))

overhead = tdiff - N*W

print('overhead %.6fs (%.3f ms per thread)' % (overhead, overhead/N*1000))