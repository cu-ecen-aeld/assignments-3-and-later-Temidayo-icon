#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u
set -x

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "Building kernel..."
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
	make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
	cp arch/${ARCH}/boot/Image ${OUTDIR}/
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories

echo "Creating root filesystem structure..."
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    echo "Configuring BusyBox..."
make distclean
make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
echo "Building and installing BusyBox..."
cd ${OUTDIR}/busybox
#make CROSS_COMPILE=${CROSS_COMPILE} -j4
#make CONFIG_PREFIX=${OUTDIR}/rootfs CROSS_COMPILE=${CROSS_COMPILE} install
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j4
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install CONFIG_PREFIX=${OUTDIR}/rootfs


echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
echo "Copying library dependencies..."
cp -a $SYSROOT/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp -a $SYSROOT/lib64/libm.so.* ${OUTDIR}/rootfs/lib64/
cp -a $SYSROOT/lib64/libresolv.so.* ${OUTDIR}/rootfs/lib64/
cp -a $SYSROOT/lib64/libc.so.* ${OUTDIR}/rootfs/lib64/
cp -a $SYSROOT/lib64/libpthread.so.* ${OUTDIR}/rootfs/lib64/

# TODO: Make device nodes
echo "Creating device nodes..."
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 622 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
echo "Building writer utility..."

cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
file writer
cp writer ${OUTDIR}/rootfs/home/

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "Copying finder scripts..."
cp autorun-qemu.sh finder-test.sh finder.sh ${OUTDIR}/rootfs/home/

# Create conf directory and copy configuration files
mkdir -p ${OUTDIR}/rootfs/home/conf
cp conf/username.txt conf/assignment.txt ${OUTDIR}/rootfs/home/conf/

# TODO: Chown the root directory
echo "Changing ownership..."
sudo chown -R root:root ${OUTDIR}/rootfs

echo "Creating init script..."
sudo tee ${OUTDIR}/rootfs/init > /dev/null << 'EOF'
#!/bin/sh
echo "Initramfs is up!"
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp

# Run finder app (or shell for debugging)
exec /bin/sh
EOF

sudo chmod +x ${OUTDIR}/rootfs/init


# TODO: Create initramfs.cpio.gz
echo "Creating initramfs..."
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root | gzip > ${OUTDIR}/initramfs.cpio.gz
