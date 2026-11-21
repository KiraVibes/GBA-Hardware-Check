import os
import sys
import subprocess
import glob
import shutil

# Standard 156-byte Nintendo Logo required by GBA BIOS
NINTENDO_LOGO = bytes([
    0x24, 0xFF, 0xAE, 0x51, 0x69, 0x9A, 0xA2, 0x21, 0x3D, 0x84, 0x82, 0x0A, 0x84, 0xE4, 0x09, 0xAD,
    0x11, 0x24, 0x8B, 0x98, 0xC0, 0x81, 0x7F, 0x21, 0xA3, 0x52, 0xBE, 0x19, 0x93, 0x09, 0xCE, 0x20,
    0x10, 0x46, 0x4A, 0x4A, 0xF8, 0x27, 0x31, 0xEC, 0x58, 0xC7, 0xE8, 0x33, 0x82, 0xE3, 0xCE, 0xBF,
    0x85, 0xF4, 0xDF, 0x94, 0xCE, 0x4B, 0x09, 0xC1, 0x94, 0x56, 0x8A, 0xC0, 0x13, 0x72, 0xA7, 0xFC,
    0x9F, 0x84, 0x4D, 0x73, 0xA3, 0xCA, 0x9A, 0x61, 0x58, 0x97, 0xA3, 0x27, 0xFC, 0x03, 0x98, 0x76,
    0x23, 0x1D, 0xC7, 0x61, 0x03, 0x04, 0xAE, 0x56, 0xBF, 0x38, 0x84, 0x00, 0x40, 0xA7, 0x0E, 0xFD,
    0xFF, 0x52, 0xFE, 0x03, 0x6F, 0x95, 0x30, 0xF1, 0x97, 0xFB, 0xC0, 0x85, 0x60, 0xD6, 0x80, 0x25,
    0xA9, 0x63, 0xBE, 0x03, 0x01, 0x4E, 0x38, 0xE2, 0xF9, 0xA2, 0x34, 0xFF, 0xBB, 0x3E, 0x03, 0x44,
    0x78, 0x00, 0x90, 0xCB, 0x88, 0x11, 0x3A, 0x94, 0x65, 0xC0, 0x7C, 0x63, 0x87, 0xF0, 0x3C, 0xAF,
    0xD6, 0x25, 0xE4, 0x8B, 0x38, 0x0A, 0xAC, 0x72, 0x21, 0xD4, 0xF8, 0x07
])

def find_tool(tool_name):
    # Check in tools/ directory first
    tools_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tools")
    if os.path.isdir(tools_dir):
        for root, dirs, files in os.walk(tools_dir):
            if tool_name + ".exe" in files:
                return os.path.join(root, tool_name + ".exe")
            if tool_name in files:
                return os.path.join(root, tool_name)
    # Check PATH
    return shutil.which(tool_name)

def ensure_toolchain():
    gcc = find_tool("arm-none-eabi-gcc")
    if gcc:
        return gcc
    print("[INFO] Portable ARM GCC toolchain not found. Running setup_toolchain.py...")
    setup_script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "setup_toolchain.py")
    if os.path.exists(setup_script):
        res = subprocess.run([sys.executable, setup_script])
        if res.returncode == 0:
            gcc = find_tool("arm-none-eabi-gcc")
            if gcc:
                return gcc
    print("[ERROR] arm-none-eabi-gcc not found!")
    print("        Please run 'python setup_toolchain.py' (Windows) or install arm-none-eabi-gcc on your PATH.")
    sys.exit(1)

