# Credit to http://pymotw.com/2/socket/uds.html for source code example.
# http://pymotw.com/2/socket/udp.html modified for datagram sockets which don't listen just bind.
import socket
import sys
import os
import struct # Module performs conversions between Python values and C structs represented as Python strings.

server_address = '/var/run/diskwatch'

# Make sure the socket does not already exist
try:
	os.unlink(server_address)
except OSError:
	if os.path.exists(server_address):
        	raise

# Create a UDS datagram socket
sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

# Bind the socket to the port
print >> sys.stderr, 'starting up on %s' % server_address
sock.bind(server_address)

# Listen for incoming connections. Not on datagram socket, is stateless.
# sock.listen(1)
try:
	while True:
		# Wait for a message, print it out when received.
		print >> sys.stderr, '\nwaiting to receive message: ',
		data = sock.recv(4096)  # Returns a tuple of two 64 bit numbers, i.e. data[0] and data[1]
		if len(data) == 16: # Expecting a string of 16 characters (128 bits) to be received.
			sector_num = int('{:08b}'.format( struct.unpack('!qq', data)[0] )[::-1], 2)
			nb_sectors = int('{:08b}'.format( struct.unpack('!qq', data)[1] )[::-1], 2)
    		print >> sys.stderr, 'sector_num: ' + str(sector_num) + ' nb_sectors: ' + str(nb_sectors)
	
finally:
	# Clean up the socket.
    	print >> sys.stderr, 'closing socket'
    	sock.close()
