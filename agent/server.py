#!/usr/bin/env python3

import socket
import os.path
import os
from time import sleep, time
import struct

CONNECT_PORT = 5555
CONNECT_PORT = 6660

boxes = {
    402551434305: 0 ,
    321481224602: 1 ,
    381796144306: 2 ,
    526106124102: 3 ,
    419560323802: 4 ,
    118715543305: 5, # 283693452806: 5 , ? REFURB?
    136068441606: 6 ,
    204025132105: 7 ,
    107585251305: 8 ,
    113498221102: 9 ,
    311689714402: 10,
    500910651205: 11,
    406774733705: 12,
    620477232705: 13,
    304347750505: 14,
    219976124702: 15,

    105581114003: 100
    }

def wait_for_ack(s):
    ack = b''
    while len(ack) < 2:
        ack += s.recv(1)
    ack += b'\x00'
    print('Received: (%d) %s' % (len(ack), ack.decode('utf-8')))

def send_file(s, path, remote_path):
    file_size = os.stat(path).st_size
    print("Sending %s (%d MiB)" % (path, file_size / (1024*1024)))
    s.sendall(struct.pack('4sI32s', b'WRTE', 4+4+32+file_size, remote_path.encode('utf-8')))

    start = time()
    with open(path, 'rb') as f:
        chunk_size = 4096
        while True:
            b = f.read(chunk_size)
            if not b:
                break
            s.sendall(b)

    # Wait for ok
    wait_for_ack(s)

    stop = time()
    duration = stop-start
    print("Transferred %d bytes in %f seconds (%f MBps)" % (file_size, duration, (file_size/duration)/(1024*1024)))

def launch(s, remote_path):
    print("Launching " + remote_path)
    s.sendall(struct.pack('4sI32s', b'LNCH', 4+4+32, remote_path.encode('utf-8')))
    #wait_for_ack(s)

def reboot(s):
    print("Rebooting")
    s.sendall(struct.pack('4sI32x', b'RSET', 4+4+32))
    #wait_for_ack(s)

def send_agent_config_file(s, ip, ip_mask, ip_gw, cs):
    # Gen 32b IP address
    remote_path = "E:\\agent_config.bin"
    file_size = 4*4
    s.sendall(struct.pack('4sI32s', b'WRTE', 4+4+32+file_size, remote_path.encode('utf-8')))
    
    def pack_ip(addr):
        return struct.pack('BBBB', *map(int, addr.split('.')))
    print('sending payload')
    s.sendall(pack_ip(ip) +
              pack_ip(ip_mask) +
              pack_ip(ip_gw) +
              pack_ip(cs))
    print('waiting for ack')
    wait_for_ack(s)

def deploy(s, team_id):
    # launch(s, "C:\\Dashboard\\dash.xbe")
    # return

    # Re-deploy agent
    # send_file(s, "./bin/default.xbe", "E:\\Dashboard\\default.xbe")
    # reboot(s)
    # return

    send_file(s, "../bin/freedm.wad", "E:\\freedm.wad")
    send_file(s, "../bin/default.xbe", "E:\\doom.xbe")
    # send_file(s, "../bin/default.xbe", "E:\\doom.xbe")
    # send_file(s, "../bin/default.xbe", "E:\\doom.xbe")
    print("About to launch doom!")
    sleep(2)
    launch(s, "E:\\doom.xbe")

def serve():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('0.0.0.0', 6666))
        s.listen()
        while True:
            try:
                conn, addr = s.accept()
                pid = os.fork()
                if pid != 0:
                    continue
            except KeyboardInterrupt:
                s.close()
                return
            with conn:
                print('Connected by', addr)

                # data = None
                data = conn.recv(32+12)
                #print('Received data from client:' + data.decode('utf-8'))

                if not data:
                    break
                #conn.sendall(data)

                try:
                    serial = int(data[32:].decode('utf-8'))
                    print('Serial:' + str(serial))

                    team_id = None
                    if serial in boxes:
                        team_id = boxes[serial]

                    if team_id == None:
                        print('UNKNOWN BOX NUMBER, UPDATE SCRIPT')
                        # conn.close()
                        # s.close()
                        # exit(1)
                        team_id = 100

                    print('Box #%d Identified' % team_id)
                    print('Deploying to', addr[0])
                    deploy(conn, team_id)
                except Exception as e:
                    print('Ignoring exception: ' + str(e))

                conn.close()
                exit()

# reboot('192.168.1.2')
serve()
# deploy('192.168.1.2')
