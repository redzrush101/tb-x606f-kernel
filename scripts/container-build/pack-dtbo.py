#!/usr/bin/env python3
"""Pack the target overlay as an Android DT table with an unsigned AVB footer."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
from repack import dt_table, align

source, output = map(Path, sys.argv[1:])
dtb = source.read_bytes()
blob = dt_table(dtb)
output.write_bytes(blob + bytes(align(len(blob), 2048) - len(blob)))
tool = Path(__file__).with_name('avbtool.py')
subprocess.run([sys.executable, str(tool), 'add_hash_footer', '--image', str(output),
                '--partition_name', 'dtbo', '--partition_size', str(8 * 1024 * 1024),
                '--algorithm', 'NONE', '--hash_algorithm', 'sha256',
                '--salt', hashlib.sha256(dtb).hexdigest()], check=True)
with tempfile.TemporaryDirectory() as verification:
    canonical = Path(verification) / 'dtbo.img'
    canonical.symlink_to(output.resolve())
    subprocess.run([sys.executable, str(tool), 'verify_image', '--image', str(canonical)], check=True)
