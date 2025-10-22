# GDB may have ./.gdbinit loading disabled by default.  In that case you can
# follow the instructions it prints.  They boil down to adding the following to
# your home directory's ~/.gdbinit file:
#
#   add-auto-load-safe-path /path/to/qemu/.gdbinit

# Load QEMU-specific sub-commands and settings
# source scripts/qemu-gdb.py

file qemu-system-riscv64

b main 
b update_cache_status

run -M virt -m 16G \
    -hmtt /data/mcf_riscv_simpoint_1/mcf_counter.trace \
    -hmtt-elf /home/wanghan/Workspace/HMTT/qemu-trace/mcf-dir/mcf \
    -cpu rv64 \
    -nographic \
    -device loader,file=${LINUX_DIR}/arch/riscv/boot/Image,addr=0x80400000 \
    -bios ${OpenSBI_DIR}/build/platform/generic/firmware/fw_jump.bin
