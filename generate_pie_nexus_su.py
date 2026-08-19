#!/usr/bin/env python3
import struct

def build_lollipop_pie_su():
    # Target: ARMv7-A Position Independent Executable (PIE) for Android 5.1.1 (API 22)
    # Must have:
    # 1. e_type = ET_DYN (3)
    # 2. PT_INTERP -> "/system/bin/linker\0"
    # 3. PT_LOAD segments properly page-aligned
    # 4. PT_DYNAMIC with DT_HASH, DT_STRTAB, DT_SYMTAB, DT_STRSZ, DT_SYMENT
    # 5. DT_HASH using standard SYSV hash table (compatible with Android 5.1.1 bionic linker)
    #
    # Code:
    # setresuid(0, 0, 0) / setuid(0) -> syscall __NR_setuid32 (213)
    # setresgid(0, 0, 0) / setgid(0) -> syscall __NR_setgid32 (214)
    # parse arguments: if argc > 2 and argv[1] == "-c", execve("/system/bin/sh", ["/system/bin/sh", "-c", argv[2], NULL], envp)
    # else if argc > 1, execve("/system/bin/sh", ["/system/bin/sh", "-c", argv[1], NULL], envp)
    # else execve("/system/bin/sh", ["/system/bin/sh", NULL], envp)

    # Let's write the ARM assembly for _start in position-independent manner:
    # On entry from kernel / linker for PIE:
    # sp points to: argc, argv[0], argv[1], ..., NULL, envp[0], ...
    #
    # ARM instructions (32-bit):
    code = bytearray()
    # 0x00: mov r0, #0
    code += struct.pack('<I', 0xe3a00000)
    # 0x04: mov r7, #214 (__NR_setgid32)
    code += struct.pack('<I', 0xe3a070d6)
    # 0x08: svc 0
    code += struct.pack('<I', 0xef000000)
    # 0x0C: mov r0, #0
    code += struct.pack('<I', 0xe3a00000)
    # 0x10: mov r7, #213 (__NR_setuid32)
    code += struct.pack('<I', 0xe3a070d5)
    # 0x14: svc 0
    code += struct.pack('<I', 0xef000000)
    # 0x18: ldr r4, [sp] (argc)
    code += struct.pack('<I', 0xe59d4000)
    # 0x1C: cmp r4, #1
    code += struct.pack('<I', 0xe3540001)
    # 0x20: ble .Ldefault_sh (argc <= 1)
    code += struct.pack('<I', 0xda00000a)
    # 0x24: ldr r5, [sp, #8] (argv[1])
    code += struct.pack('<I', 0xe59d5008)
    # 0x28: ldrb r6, [r5, #0]
    code += struct.pack('<I', 0xe5d56000)
    # 0x2C: cmp r6, #'-'
    code += struct.pack('<I', 0xe356002d)
    # 0x30: bne .Lcustom
    code += struct.pack('<I', 0x1a000004)
    # 0x34: ldrb r6, [r5, #1]
    code += struct.pack('<I', 0xe5d56001)
    # 0x38: cmp r6, #'c'
    code += struct.pack('<I', 0xe3560063)
    # 0x3C: bne .Lcustom
    code += struct.pack('<I', 0x1a000001)
    # 0x40: cmp r4, #2
    code += struct.pack('<I', 0xe3540002)
    # 0x44: bgt .Lhave_cmd2
    code += struct.pack('<I', 0xca000001)
    # 0x48: .Lcustom: ldr r2, [sp, #8] (argv[1])
    code += struct.pack('<I', 0xe59d2008)
    # 0x4C: b .Lrun_dash_c
    code += struct.pack('<I', 0xea000003)
    # 0x50: .Lhave_cmd2: ldr r2, [sp, #12] (argv[2])
    code += struct.pack('<I', 0xe59d200c)
    # 0x54: b .Lrun_dash_c
    code += struct.pack('<I', 0xea000001)
    # 0x58: .Ldefault_sh:
    # exec /system/bin/sh directly without -c
    # add r0, pc, #offset_to_str_sh
    code += struct.pack('<I', 0xe28f002c) # 0x58 + 8 + 0x2c = 0x8c (str_sh)
    # mov r1, #0
    code += struct.pack('<I', 0xe3a01000)
    # push {r0, r1}
    code += struct.pack('<I', 0xe92d0003)
    # mov r1, sp
    code += struct.pack('<I', 0xe1a0100d)
    # mov r2, #0
    code += struct.pack('<I', 0xe3a02000)
    # mov r7, #11
    code += struct.pack('<I', 0xe3a0700b)
    # svc 0
    code += struct.pack('<I', 0xef000000)
    # .Lrun_dash_c:
    # 0x74: add r0, pc, #offset_to_str_sh (0x8c - (0x74 + 8) = 0x10)
    code += struct.pack('<I', 0xe28f0010)
    # 0x78: add r1, pc, #offset_to_str_dash_c (0x84 - (0x78 + 8) = 4)
    code += struct.pack('<I', 0xe28f1004)
    # 0x7C: mov r3, #0
    code += struct.pack('<I', 0xe3a03000)
    # 0x80: push {r0, r1, r2, r3}
    code += struct.pack('<I', 0xe92d000f)
    # 0x84: str_dash_c: "-c\0\0"
    code += b"-c\0\0"
    # 0x88: mov r1, sp
    code += struct.pack('<I', 0xe1a0100d)
    # 0x8C: str_sh: "/system/bin/sh\0\0"
    code += b"/system/bin/sh\0\0"
    # 0x9C: mov r2, #0
    code += struct.pack('<I', 0xe3a02000)
    # 0xA0: mov r7, #11 (__NR_execve)
    code += struct.pack('<I', 0xe3a0700b)
    # 0xA4: svc 0
    code += struct.pack('<I', 0xef000000)
    # 0xA8: mov r0, #127
    code += struct.pack('<I', 0xe3a0007f)
    # 0xAC: mov r7, #1
    code += struct.pack('<I', 0xe3a07001)
    # 0xB0: svc 0
    code += struct.pack('<I', 0xef000000)

    # Pad code to multiple of 4
    while len(code) % 4 != 0:
        code += b'\x00'

    interp_str = b"/system/bin/linker\0"
    
    # Construct ELF sections and program headers
    # Layout in memory:
    # 0x0000 - 0x0034: ELF Header (52 bytes)
    # 0x0034 - 0x0114: Program Headers (7 * 32 = 224 bytes)
    # [PT_PHDR, PT_INTERP, PT_LOAD(RX), PT_LOAD(RW), PT_DYNAMIC, PT_GNU_RELRO, PT_ARM_EXIDX]
    # 0x0114: INTERP string
    # Followed by DYNAMIC table, HASH table, STRTAB, SYMTAB, CODE
    
    ehdr_sz = 52
    ph_count = 5
    phdr_sz = ph_count * 32
    
    off_interp = ehdr_sz + phdr_sz
    len_interp = len(interp_str)
    
    off_code = (off_interp + len_interp + 3) & ~3
    len_code = len(code)
    
    off_strtab = off_code + len_code
    strtab = b"\0_start\0/system/bin/linker\0"
    len_strtab = len(strtab)
    
    off_symtab = (off_strtab + len_strtab + 3) & ~3
    # Sym 0: null, Sym 1: _start (value = off_code)
    sym0 = b'\x00' * 16
    sym1 = struct.pack('<IIIBBH', 1, off_code, len_code, 0x12, 0, 1) # STB_GLOBAL, STT_FUNC
    symtab = sym0 + sym1
    len_symtab = len(symtab)
    
    # Standard SYSV Hash table for 2 symbols:
    # nbucket = 1, nchain = 2
    # bucket[0] = 1, chain[0] = 0, chain[1] = 0
    off_hash = off_symtab + len_symtab
    hash_table = struct.pack('<IIII', 1, 2, 1, 0) + struct.pack('<I', 0)
    len_hash = len(hash_table)
    
    # Dynamic table
    off_dynamic = (off_hash + len_hash + 3) & ~3
    dynamic_entries = [
        (4, off_hash),       # DT_HASH
        (5, off_strtab),     # DT_STRTAB
        (6, off_symtab),     # DT_SYMTAB
        (10, len_strtab),    # DT_STRSZ
        (11, 16),            # DT_SYMENT
        (0, 0)               # DT_NULL
    ]
    dynamic_bytes = bytearray()
    for tag, val in dynamic_entries:
        dynamic_bytes += struct.pack('<II', tag, val)
    len_dynamic = len(dynamic_bytes)
    
    total_size = off_dynamic + len_dynamic
    
    # ELF Header
    e_ident = b'\x7fELF\x01\x01\x01\x00' + b'\x00'*8
    e_type = 3          # ET_DYN (PIE)
    e_machine = 40      # EM_ARM
    e_version = 1
    e_entry = off_code
    e_phoff = ehdr_sz
    e_shoff = 0
    e_flags = 0x05000000 # EF_ARM_EABI_VER5
    e_ehsize = ehdr_sz
    e_phentsize = 32
    e_phnum = ph_count
    e_shentsize = 0
    e_shnum = 0
    e_shstrndx = 0

    ehdr = struct.pack('<16sHHIIIIIHHHHHH',
        e_ident, e_type, e_machine, e_version, e_entry,
        e_phoff, e_shoff, e_flags, e_ehsize, e_phentsize,
        e_phnum, e_shentsize, e_shnum, e_shstrndx
    )

    # Program Headers:
    # 0: PT_PHDR
    ph_phdr = struct.pack('<IIIIIIII', 6, ehdr_sz, ehdr_sz, ehdr_sz, phdr_sz, phdr_sz, 4, 4)
    # 1: PT_INTERP
    ph_interp = struct.pack('<IIIIIIII', 3, off_interp, off_interp, off_interp, len_interp, len_interp, 4, 1)
    # 2: PT_LOAD (RX) - covers everything up to off_dynamic
    ph_load_rx = struct.pack('<IIIIIIII', 1, 0, 0, 0, off_dynamic, off_dynamic, 5, 0x1000)
    # 3: PT_LOAD (RW) - covers dynamic table
    ph_load_rw = struct.pack('<IIIIIIII', 1, off_dynamic, off_dynamic, off_dynamic, len_dynamic, len_dynamic, 6, 0x1000)
    # 4: PT_DYNAMIC
    ph_dynamic = struct.pack('<IIIIIIII', 2, off_dynamic, off_dynamic, off_dynamic, len_dynamic, len_dynamic, 6, 4)

    ph_all = ph_phdr + ph_interp + ph_load_rx + ph_load_rw + ph_dynamic

    binary = bytearray(total_size)
    binary[0:ehdr_sz] = ehdr
    binary[ehdr_sz:ehdr_sz + phdr_sz] = ph_all
    binary[off_interp:off_interp + len_interp] = interp_str
    binary[off_code:off_code + len_code] = code
    binary[off_strtab:off_strtab + len_strtab] = strtab
    binary[off_symtab:off_symtab + len_symtab] = symtab
    binary[off_hash:off_hash + len_hash] = hash_table
    binary[off_dynamic:off_dynamic + len_dynamic] = dynamic_bytes

    return bytes(binary)

if __name__ == '__main__':
    binary = build_lollipop_pie_su()
    with open('APKNexusADBWatchdog-2.4/factory/nexus_su', 'wb') as f:
        f.write(binary)
    with open('APKNexusADBWatchdog-2.4/app/src/main/assets/native/armeabi-v7a/nexus_su', 'wb') as f:
        f.write(binary)
    with open('APKNexusADBWatchdog-2.4/app/src/main/assets/nexus_su', 'wb') as f:
        f.write(binary)
    print(f'Successfully built Lollipop PIE ARM ELF nexus_su ({len(binary)} bytes)')
