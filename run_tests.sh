echo "Running dynamically linked x86-64 test"
../qemu/build/qemu-x86_64 -plugin ./libibresolver.so,output="tests/x86-64/fn_ptr.csv" tests/x86-64/fn_ptr.elf

echo "Running dynamically linked x86-64 test with an offset .text section"
../qemu/build/qemu-x86_64 -plugin ./libibresolver.so,output="tests/x86-64/fn_ptr-offset-text.csv" tests/x86-64/fn_ptr-offset-text.elf

echo "Running statically linked x86-64 test"
../qemu/build/qemu-x86_64 -plugin ./libibresolver.so,output="tests/x86-64/fn_ptr-static.csv" tests/x86-64/fn_ptr-static.elf

echo "Running dynamically linked arm32 test"
../qemu/build/qemu-arm -L /usr/arm-linux-gnueabihf/ -plugin ./libibresolver.so,output="tests/arm32/fn_ptr.csv" tests/arm32/fn_ptr.elf

echo "Running statically linked arm32 test"
../qemu/build/qemu-arm -plugin ./libibresolver.so,output="tests/arm32/fn_ptr-static.csv" tests/arm32/fn_ptr-static.elf

echo "Running dynamically linked arm32/arm_thumb_mixed test"
../qemu/build/qemu-arm -L /usr/arm-linux-gnueabihf/ -plugin ./libibresolver.so,output="tests/arm32/arm_thumb_mixed.csv" tests/arm32/arm_thumb_mixed.elf

echo "Running statically linked arm32/arm_thumb_mixed test"
../qemu/build/qemu-arm -plugin ./libibresolver.so,output="tests/arm32/arm_thumb_mixed-static.csv" tests/arm32/arm_thumb_mixed-static.elf
