aarch64-linux-gnu-objcopy -O binary build/helloworld_kvm-arm64 ../../../avisor/image/output.bin

# Get entry point
entry_point=$(aarch64-linux-gnu-readelf -h build/helloworld_kvm-arm64 | grep Entry | awk '{print $4}')

# Generate the header file
echo "#ifndef __ENTRY_POINT_H__" > entry_point.h
echo "#define __ENTRY_POINT_H__" >> entry_point.h
echo "" >> entry_point.h
echo "#define ENTRY_POINT $entry_point" >> entry_point.h
echo "" >> entry_point.h
echo "#endif" >> entry_point.h

# Copy the header file to the avisor/image directory
mv entry_point.h ../../../avisor/src/entry.h

