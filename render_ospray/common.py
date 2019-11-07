import os, re
from struct import pack, unpack
from logging import getLogger

PROTOCOL_VERSION = 2

VERBOSE_PROTOBUF = False

# Keep in sync with include/ospray/OSPEnums.h
OSP_FB_NONE = 0 
OSP_FB_RGBA8 = 1    # one dword per pixel: rgb+alpha, each one byte
OSP_FB_SRGBA = 2    # one dword per pixel: rgb (in sRGB space) + alpha, each one byte
OSP_FB_RGBA32F = 3  # one float4 per pixel: rgb+alpha, each one float

def send_protobuf(sock, pb, sendall=True):
    """Serialize a protobuf object and send it on the socket"""
    if VERBOSE_PROTOBUF:
        getLogger('blospray').debug('send_protobuf(): %s' % pb)

    s = pb.SerializeToString()
    sock.send(pack('<I', len(s)))
    if sendall:
        sock.sendall(s)
    else:
        sock.send(s)

def receive_protobuf(sock, protobuf):
    d = sock.recv(4)

    if d == b'':
        if VERBOSE_PROTOBUF:
            getLogger('blospray').debug('receive_protobuf(): connection reset by peer')
        raise ConnectionResetError()

    bufsize = unpack('<I', d)[0]

    parts = []
    bytes_left = bufsize
    while bytes_left > 0:
        d = sock.recv(bytes_left)
        if d == b'':
            if VERBOSE_PROTOBUF:
                getLogger('blospray').debug('receive_protobuf(): connection reset by peer')            
            raise ConnectionResetError()
        parts.append(d)
        bytes_left -= len(d)

    message = b''.join(parts)

    protobuf.ParseFromString(message)

    if VERBOSE_PROTOBUF:
        getLogger('blospray').debug('receive_protobuf(): %s' % protobuf)

def receive_buffer(sock, n):
    
    parts = []
    left = n
    while left > 0:
        d = sock.recv(min(n,4096))
        left -= len(d)
        parts.append(d)
        
    return b''.join(parts)
        
        
def receive_into_numpy_array(sock, buffer, numbytes):

    view = memoryview(buffer).cast('B')
    bytes_left = numbytes
    while bytes_left > 0:
        n = sock.recv_into(view, bytes_left)
        view = view[n:]
        bytes_left -= n


def substitute_values(value, expression_locals={}):

    # Local environment variables, e.g. ${HOME}
    pat = re.compile('\$\{([^\}]+?)\}')
    def func(m):
        envvar = m.group(1)
        if envvar in os.environ:
            return os.environ[envvar]
        else:
            return '${%s}' % envvar
    value, n = re.subn(pat, func, value)

    # Global values, e.g. {{frame}}
    pat2 = re.compile('\{\{([^\}]+?)\}\}')
    def func2(m):
        expr = m.group(1)
        try:
            svalue = eval(expr, None, expression_locals)            
            svalue = str(svalue)
            return svalue
        except:            
            # XXX show backtrace
            print('WARNING: exception while trying to evaluate "%s" in property value' % expr)
            return '{{%s}}' % expr
    value, n = re.subn(pat2, func2, value)

    return value


if __name__ == '__main__':

    def tsub(value, expression_locals={}):
        s = substitute_values(value, expression_locals)
        print(repr(value), '->', repr(s))

    tsub('${HOME}')
    tsub('${HOME} ${HOME}')
    tsub('$<HOME>')
    tsub('{{frame}}')
    tsub('file{{frame}}.raw', dict(frame=25))
    tsub("file{{'%04d' % frame}}.raw", dict(frame=25))