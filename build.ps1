$ErrorActionPreference = "Stop"

$BaseDir = $PSScriptRoot
if (-not $BaseDir) { $BaseDir = (Get-Location).Path }
$BuildDir = Join-Path $BaseDir "build"
$SrcDir = Join-Path $BaseDir "src"
$IncDir = Join-Path $BaseDir "include"
$LinkerScript = Join-Path $BaseDir "gba.ld"
$OutputElf = Join-Path $BuildDir "gba_hw_check.elf"
$OutputGba = Join-Path $BaseDir "gba_hw_check.gba"

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

$ToolsDir = Join-Path $BaseDir "tools\xpack-arm-none-eabi-gcc-14.2.1-1.1\bin"
$Gcc = Join-Path $ToolsDir "arm-none-eabi-gcc.exe"
$Objcopy = Join-Path $ToolsDir "arm-none-eabi-objcopy.exe"
$SizeTool = Join-Path $ToolsDir "arm-none-eabi-size.exe"

Write-Host "Compiling GBA Hardware Check..." -ForegroundColor Cyan

# 0. Rebuild MultiBoot client payload so multiboot_payload.h is always up to date
$MbBuilder = Join-Path $BaseDir "build_mb_client.py"
if ((Test-Path $MbBuilder) -and (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Host "Building MultiBoot client payload..." -ForegroundColor Cyan
    & python $MbBuilder
    if ($LASTEXITCODE -ne 0) { throw "MultiBoot client build failed" }
}

$CFlags = @(
    "-mcpu=arm7tdmi",
    "-mtune=arm7tdmi",
    "-marm",
    "-O2",
    "-ffreestanding",
    "-fomit-frame-pointer",
    "-Wall",
    "-Wextra",
    "-I$IncDir"
)

$Objects = @()

# 1. Assemble crt0.s
$Crt0Src = Join-Path $SrcDir "crt0.s"
$Crt0Obj = Join-Path $BuildDir "crt0.o"
Write-Host "Assembling crt0.s..."
& $Gcc @CFlags -c $Crt0Src -o $Crt0Obj
if ($LASTEXITCODE -ne 0) { throw "Assembly of crt0.s failed" }
$Objects += $Crt0Obj

# 2. Compile all C sources
$CSources = Get-ChildItem -Path $SrcDir -Filter "*.c"
foreach ($src in $CSources) {
    $objName = [System.IO.Path]::GetFileNameWithoutExtension($src.Name) + ".o"
    $objPath = Join-Path $BuildDir $objName
    Write-Host "Compiling $($src.Name)..."
    & $Gcc @CFlags -c $src.FullName -o $objPath
    if ($LASTEXITCODE -ne 0) { throw "Compilation of $($src.Name) failed" }
    $Objects += $objPath
}

# 3. Link ELF
Write-Host "Linking binary..."
$LdFlags = @(
    "-T", $LinkerScript,
    "-nostartfiles",
    "-nodefaultlibs",
    "-lgcc"
)
& $Gcc @CFlags @Objects @LdFlags -o $OutputElf
if ($LASTEXITCODE -ne 0) { throw "Linking failed" }

& $SizeTool $OutputElf

# 4. Convert ELF to raw binary
$TempGba = Join-Path $BuildDir "temp.gba"
Write-Host "Creating GBA ROM: $OutputGba..."
& $Objcopy -O binary $OutputElf $TempGba
if ($LASTEXITCODE -ne 0) { throw "Objcopy failed" }

# 5. Patch Nintendo Logo and Complement Checksum
[byte[]]$Logo = @(
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
)

[byte[]]$Rom = [System.IO.File]::ReadAllBytes($TempGba)
if ($Rom.Length -lt 0xC0) {
    [Array]::Resize([ref]$Rom, 0xC0)
}

# Copy logo (0x04..0x9F)
for ($i = 0; $i -lt $Logo.Length; $i++) {
    $Rom[0x04 + $i] = $Logo[$i]
}

# Title: "GBA HW CHECK" (0xA0..0xAB)
$Title = [System.Text.Encoding]::ASCII.GetBytes("GBA HW CHECK")
for ($i = 0; $i -lt 12; $i++) {
    $Rom[0xA0 + $i] = if ($i -lt $Title.Length) { $Title[$i] } else { 0 }
}

# Game Code: "V49E" (0xAC..0xAF) - Hardware Rumble ID (Drill Dozer)
$GameCode = [System.Text.Encoding]::ASCII.GetBytes("V49E")
for ($i = 0; $i -lt 4; $i++) {
    $Rom[0xAC + $i] = $GameCode[$i]
}

# Maker Code: "01" (0xB0..0xB1)
$Rom[0xB0] = 0x30
$Rom[0xB1] = 0x31

# Fixed byte 0xB2 = 0x96
$Rom[0xB2] = 0x96

# Complement checksum at 0xBD
$chk = 0
for ($i = 0xA0; $i -lt 0xBD; $i++) {
    $chk = ($chk + $Rom[$i]) -band 0xFF
}
$complement = (-(0x19 + $chk)) -band 0xFF
$Rom[0xBD] = [byte]$complement

# Pad to multiple of 256 bytes
$pad = (256 - ($Rom.Length % 256)) % 256
if ($pad -gt 0) {
    $oldLen = $Rom.Length
    [Array]::Resize([ref]$Rom, $oldLen + $pad)
    for ($i = $oldLen; $i -lt $Rom.Length; $i++) {
        $Rom[$i] = 0xFF
    }
}

[System.IO.File]::WriteAllBytes($TempGba, $Rom)

$CopiedMain = $false
try {
    Copy-Item -Path $TempGba -Destination $OutputGba -Force -ErrorAction Stop
    $CopiedMain = $true
} catch {
    $AltGba = Join-Path $BaseDir "gba_hw_check_updated.gba"
    Copy-Item -Path $TempGba -Destination $AltGba -Force
    Write-Warning "gba_hw_check.gba is currently locked by a running emulator! Fresh ROM saved as: $AltGba and $TempGba"
}

Write-Host "Header patched: Complement=0x$($complement.ToString('X2')), ROM Size=$($Rom.Length) bytes ($([Math]::Round($Rom.Length/1024, 1)) KB)" -ForegroundColor Green
Write-Host "`nSUCCESS! Game Boy Advance ROM generated:" -ForegroundColor Green
if ($CopiedMain) {
    Write-Host "-> $OutputGba" -ForegroundColor Yellow
} else {
    Write-Host "-> $AltGba (and $TempGba)" -ForegroundColor Yellow
}