def fix_gba_header(rom_path, title="GBA HW CHECK", game_code="AGBC", maker_code="01"):
    with open(rom_path, "r+b") as f:
        rom = bytearray(f.read())

        # Ensure minimum header size
        if len(rom) < 0xC0:
            rom.extend(b'\x00' * (0xC0 - len(rom)))

        # 1. Patch Nintendo Logo (0x04 to 0x9F, 156 bytes)
        rom[0x04:0x04 + len(NINTENDO_LOGO)] = NINTENDO_LOGO

        # 2. Patch Title (0xA0 to 0xAB, 12 bytes)
        title_bytes = title.encode('ascii')[:12].ljust(12, b'\x00')
        rom[0xA0:0xAC] = title_bytes

        # 3. Patch Game Code (0xAC to 0xAF, 4 bytes)
        rom[0xAC:0xB0] = game_code.encode('ascii')[:4].ljust(4, b'\x00')

        # 4. Patch Maker Code (0xB0 to 0xB1, 2 bytes)
        rom[0xB0:0xB2] = maker_code.encode('ascii')[:2].ljust(2, b'\x00')

        # 5. Fixed byte (0xB2 = 0x96)
        rom[0xB2] = 0x96

        # 6. Complement check (0xBD)
        chk = 0
        for i in range(0xA0, 0xBD):
            chk = (chk + rom[i]) & 0xFF
        complement = (-(0x19 + chk)) & 0xFF
        rom[0xBD] = complement

        # Pad ROM to a multiple of 256 bytes
        pad = (256 - (len(rom) % 256)) % 256
        if pad > 0:
            rom.extend(b'\xFF' * pad)

        f.seek(0)
        f.write(rom)
        f.truncate()

    print(f"Header patched: Complement=0x{complement:02X}, ROM Size={len(rom)} bytes ({len(rom)/1024:.1f} KB)")

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(base_dir, "build")
    src_dir = os.path.join(base_dir, "src")
    inc_dir = os.path.join(base_dir, "include")
    linker_script = os.path.join(base_dir, "gba.ld")
    output_elf = os.path.join(build_dir, "gba_hw_check.elf")
    output_gba = os.path.join(base_dir, "gba_hw_check.gba")

    os.makedirs(build_dir, exist_ok=True)

    gcc = ensure_toolchain()
    objcopy = find_tool("arm-none-eabi-objcopy")
    size_tool = find_tool("arm-none-eabi-size")
    if not objcopy or not size_tool:
        print("[ERROR] arm-none-eabi-objcopy or arm-none-eabi-size not found!")
        sys.exit(1)

    print(f"Using GCC: {gcc}")

    c_flags = [
        "-mcpu=arm7tdmi",
        "-mtune=arm7tdmi",
        "-marm",
        "-O2",
        "-ffreestanding",
        "-fomit-frame-pointer",
        "-Wall",
        "-Wextra",
        f"-I{inc_dir}"
    ]

    # Build MultiBoot client payload if needed
    mb_builder = os.path.join(base_dir, "build_mb_client.py")
    if os.path.exists(mb_builder):
        subprocess.run([sys.executable, mb_builder], check=True)

    objects = []

    # Compile crt0.s
    crt0_src = os.path.join(src_dir, "crt0.s")
    crt0_obj = os.path.join(build_dir, "crt0.o")
    print("Assembling crt0.s...")
    cmd = [gcc] + c_flags + ["-c", crt0_src, "-o", crt0_obj]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        print("Assembly failed!")
        sys.exit(1)
    objects.append(crt0_obj)

    # Compile C sources
    c_sources = glob.glob(os.path.join(src_dir, "*.c"))
    for src in c_sources:
        obj_name = os.path.splitext(os.path.basename(src))[0] + ".o"
        obj_path = os.path.join(build_dir, obj_name)
        print(f"Compiling {os.path.basename(src)}...")
        cmd = [gcc] + c_flags + ["-c", src, "-o", obj_path]
        res = subprocess.run(cmd)
        if res.returncode != 0:
            print(f"Compilation of {src} failed!")
            sys.exit(1)
        objects.append(obj_path)

    # Link ELF
    print("Linking binary...")
    ld_flags = [
        "-T", linker_script,
        "-nostartfiles",
        "-nodefaultlibs",
        "-lgcc"
    ]
    cmd = [gcc] + c_flags + objects + ld_flags + ["-o", output_elf]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        print("Linking failed!")
        sys.exit(1)

    # Report size
    subprocess.run([size_tool, output_elf])

    # Convert ELF to GBA raw binary in build/temp.gba first
    temp_gba = os.path.join(build_dir, "temp.gba")
    print(f"Creating GBA ROM: {output_gba}...")
    cmd = [objcopy, "-O", "binary", output_elf, temp_gba]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        print("Objcopy failed!")
        sys.exit(1)

    # Patch Nintendo Header & complement checksum (Game code V49E for Drill Dozer Hardware Rumble autodetection)
    fix_gba_header(temp_gba, title="GBA HW CHECK", game_code="V49E", maker_code="01")

    # Copy to output path (fall back gracefully if locked by an emulator)
    import shutil
    try:
        shutil.copyfile(temp_gba, output_gba)
        print("\nSUCCESS! Game Boy Advance ROM generated:")
        print(f"-> {output_gba}")
    except OSError:
        updated_gba = os.path.join(base_dir, "gba_hw_check_updated.gba")
        shutil.copyfile(temp_gba, updated_gba)
        print("\n[WARNING] gba_hw_check.gba is currently locked by a running emulator!")
        print("Freshly built ROM saved instead to:")
        print(f"-> {updated_gba}")
        print(f"-> {temp_gba}")

if __name__ == "__main__":
    main()
