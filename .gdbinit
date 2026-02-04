# GDB may have ./.gdbinit loading disabled by default.  In that case you can
# follow the instructions it prints.  They boil down to adding the following to
# your home directory's ~/.gdbinit file:
#
#   add-auto-load-safe-path /path/to/qemu/.gdbinit

# Load QEMU-specific sub-commands and settings
# source scripts/qemu-gdb.py

file qemu-system-riscv64

b hmtt_forward
run -M virt -m 16G \
    -cpu rv64 \
    -nographic \
    -hmtt /data/memory-bound-trace/libquantum-split/piece000536.trc \
    -hmtt-elf /home/wanghan/Workspace/DASICS_ICT/rootfs/collect/libquantum/libquantum \
    -device loader,file=${LINUX_DIR}/arch/riscv/boot/Image,addr=0x80400000 \
    -bios ${OpenSBI_DIR}/build/platform/generic/firmware/fw_jump.bin
