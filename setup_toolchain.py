import os
import sys
import urllib.request
import zipfile
import shutil

TOOLCHAIN_URL = "https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v14.2.1-1.1/xpack-arm-none-eabi-gcc-14.2.1-1.1-win32-x64.zip"
TOOLS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tools")
ZIP_PATH = os.path.join(TOOLS_DIR, "arm-toolchain.zip")
GCC_BIN_DIR = None

def progress_hook(count, block_size, total_size):
    if total_size > 0:
        percent = int(count * block_size * 100 / total_size)
        mb_down = (count * block_size) / (1024 * 1024)
        mb_total = total_size / (1024 * 1024)
        sys.stdout.write(f"\rDownloading: {percent}% ({mb_down:.1f} MB / {mb_total:.1f} MB)")
        sys.stdout.flush()

def setup():
    os.makedirs(TOOLS_DIR, exist_ok=True)
    
    # Check if gcc already exists
    for root, dirs, files in os.walk(TOOLS_DIR):
        if "arm-none-eabi-gcc.exe" in files:
            print(f"ARM toolchain already found in: {root}")
            return root

    print(f"Fetching ARM GCC toolchain from {TOOLCHAIN_URL}...")
    headers = {'User-Agent': 'Mozilla/5.0'}
    req = urllib.request.Request(TOOLCHAIN_URL, headers=headers)
    with urllib.request.urlopen(req) as response, open(ZIP_PATH, 'wb') as out_file:
        total_size = int(response.info().get('Content-Length', -1))
        count = 0
        block_size = 1024 * 1024
        while True:
            chunk = response.read(block_size)
            if not chunk:
                break
            out_file.write(chunk)
            count += 1
            progress_hook(count, block_size, total_size)
    print("\nDownload complete. Extracting archive...")
    
    with zipfile.ZipFile(ZIP_PATH, 'r') as zip_ref:
        zip_ref.extractall(TOOLS_DIR)
        
    try:
        os.remove(ZIP_PATH)
    except Exception:
        pass
        
    # Locate bin directory
    for root, dirs, files in os.walk(TOOLS_DIR):
        if "arm-none-eabi-gcc.exe" in files:
            print(f"Toolchain installed successfully at: {root}")
            return root
            
    print("Warning: arm-none-eabi-gcc.exe not found in extracted files!")
    return None

if __name__ == "__main__":
    setup()
