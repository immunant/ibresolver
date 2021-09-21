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

def check_fn_ptr_test(prefix, callsite):
    """
    Check for indirect calls from the instruction at `callsite` (found using objdump)
    to functions named `add` and `sub`.
    """
    add_fn = sym_addr(prefix, "add")
    sub_fn = sym_addr(prefix, "sub")
    assert check_jump(prefix, callsite, add_fn)
    assert check_jump(prefix, callsite, sub_fn)

def test_fn_ptr_x86_64():
    """
    fn_ptr.c compiled for x86-64 with no special flags
    """
    check_fn_ptr_test("x86-64/fn_ptr", 0x117f)

def test_fn_ptr_x86_64_offset_text():
    """
    fn_ptr.c compiled for x86-64 with -Ttext=0x8000 to ensure that the .text
    section is not in the first loadable segment. This means that virtual
    addresses in .text won't correspond to offsets into the file, so instead of
    using symbols like in `check_fn_ptr_test` we use objdump -dF to get the file
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
    check_fn_ptr_test("x86-64/fn_ptr-static", 0x4017db)

def test_fn_ptr_arm32():
    """
    fn_ptr.c compiled for arm32 with no special flags
    """
    check_fn_ptr_test("arm32/fn_ptr", 0x5fc)

def test_fn_ptr_arm32_static():
    """
    fn_ptr.c compiled for arm32 with -static
    """
    check_fn_ptr_test("arm32/fn_ptr-static", 0x105fc)
