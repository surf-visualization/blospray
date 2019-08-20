from struct import pack, unpack

def send_protobuf(sock, pb, sendall=False):
    """Serialize a protobuf object and send it on the socket"""
    s = pb.SerializeToString()
    sock.send(pack('<I', len(s)))
    if sendall:
        sock.sendall(s)
    else:
        sock.send(s)

def receive_protobuf(sock, protobuf):
    d = sock.recv(4)
    bufsize = unpack('<I', d)[0]

    parts = []
    bytes_left = bufsize
    while bytes_left > 0:
        d = sock.recv(bytes_left)
        parts.append(d)
        bytes_left -= len(d)

    message = b''.join(parts)

    #print('receive_protobuf():\n')
    #print(message)

    protobuf.ParseFromString(message)

def receive_buffer(sock, n):
    
    parts = []
    left = n
    while left > 0:
        d = sock.recv(min(n,4096))
        left -= len(d)
        parts.append(d)
        
    return b''.join(parts)
        
        
def receive_into_numpy_array(sock, buffer, n):
    
    view = memoryview(buffer)
    bytes_left = n
    while bytes_left > 0:
        n = sock.recv_into(view, bytes_left)
        view = view[n:]
        bytes_left -= n