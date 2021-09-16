import csv
from elftools.elf.elffile import ELFFile

def check_jump(filename, origin, dst):
    """
    Checks if an indirect jump from `origin` to `dst` was recorded in the output
    file `filename + ".csv"`
    """
    #statically_compiled = False
    #entry = 0
    #with open(filename + ".elf", 'rb') as elffile:
    #    elf = ELFFile(elffile)
    #    if elf.header['e_type'] == "ET_EXEC":
    #        statically_compiled = True
    #        entry = elf.header['e_entry']
    with open(filename + ".csv", newline='') as csvfile:
        output = csv.reader(csvfile)
        first_line = True
        #offset_origin = origin
        #if statically_compiled:
        #    offset_origin = origin - 0x400000
        #offset_dst = dst
        #if statically_compiled:
        #    offset_dst = dst - 0x400000
        for row in output:
            if first_line:
                first_line = False
                continue
            if origin == int(row[0], 16):
                if dst == int(row[2], 16):
                    return True
        return False

# The `origin`s for `test_fn_ptr_*` correspond to the addresses of the indirect call to `call_with_args`
# The `dst`s correspond to the addresses of the `add` and `sub` functions

def test_fn_ptr_x86_64():
    assert check_jump("x86-64/fn_ptr", 0x117f, 0x1139)
    assert check_jump("x86-64/fn_ptr", 0x117f, 0x114d)

def test_fn_ptr_x86_64_static():
    assert check_jump("x86-64/fn_ptr-static", 0x4017db, 0x401795)
    assert check_jump("x86-64/fn_ptr-static", 0x4017db, 0x4017a9)

def test_fn_ptr_arm32():
    assert check_jump("arm32/fn_ptr", 0x5fc, 0x578)
    assert check_jump("arm32/fn_ptr", 0x5fc, 0x5a8)

def test_fn_ptr_arm32_static():
    assert check_jump("arm32/fn_ptr-static", 0x105fc, 0x10578)
    assert check_jump("arm32/fn_ptr-static", 0x105fc, 0x105a8)
