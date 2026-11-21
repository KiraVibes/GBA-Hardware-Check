import os
import sys
import subprocess

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
    base_dir = os.path.dirname(os.path.abspath(__file__))
    tools_dir = os.path.join(base_dir, "tools")
    for root, dirs, files in os.walk(tools_dir):
        if tool_name + ".exe" in files:
            return os.path.join(root, tool_name + ".exe")
        if tool_name in files:
            return os.path.join(root, tool_name)
    return tool_name

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    mb_dir = os.path.join(base_dir, "multiboot_client")
    build_dir = os.path.join(base_dir, "build")
    inc_dir = os.path.join(base_dir, "include")

    os.makedirs(build_dir, exist_ok=True)

    gcc = find_tool("arm-none-eabi-gcc")
    objcopy = find_tool("arm-none-eabi-objcopy")

    src_dir = os.path.join(base_dir, "src")
    crt0_src = os.path.join(mb_dir, "crt0_mb.s")
    main_src = os.path.join(mb_dir, "mb_main.c")
    ld_script = os.path.join(mb_dir, "multiboot.ld")
    elf_out = os.path.join(build_dir, "mb_client.elf")
    bin_out = os.path.join(build_dir, "mb_client.bin")
    header_out = os.path.join(inc_dir, "multiboot_payload.h")

    suite_sources = [
        crt0_src,
        main_src,
        os.path.join(src_dir, "graphics.c"),
        os.path.join(src_dir, "font.c"),
        os.path.join(src_dir, "input.c"),
        os.path.join(src_dir, "display_test.c"),
        os.path.join(src_dir, "benchmark.c"),
        os.path.join(src_dir, "stress_test.c"),
        os.path.join(src_dir, "sysinfo.c"),
        os.path.join(src_dir, "help_guide.c"),
        os.path.join(src_dir, "audio_test.c"),
        os.path.join(src_dir, "rtc.c"),
        os.path.join(src_dir, "health_audit.c"),
        os.path.join(src_dir, "localization.c")
    ]

    print("Compiling MultiBoot client with full diagnostic suite...")
    cmd = [
        gcc,
        "-mcpu=arm7tdmi",
        "-mtune=arm7tdmi",
        "-marm",
        "-Os",
        "-ffreestanding",
        "-fomit-frame-pointer",
        "-Wall",
        "-DSTANDALONE_MULTIBOOT",
        f"-I{inc_dir}",
        "-nostartfiles",
        "-nodefaultlibs",
        "-T", ld_script
    ] + suite_sources + [
        "-lgcc",
        "-o", elf_out
    ]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        print("Compilation failed!")
        sys.exit(1)

    # Convert to raw binary
    cmd = [objcopy, "-O", "binary", elf_out, bin_out]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        print("Objcopy failed!")
        sys.exit(1)

    with open(bin_out, "r+b") as f:
        rom = bytearray(f.read())

    # Pad to multiple of 16 bytes (minimum 0x1C0 = 448 bytes)
    if len(rom) < 0x1C0:
        rom.extend(b'\x00' * (0x1C0 - len(rom)))
    rem = len(rom) % 16
    if rem != 0:
        rom.extend(b'\x00' * (16 - rem))

    # Patch Nintendo logo (0x04 to 0x9F)
    rom[0x04:0x04 + len(NINTENDO_LOGO)] = NINTENDO_LOGO

    # Patch Title (0xA0 to 0xAB)
    title = "GBA HW CHECK".encode('ascii')[:12].ljust(12, b'\x00')
    rom[0xA0:0xAC] = title

    # Patch Game Code (0xAC to 0xAF)
    rom[0xAC:0xB0] = b"MBDC"

    # Patch Maker Code (0xB0 to 0xB1)
    rom[0xB0:0xB2] = b"01"

    # Fixed 0x96
    rom[0xB2] = 0x96

    # Unit & device
    rom[0xB3] = 0x00
    rom[0xB4] = 0x00

    # Complement check (0xBD)
    chk = 0
    for i in range(0xA0, 0xBD):
        chk = (chk + rom[i]) & 0xFF
    complement = (-(0x19 + chk)) & 0xFF
    rom[0xBD] = complement

    # Write patched bin
    with open(bin_out, "wb") as f:
        f.write(rom)

    print(f"MultiBoot client built: {len(rom)} bytes ({len(rom)/1024:.2f} KB)")

    # Generate C header
    with open(header_out, "w") as f:
        f.write("/* Auto-generated MultiBoot payload */\n")
        f.write("#ifndef MULTIBOOT_PAYLOAD_H\n")
        f.write("#define MULTIBOOT_PAYLOAD_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define MULTIBOOT_PAYLOAD_SIZE {len(rom)}\n\n")
        f.write("static const uint8_t multiboot_payload[MULTIBOOT_PAYLOAD_SIZE] __attribute__((aligned(4))) = {\n")
        for i in range(0, len(rom), 16):
            chunk = rom[i:i+16]
            hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
            f.write(f"    {hex_str},\n")
        f.write("};\n\n")
        f.write("#endif /* MULTIBOOT_PAYLOAD_H */\n")

    print(f"Header generated: {header_out}")

if __name__ == "__main__":
    main()
