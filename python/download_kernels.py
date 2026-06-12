"""
Download NASA SPICE kernels required for SPICE validation (notebook 07).

Usage:
    python3 python/download_kernels.py

Downloads to data/spice/ (gitignored, ~115MB total).
"""

import os
import urllib.request

KERNEL_DIR = os.path.join(os.path.dirname(__file__), '..', 'data', 'spice')

KERNELS = [
    ('naif0012.tls', 'https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls'),
    ('pck00011.tpc', 'https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/pck00011.tpc'),
    ('de440.bsp',    'https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440.bsp'),
]

def download():
    os.makedirs(KERNEL_DIR, exist_ok=True)
    for filename, url in KERNELS:
        dest = os.path.join(KERNEL_DIR, filename)
        if os.path.exists(dest):
            print(f'  {filename} already exists, skipping')
            continue
        print(f'  Downloading {filename}...')
        urllib.request.urlretrieve(url, dest)
        size_mb = os.path.getsize(dest) / 1e6
        print(f'  {filename} done ({size_mb:.1f} MB)')

if __name__ == '__main__':
    print('Downloading SPICE kernels to data/spice/')
    download()
    print('Done. Total kernels ready.')
