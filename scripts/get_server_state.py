#!/usr/bin/env python
import sys, os, json, socket

# Can't import the render_ospray package directly as that trigger bpy stuff
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir,'render_ospray'))

from common import PROTOCOL_VERSION, send_protobuf, receive_protobuf
from messages_pb2 import (
    HelloResult, ClientMessage, ServerStateResult
)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)                
sock.connect(('localhost', 5909))

# Handshake
client_message = ClientMessage()
client_message.type = ClientMessage.HELLO
client_message.uint_value = PROTOCOL_VERSION
send_protobuf(sock, client_message)

result = HelloResult()
receive_protobuf(sock, result)

if not result.success:
    print('ERROR: Handshake with server:')
    print(result.message)
    sys.exit(-1)

# Send request
print('Getting server state')
client_message = ClientMessage()
client_message.type = ClientMessage.GET_SERVER_STATE
send_protobuf(sock, client_message)

# Get result
result = ServerStateResult()        
receive_protobuf(sock, result)

# Bye
client_message.type = ClientMessage.BYE
send_protobuf(sock, client_message)        
sock.close()

print(result.state)
