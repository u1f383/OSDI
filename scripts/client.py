#!/usr/bin/env python3

from pwn import *
import os
import sys

dev_path = "/dev/tty.usbserial-0001"
kern_path = "/Users/u1f383/Documents/Homework/OSDI/kernel8.img"
chk_sz = 1024

if len(sys.argv) > 1:
    dev_path = sys.argv[1]
if len(sys.argv) > 2:
    kern_path = sys.argv[2]

r = serialtube(dev_path, baudrate=115200, timeout=10, buffer_fill_size=0, convert_newlines=False)

def calc_checksum(data, size):
    sum = 0
    for i in range(size):
        sum += data[i]
    return sum & 0xff

STATUS_FIN = 1
STATUS_PED = 2
STATUS_END = 3
STATUS_OK  = 4
STATUS_BAD = 5

def pack_packet(_id, status, size, data):
    assert(len(data) == size)
    checksum = calc_checksum(data, size)
    return p32(_id) + p8(status) + p8(checksum) + p16(size) + data

def unpack_packet(packet):
    _id = u32(packet[:4])
    status = u8(packet[4:5])
    checksum = u8(packet[5:6])
    size = u16(packet[6:8])
    return _id, status, checksum, size

def send(r, data):
    size = len(data)
    for i in range(size):
        r.send_raw(bytes([ data[i] ]))
        

def recv(r, size):
    data = b''
    while len(data) < size:
        data += r.recv_raw(1)
    return data

MAGIC_1 = 0x54875487
MAGIC_2 = 0x77777777
MAGIC_3 = 0xcccc8763

res = b''
while True:
    if b'load_kern\r\n' in res:
        print("[log] send command OK")
        break
    else:
        send(r, b'load_kern\n')
        sleep(1)
        res = r.recv_raw(0x20)

res = res.replace(b'load_kern\r\n', b'')
if u32(res) == MAGIC_1:
    send(r, p32(MAGIC_2))
    res = recv(r, 4)

    if u32(res) == MAGIC_3:
        print("OK")
        with open(kern_path, "rb") as kern:
            data = kern.read()
            size = len(data)

            chunks = [ data[i:i+chk_sz] for i in range(0, size, chk_sz) ]
            _id = 0

            for chunk in chunks:
                packet = pack_packet(_id, STATUS_PED, len(chunk), chunk)
                status = 0
                
                while status != STATUS_OK:
                    print("[log] send data:", _id*chk_sz, (_id+1)*chk_sz)
                    try:
                        send(r, packet)
                        sleep(0.1)
                        res = recv(r, 8)
                        __id, status, checksum, size = unpack_packet(res)

                        if status == STATUS_BAD:
                            print("RECV:", checksum, size)
                            print("SEND:", calc_checksum(chunk, len(chunk)), len(chunk))
                            break
                    except:
                        print("[log] wrong")
                        r.close()
                        exit(1)
                
                _id += 1
            print("[log] send data end")
    
        packet = pack_packet(0, STATUS_END, 0, b"")
        send(r, packet)

        res = recv(r, 8)
        _, status, _, _ = unpack_packet(res)

        if status == STATUS_FIN:
            print("[log] finish connection successfully")
        else:
            print("[log] finish connection accidently")
            
r.close()