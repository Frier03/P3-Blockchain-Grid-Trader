import socket

# take the server name and port name
print("Startet clients")
host = 'local host'
port = 6666

# create a socket at client side
# using TCP / IP protocol
s = socket.socket(socket.AF_INET,
                  socket.SOCK_STREAM)

# connect it to server and port
# number on local computer.
s.connect(('127.0.0.1', port))

# Send a message to the server
message = 'rql,3'
s.sendall(message.encode('utf-8'))
print("sending message")

# receive message string from
# server, at a time 1024 B
msg = s.recv(1024)
while msg == b'':
    print("waiting...")
# repeat as long as message
# string are not empty
while msg:
    print('Received:' + msg.decode())
    msg = s.recv(1024)

# disconnect the client
s.close()
