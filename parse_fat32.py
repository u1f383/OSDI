#!/usr/bin/python3

from pwn import *
from dataclasses import dataclass

img = open("./sfn_nctuos.img", "rb").read()

partition_descs = []
in_used_partition_desc = []
sector_size = 512 # 0x200

def get_int(b):
    return u32(b.ljust(4, b'\x00'))

def CHS_decode(CHS):
    # Cylinder (10-bits), Head (8-bits) and Sector (6-bits)
    # 1024th cylinder, 255th head and 63rd sector
    C = CHS & ((1 << 10) - 1)
    H = (CHS >> 10) & ((1 << 8) - 1)
    S = (CHS >> 18) & ((1 << 6) - 1)
    print(f"C: {C}, H: {H}, S: {S}")

@dataclass
class PartitonDesc:
    idx : int
    boot_indicator : int
    start_CHS_values : int
    partition_type_desc : int
    end_CHS_values : int
    start_sector : int
    partition_size : int

for i in range(446, 446+64, 16):
    partition_descs.append(img[i:i+16])

for i in range(len(partition_descs)):
    print(f"======== partiton {i+1} ========")
    partition_desc = PartitonDesc(
        i,
        partition_descs[i][0],
        get_int(partition_descs[i][1:4]),
        partition_descs[i][4],
        get_int(partition_descs[i][5:8]),
        get_int(partition_descs[i][8:12]),
        get_int(partition_descs[i][12:16]),
    )
    

    print(f"boot indicator: {hex(partition_desc.boot_indicator)}")
    print(f"start CHS values: {hex(partition_desc.start_CHS_values)}")
    CHS_decode(partition_desc.start_CHS_values)
    print(f"partition type descriptor: {hex(partition_desc.partition_type_desc)}")
    print(f"end CHS values: {hex(partition_desc.end_CHS_values)}")
    CHS_decode(partition_desc.end_CHS_values)
    print(f"start sector: {hex(partition_desc.start_sector)}")
    print(f"partition size: {hex(partition_desc.partition_size)}")

    if partition_desc.boot_indicator == 0x80:
        in_used_partition_desc.append(partition_desc)
    if partition_desc.partition_type_desc == 0xb:
        print("[*] 32-bit FAT")

if len(in_used_partition_desc) == 0:
    exit(0)


for desc in in_used_partition_desc:
    print(f"======== partiton {desc.idx} - Volume Boot Record (VBR) ========")
    start = desc.start_sector * sector_size
    data = img[start : start + sector_size]
    
    OEM = data[3:11].decode()
    bytes_per_sector = get_int(data[11:13])
    sector_per_cluster = data[13]
    num_fat = data[16]
    rsvd_sector = get_int(data[14:16])
    num_rootdir_ent = get_int(data[17:19])
    sector_per_track = get_int(data[24:26])
    first_rootdir_cluster = get_int(data[44:46])
    
    num_sector = get_int(data[32:36])
    sector_per_fat = get_int(data[36:40])
    fs_type = data[82:90].decode()

    print("Filesystem type: ", fs_type)
    print("Original Equipment Manufacturer (OEM): ", OEM)
    print("Bytes per sector: ", hex(bytes_per_sector))
    print("Rsvd secotr: ", hex(rsvd_sector))
    print("Number of root dir entry: ", hex(num_rootdir_ent))
    print("Number of sector: ", hex(num_sector))
    print("Number of fat: ", hex(num_fat))
    print("Sector per fat: ", hex(sector_per_fat))
    print("Sector per track: ", hex(sector_per_track))
    print("First rootdir cluster: ", hex(first_rootdir_cluster))

    assert(num_sector == desc.partition_size)
    assert(first_rootdir_cluster == 2)

    print("Data block start at: ", (rsvd_sector + sector_per_fat))