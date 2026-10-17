#!/bin/bash
# Cross-build mbrgptnt.sys with clang + lld-link on a Linux host.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src"
DEFS=""
[ "${GPTNT_DBG:-0}" = "1" ] && DEFS="$DEFS -DGPTNT_DBG"
CFLAGS="--target=i686-pc-windows-msvc -march=i486 -ffreestanding -fno-stack-protector -fno-builtin -fno-zero-initialized-in-bss -O1 -Wall $DEFS"

CFLAGS64="--target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -fno-builtin -fno-zero-initialized-in-bss -mno-red-zone -mgeneral-regs-only -O1 -Wall $DEFS"

# x64: Windows XP x64 / Server 2003 x64 and later
build_x64() {
    local out="$ROOT/build/x64" o s
    mkdir -p "$out"
    echo "[*] x64: compiling"
    for o in gptcore ldmcore mgptdrv; do clang $CFLAGS64 -c "$SRC/$o.c" -o "$out/$o.obj"; done
    local prov und def="$out/ntoskrnl.def"
    prov=$(llvm-nm --defined-only "$out"/*.obj 2>/dev/null | awk '{print $NF}' | sort -u)
    und=$(llvm-nm --undefined-only "$out"/*.obj 2>/dev/null | awk '{print $NF}' | sort -u)
    { echo "LIBRARY ntoskrnl.exe"; echo "EXPORTS"; } > "$def"
    for s in $und; do echo "$prov" | grep -qx "$s" && continue; echo "$s" >> "$def"; done
    llvm-dlltool -m i386:x86-64 -d "$def" -l "$out/ntoskrnl.lib"
    echo "[*] x64: linking"
    lld-link /nologo /driver /subsystem:native,5.02 /entry:DriverEntry /base:0x10000 /nodefaultlib \
        /out:"$out/mbrgptnt.sys" "$out/gptcore.obj" "$out/ldmcore.obj" "$out/mgptdrv.obj" "$out/ntoskrnl.lib"
    python3 - "$out/mbrgptnt.sys" <<'PY'
import sys, pefile
p = pefile.PE(sys.argv[1]); p.OPTIONAL_HEADER.CheckSum = p.generate_checksum(); p.write(sys.argv[1])
print("    x64 checksum=0x%08X %d bytes" % (p.OPTIONAL_HEADER.CheckSum, len(open(sys.argv[1],'rb').read())))
PY
}

build_one() {
    local target="$1" subver
    case "$target" in
        nt31) subver="native,3.10" ;;
        nt35) subver="native,3.50" ;;
        nt40) subver="native,4.00" ;;
        x64) build_x64; return ;;
        *) echo "unknown target $target"; return 1 ;;
    esac
    local out="$ROOT/build/$target"
    mkdir -p "$out"
    echo "[*] $target: compiling"
    clang $CFLAGS -c "$SRC/gptcore.c" -o "$out/gptcore.obj"
    clang $CFLAGS -c "$SRC/ldmcore.c" -o "$out/ldmcore.obj"
    clang $CFLAGS -c "$SRC/mgptdrv.c" -o "$out/mgptdrv.obj"
    local prov und def="$out/ntoskrnl.def" s
    prov=$(llvm-nm --defined-only "$out/gptcore.obj" "$out/ldmcore.obj" "$out/mgptdrv.obj" 2>/dev/null | awk '{print $NF}' | sort -u)
    und=$(llvm-nm --undefined-only "$out/gptcore.obj" "$out/ldmcore.obj" "$out/mgptdrv.obj" 2>/dev/null | awk '{print $NF}' | grep -E '^_' | sort -u)
    { echo "LIBRARY ntoskrnl.exe"; echo "EXPORTS"; } > "$def"
    for s in $und; do echo "$prov" | grep -qx "$s" && continue; echo "${s#_}" >> "$def"; done
    llvm-dlltool -m i386 --kill-at -d "$def" -l "$out/ntoskrnl.lib"
    echo "[*] $target: linking"
    lld-link /nologo /driver /subsystem:$subver /entry:DriverEntry@8 /base:0x10000 /nodefaultlib \
        /out:"$out/mbrgptnt.sys" "$out/gptcore.obj" "$out/ldmcore.obj" "$out/mgptdrv.obj" "$out/ntoskrnl.lib"
    python3 - "$out/mbrgptnt.sys" <<'PY'
import sys, pefile
p = pefile.PE(sys.argv[1]); p.OPTIONAL_HEADER.CheckSum = p.generate_checksum(); p.write(sys.argv[1])
print("    %s checksum=0x%08X %d bytes" % (sys.argv[1].split('/')[-2], p.OPTIONAL_HEADER.CheckSum, len(open(sys.argv[1],'rb').read())))
PY
}

T="${1:-all}"
if [ "$T" = "all" ]; then for t in nt31 nt35 nt40 x64; do build_one "$t"; done
else build_one "$T"; fi
echo "[*] done"
