#!/usr/bin/env python3
"""Create a FAT32 SD image for aVisor - superfloppy format (no MBR)."""
import struct, sys, os, math, subprocess, tempfile, shutil

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} output.img file1 [file2 ...]")
        sys.exit(1)

    outpath = sys.argv[1]
    files = sys.argv[2:]
    img_size = 64 * 1024 * 1024

    with tempfile.NamedTemporaryFile(suffix='.img', delete=False) as tmp:
        tmpname = tmp.name
        tmp.write(b'\x00' * img_size)

    try:
        subprocess.run(['mkfs.fat', '-F', '32', '-s', '1', '-n', 'AVISOR', tmpname],
                       check=True, capture_output=True, text=True)

        img = bytearray(open(tmpname, 'rb').read())

        bytes_per_sector = struct.unpack_from('<H', img, 11)[0]
        sectors_per_cluster = img[13]
        reserved_sectors = struct.unpack_from('<H', img, 14)[0]
        num_fats = img[16]
        fat_size = struct.unpack_from('<I', img, 36)[0]
        root_cluster = struct.unpack_from('<I', img, 44)[0]
        cluster_size = bytes_per_sector * sectors_per_cluster
        fat_start = reserved_sectors * bytes_per_sector
        data_start = (reserved_sectors + num_fats * fat_size) * bytes_per_sector
        root_dir_offset = data_start + (root_cluster - 2) * cluster_size

        print(f"BPB: bps={bytes_per_sector} spc={sectors_per_cluster} "
              f"reserved={reserved_sectors} fats={num_fats} fatsize={fat_size} "
              f"root_cluster={root_cluster}")
        print(f"Layout: fat@{fat_start} data@{data_start} rootdir@{root_dir_offset}")

        def read_fat(cluster):
            off = fat_start + cluster * 4
            return struct.unpack_from('<I', img, off)[0] & 0x0FFFFFFF

        def write_fat(cluster, value):
            for i in range(num_fats):
                off = (reserved_sectors + i * fat_size) * bytes_per_sector + cluster * 4
                struct.pack_into('<I', img, off, value & 0x0FFFFFFF)

        next_free = root_cluster + 1
        while next_free < 0x0FFFFFF0 and read_fat(next_free) != 0:
            next_free += 1

        dir_entry_idx = 0
        for fpath in files:
            data = open(fpath, 'rb').read()
            fname = os.path.basename(fpath).upper()

            parts = fname.rsplit('.', 1)
            if len(parts) == 2:
                name8 = parts[0][:8].ljust(8)
                ext3 = parts[1][:3].ljust(3)
            else:
                name8 = fname[:8].ljust(8)
                ext3 = '   '
            sfn = name8 + ext3

            num_clusters = max(1, math.ceil(len(data) / cluster_size))
            start_cluster = next_free

            for i in range(num_clusters):
                c = next_free
                next_free += 1
                if i < num_clusters - 1:
                    write_fat(c, next_free)
                else:
                    write_fat(c, 0x0FFFFFFF)

                coff = data_start + (c - 2) * cluster_size
                chunk = data[i*cluster_size:(i+1)*cluster_size]
                img[coff:coff+len(chunk)] = chunk

            entry = bytearray(32)
            entry[0:11] = sfn.encode('ascii')
            entry[11] = 0x20
            struct.pack_into('<H', entry, 20, start_cluster >> 16)
            struct.pack_into('<H', entry, 26, start_cluster & 0xFFFF)
            struct.pack_into('<I', entry, 28, len(data))

            eoff = root_dir_offset + dir_entry_idx * 32
            img[eoff:eoff+32] = entry
            dir_entry_idx += 1

            print(f"  {sfn.strip():12s} cluster={start_cluster} size={len(data)} clusters={num_clusters}")

        with open(outpath, 'wb') as f:
            f.write(img)

        print(f"Created {outpath}")

    finally:
        os.unlink(tmpname)

if __name__ == '__main__':
    main()
