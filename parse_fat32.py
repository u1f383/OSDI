#!/usr/bin/python3

from pwn import *
from dataclasses import dataclass
import math

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
    print(f"    C: {C}, H: {H}, S: {S}")

def get_dir_attr(val):
    attr_str = ""
    if val & 0x0f == 0xf:
        return "long_name"
    if val & 0x01:
        attr_str += "rdonly,"
    if val & 0x02:
        attr_str += "hidden,"
    if val & 0x04:
        attr_str += "system,"
    if val & 0x08:
        attr_str += "volume_id,"
    if val & 0x10:
        attr_str += "directory,"
    if val & 0x20:
        attr_str += "archive,"
    return attr_str[:-1]

@dataclass
class PartitonDesc:
    idx : int
    boot_indicator : int
    start_CHS_values : int
    partition_type_desc : int
    end_CHS_values : int
    start_sector : int
    partition_size : int

@dataclass
class DirEnt:
    name : str
    ext : str
    attr : str
    ntres : int
    crt_time_tench : int
    crt_time : int
    crt_date : int
    last_acc_date : int
    fst_clus : int
    wrt_time : int
    wrt_date : int
    file_size : int

for i in range(446, 446+64, 16):
    partition_descs.append(img[i:i+16])

for i in range(len(partition_descs)):
    partition_desc = PartitonDesc(
        i,
        partition_descs[i][0],
        get_int(partition_descs[i][1:4]),
        partition_descs[i][4],
        get_int(partition_descs[i][5:8]),
        get_int(partition_descs[i][8:12]),
        get_int(partition_descs[i][12:16]),
    )
    
    if partition_desc.boot_indicator != 0x80:
        continue

    print(f"======== partiton {i+1} ========")
    print(f"boot indicator: {hex(partition_desc.boot_indicator)}")
    print(f"start CHS values: {hex(partition_desc.start_CHS_values)}")
    CHS_decode(partition_desc.start_CHS_values)
    print(f"partition type descriptor: {hex(partition_desc.partition_type_desc)}")
    print(f"end CHS values: {hex(partition_desc.end_CHS_values)}")
    CHS_decode(partition_desc.end_CHS_values)
    print(f"start sector: {hex(partition_desc.start_sector)}")
    print(f"partition size: {hex(partition_desc.partition_size)}")

    if partition_desc.partition_type_desc == 0xb:
        print("[*] 32-bit FAT")

    in_used_partition_desc.append(partition_desc)

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
    print("Rsvd sector: ", hex(rsvd_sector))
    print("Number of root dir entry: ", hex(num_rootdir_ent))
    print("Number of sector: ", hex(num_sector))
    print("Number of fat: ", hex(num_fat))
    print("Sector per cluster: ", hex(sector_per_cluster))
    print("Sector per fat: ", hex(sector_per_fat))
    print("Sector per track: ", hex(sector_per_track))
    print("First rootdir cluster: ", hex(first_rootdir_cluster))

    assert(num_sector == desc.partition_size)
    assert(first_rootdir_cluster == 2)

    data_block_sector = rsvd_sector + sector_per_fat * num_fat
    fat_sector = rsvd_sector

    db_offset = start + data_block_sector * sector_size
    fat_offset = start + fat_sector * sector_size
    fat_offset_end = fat_offset + sector_per_fat * sector_size
    print("Data block start at (sector): ", hex(data_block_sector))
    print("Data block start at (bytes): ", hex(db_offset))
    print("FAT start at (sector): ", hex(fat_sector))
    print("FAT start at (bytes): ", hex(fat_offset))
    print("FAT end at (bytes): ", hex(fat_offset_end))

    fat_data = img[fat_offset:fat_offset_end]

    print("======== first 4 FAT ========")
    # for i in range(0, len(fat_data), 0x4):
        # if get_int(fat_data[i:i+4]) == 0:
        #     print("empty fat ent idx: ", hex(i))
        #     break
        # print(hex(get_int(fat_data[i:i+4])))

    # In fat32, the rootdir is included in the data block sector
    rootdir_sector = data_block_sector
    rootdir_offset = db_offset
    print("Rootdir at (bytes): ", hex(rootdir_offset))
    rootdir_data = img[rootdir_offset:rootdir_offset+0x100]

    def get_clus_N_offset(N):
        return ((N-2) * sector_per_cluster + data_block_sector) * sector_size + start
    
    dir_ents = []
    for i in range(0, 0x100, 0x20):
        if rootdir_data[i:i+8] == b'\x00' * 8:
            break

        if get_dir_attr(rootdir_data[i+11]) == "long_name":
            continue
        dir_ent = DirEnt(
            rootdir_data[i:i+8].strip().decode(),
            rootdir_data[i+8:i+11].strip().decode(),
            get_dir_attr(rootdir_data[i+11]),
            rootdir_data[i+12],
            rootdir_data[i+13],
            get_int(rootdir_data[i+14:i+16]),
            get_int(rootdir_data[i+16:i+18]),
            get_int(rootdir_data[i+18:i+20]),
            (get_int(rootdir_data[i+20:i+22]) << 16) + get_int(rootdir_data[i+26:i+28]),
            get_int(rootdir_data[i+22:i+24]),
            get_int(rootdir_data[i+24:i+26]),
            get_int(rootdir_data[i+28:i+32]),
        )

        dir_ents.append(dir_ent)


    for ent in dir_ents:
        start_offset = get_clus_N_offset(ent.fst_clus)
        size_clus = math.ceil(ent.file_size / sector_size)
        end_offset = get_clus_N_offset(ent.fst_clus + size_clus)
        print(f"file {ent.name}.{ent.ext} in {hex(start_offset)} ({ent.fst_clus}) - {hex(end_offset)}")
        # if ent.name == "FAT_WS":
        #     open(f"./{ent.name}.{ent.ext}", "wb").write(img[start_offset:end_offset])