#!/usr/bin/env python3
"""Replace only the kernel in a stock Android boot-v2 image, then renew AVB."""
import gzip
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile
import zlib

import avbtool


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def align(n, page):
    return (n + page - 1) // page * page


def unpack(data):
    require(data[:8] == b'ANDROID!', 'Input is not an Android boot image')
    fields = struct.unpack_from('<10I', data, 8)
    ks, ka, rs, ra, ss, sa, ta, page, version, os_version = fields
    require(version == 2, 'This script requires a stock boot header v2')
    require(page in (2048, 4096, 8192, 16384), 'Invalid page size')
    recovery_size, recovery_offset, header_size, dtb_size, dtb_addr = \
        struct.unpack_from('<IQIIQ', data, 1632)
    require(header_size == 1660, 'Unexpected v2 header size')
    require(len(data) >= page, 'Truncated header page')
    pos = page
    parts = {}
    for name, size in [('kernel', ks), ('ramdisk', rs), ('second', ss),
                       ('recovery_dtbo', recovery_size), ('dtb', dtb_size)]:
        if name == 'recovery_dtbo' and size:
            require(pos == recovery_offset, 'Unexpected recovery DTBO offset')
        require(pos + size <= len(data), 'Truncated ' + name)
        parts[name] = data[pos:pos + size]
        pos += align(size, page)
    info = dict(zip(('kernel_size', 'kernel_addr', 'ramdisk_size', 'ramdisk_addr',
                     'second_size', 'second_addr', 'tags_addr', 'page_size',
                     'header_version', 'os_version'), fields))
    info.update(dtb_size=dtb_size, dtb_addr=dtb_addr,
                recovery_dtbo_size=recovery_size,
                name=data[48:64].split(b'\0')[0].decode(),
                cmdline=(data[64:576].split(b'\0')[0] +
                         data[608:1632].split(b'\0')[0]).decode())
    return parts, info


def split_gzip(data):
    decoder = zlib.decompressobj(31)
    raw = decoder.decompress(data)
    require(decoder.eof, 'Truncated gzip kernel')
    return data[:len(data) - len(decoder.unused_data)], decoder.unused_data, raw


def main(stock_path, kernel_path, output_path):
    stock_path, kernel_path, output = map(Path, (stock_path, kernel_path, output_path))
    if zipfile.is_zipfile(stock_path):
        with zipfile.ZipFile(stock_path) as archive:
            matches = [n for n in archive.namelist() if Path(n).name == 'boot.img']
            require(len(matches) == 1, 'Firmware ZIP must contain exactly one boot.img')
            stock = archive.read(matches[0])
    else:
        stock = stock_path.read_bytes()
        if stock[:2] == b'\x1f\x8b':
            stock = gzip.decompress(stock)
    parts, original = unpack(stock)
    page = original['page_size']
    _, appended_dtb, _ = split_gzip(parts['kernel'])
    require(appended_dtb[:4] == b'\xd0\x0d\xfe\xed', 'Missing appended stock DTB')
    new_gzip, extra, raw = split_gzip(kernel_path.read_bytes())
    require(not extra, 'Expected Image.gz without an appended DTB')
    require(raw[56:60] == b'ARM\x64', 'Expected an ARM64 kernel Image')
    parts['kernel'] = new_gzip + appended_dtb
    header = bytearray(stock[:page])
    struct.pack_into('<I', header, 8, len(parts['kernel']))
    # Android v0/v1/v2 boot ID: SHA1 of each component and its LE32 size.
    digest = hashlib.sha1()
    for name in ('kernel', 'ramdisk', 'second', 'recovery_dtbo', 'dtb'):
        digest.update(parts[name])
        digest.update(struct.pack('<I', len(parts[name])))
    header[576:608] = digest.digest() + bytes(12)
    if parts['recovery_dtbo']:
        recovery_offset = page + sum(align(len(parts[n]), page)
                                     for n in ('kernel', 'ramdisk', 'second'))
        struct.pack_into('<Q', header, 1636, recovery_offset)
    payload = bytes(header)
    for name in ('kernel', 'ramdisk', 'second', 'recovery_dtbo', 'dtb'):
        component = parts[name]
        payload += component + bytes(align(len(component), page) - len(component))
    output.parent.mkdir(parents=True, exist_ok=True)
    tool = Path(__file__).with_name('avbtool.py')
    with tempfile.TemporaryDirectory() as temporary:
        stock_file = Path(temporary) / 'stock.img'
        stock_file.write_bytes(stock)
        footer, avb_header, descriptors, _ = avbtool.Avb()._parse_image(
            avbtool.ImageHandler(str(stock_file), read_only=True))
        require(footer is not None, 'Expected a stock AVB footer')
        hashes = [d for d in descriptors if isinstance(d, avbtool.AvbHashDescriptor)]
        require(len(hashes) == 1 and hashes[0].partition_name == 'boot',
                'Expected one AVB boot hash descriptor')
        h = hashes[0]
        require(h.flags in (0, 1), 'Unsupported AVB hash flags')
        output.write_bytes(payload)
        command = [sys.executable, str(tool), 'add_hash_footer', '--image', str(output),
                   '--partition_size', str(len(stock)), '--partition_name', 'boot',
                   '--algorithm', 'NONE', '--hash_algorithm', h.hash_algorithm,
                   '--salt', h.salt.hex(), '--rollback_index', str(avb_header.rollback_index),
                   '--rollback_index_location', str(avb_header.rollback_index_location)]
        if h.flags & 1:
            command += ['--do_not_use_ab']
        for d in descriptors:
            if isinstance(d, avbtool.AvbPropertyDescriptor):
                command += ['--prop', d.key + ':' + d.value.decode()]
            else:
                require(isinstance(d, avbtool.AvbHashDescriptor), 'Unsupported AVB descriptor')
        subprocess.run(command, check=True)
    subprocess.run([sys.executable, str(tool), 'verify_image', '--image', str(output)], check=True)
    result = output.read_bytes()
    verified, current = unpack(result)
    for name in ('ramdisk', 'second', 'recovery_dtbo', 'dtb'):
        require(verified[name] == parts[name], name + ' changed during repacking')
    require(verified['kernel'] == parts['kernel'], 'Kernel changed during repacking')
    require(len(result) == len(stock), 'Partition size changed')
    require({k: v for k, v in current.items() if k != 'kernel_size'} ==
            {k: v for k, v in original.items() if k != 'kernel_size'},
            'Stock boot parameters changed')
    manifest = dict(stock_sha256=sha(stock), output_sha256=sha(result),
                    partition_size=len(stock), stock_header=original, output_header=current,
                    preserved_sha256={n: sha(parts[n]) for n in
                                      ('ramdisk', 'second', 'recovery_dtbo', 'dtb')},
                    appended_stock_dtb_sha256=sha(appended_dtb),
                    kernel_version=raw[raw.index(b'Linux version '):].split(b'\0')[0].decode(),
                    avb='Regenerated unsigned hash footer; Lenovo signature cannot be retained')
    output.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit('Usage: repack.py stock.zip-or-boot.img Image.gz output.img')
    main(*sys.argv[1:])
