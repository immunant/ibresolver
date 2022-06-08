import csv
from elftools.elf.elffile import ELFFile

def open_elf(filename):
    f = open(filename + ".elf", 'rb')
    return ELFFile(f)

def open_csv(filename):
    f = open(filename + ".csv", newline='')
    return csv.reader(f)

def sym_addr(filename, sym_name):
    """
    Gets the address of the first symbol matching `sym_name` in `filename + ".elf"`
    """
    elf = open_elf(filename)
    symtab = elf.get_section_by_name('.symtab')
    syms = symtab.get_symbol_by_name(sym_name)
    if len(syms) == 0:
        return None
    return syms[0]['st_value']

def check_jump(filename, origin, dst):
    """
    Checks if an indirect jump from `origin` to `dst` was recorded in the output
    file `filename + ".csv"`
    """
    elf = open_elf(filename)
    statically_compiled = elf.header['e_type'] == "ET_EXEC"
    header_line = True
    for row in open_csv(filename):
        if header_line:
            header_line = False
            continue
        call_origin = row[2] if statically_compiled else row[0]
        call_dst = row[3] if statically_compiled else row[1]
        if origin == int(call_origin, 16):
            if dst == int(call_dst, 16):
                return True
    return False

def check_jump_to_sym(prefix, callsite, callee_sym, callee_is_thumb=False):
    """
    Check for indirect calls from the instruction at `callsite` (found manually
    using objdump) to a function named `callee_sym`.
    """
    fn_addr = sym_addr(prefix, callee_sym)
    if callee_is_thumb:
        fn_addr &= 0xFFFFFFFE
    print(callee_sym)
    print(hex(callsite))
    print(hex(fn_addr))
    assert check_jump(prefix, callsite, fn_addr)

def test_fn_ptr_x86_64():
    """
    fn_ptr.c compiled for x86-64 with no special flags
    """
    check_jump_to_sym("x86-64/fn_ptr", 0x117f, "add")
    check_jump_to_sym("x86-64/fn_ptr", 0x117f, "sub")

def test_fn_ptr_x86_64_offset_text():
    """
    fn_ptr.c compiled for x86-64 with -Ttext=0x8000 to ensure that the .text
    section is not in the first loadable segment. This means that virtual
    addresses in .text won't correspond to offsets into the file, so instead of
    using symbols like in `check_jump_to_sym` we use objdump -dF to get the file
    offsets for the callsite and the `add` and `sub` functions.
    """
    prefix = "x86-64/fn_ptr-offset-text"
    callsite = 0x213f
    add_fn = 0x20f9
    sub_fn = 0x210d
    assert check_jump(prefix, callsite, add_fn)
    assert check_jump(prefix, callsite, sub_fn)

def test_fn_ptr_x86_64_static():
    """
    fn_ptr.c compiled for x86-64 with -static
    """
    check_jump_to_sym("x86-64/fn_ptr-static", 0x4017db, "add")
    check_jump_to_sym("x86-64/fn_ptr-static", 0x4017db, "sub")

def test_fn_ptr_arm32():
    """
    fn_ptr.c compiled for arm32 with no special flags
    """
    check_jump_to_sym("arm32/fn_ptr", 0x5fc, "add")
    check_jump_to_sym("arm32/fn_ptr", 0x5fc, "sub")

def test_fn_ptr_arm32_static():
    """
    fn_ptr.c compiled for arm32 with -static
    """
    check_jump_to_sym("arm32/fn_ptr-static", 0x105fc, "add")
    check_jump_to_sym("arm32/fn_ptr-static", 0x105fc, "sub")

def test_arm_thumb_mixed():
    """
    arm_thumb_mixed.c compiled for arm32 with -march=armv7-a -mfloat-abi=hard
    """
    check_jump_to_sym("arm32/arm_thumb_mixed", 0x6a2, "arm_callee")
    check_jump_to_sym("arm32/arm_thumb_mixed", 0x6a2, "thumb_callee", True)
    check_jump_to_sym("arm32/arm_thumb_mixed", 0x73c, "arm_callee")
    check_jump_to_sym("arm32/arm_thumb_mixed", 0x73c, "thumb_callee", True)

def test_arm_thumb_mixed_static():
    """
    arm_thumb_mixed.c compiled for arm32 with -march=armv7-a -mfloat-abi=hard -static
    """
    check_jump_to_sym("arm32/arm_thumb_mixed-static", 0x105ea, "arm_callee")
    check_jump_to_sym("arm32/arm_thumb_mixed-static", 0x105ea, "thumb_callee", True)
    check_jump_to_sym("arm32/arm_thumb_mixed-static", 0x10684, "arm_callee")
    check_jump_to_sym("arm32/arm_thumb_mixed-static", 0x10684, "thumb_callee", True)

