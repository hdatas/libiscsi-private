
1. libiscsi support for sg3-utils

   A. HCD: git clone --depth=1 git@github.com:hdatas/libiscsi-private.git

   B. Build and install libiscsi. the default library path is /usr/local/lib. you need add this path to LD_LIBRARY_PATH when build sg3_utils package.
      Build with debugging info
      1t ./autogen.sh
	    2) ./configure --prefix=/usr/local CFLAGS=-g CXXFLAGS=-g
      3) make
      4) sudo make install

   C. git clone sg3_utils at: git clone --branch v1.32 --depth=1 git@github.com:hreinecke/sg3_utils.git
   (there is another repo by ubuntu, git clone https://git.launchpad.net/ubuntu/+source/sg3-utils, but has no v1.32 tag)
   The current patch provided by libiscsi was written for sg3-utils v1.32.

      HCD: git clone --depth=1 git@github.com:hdatas/sg3_utils_private.git
           git fetch origin vvol:vvol
           git checkout vvol
           Use git status to verify you are at vvol branch (origin v1.32 + libiscsi patch + sg_luns.c"

   D. Build sg3-utils with libiscsi support
      Apply the patch first. See item 3 below on how to apply git patch. Ignore the trailing whitespace errors.
      1) export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
      2) ./autogen.sh
	    3) ./configure --prefix=/usr/local CFLAGS=-g CXXFLAGS=-g &> ~/tmp/sg3-configure.log
         make sure you see output "checking if libiscsi is available... yes"
      4) ./make
      5) Do we need a "sudo make install"?


2. git apply patch
   hcd@probe14:~/projects/sg3_utils$ git apply -v --reject --whitespace=fix  ../libiscsi/patches/sg3_utils-1.32.patch
../libiscsi/patches/sg3_utils-1.32.patch:38: trailing whitespace.
this library is available and if it is build a version of sg3_utils with
../libiscsi/patches/sg3_utils-1.32.patch:311: trailing whitespace.
        memcpy(ptp->datain.data, task->datain.data,
Checking patch README...
Checking patch configure.ac...
Checking patch include/sg_pt_iscsi.h...
Checking patch lib/Makefile.am...
Checking patch lib/sg_pt_iscsi.c...
Checking patch lib/sg_pt_linux.c...
Applied patch README cleanly.
Applied patch configure.ac cleanly.
Applied patch include/sg_pt_iscsi.h cleanly.
Applied patch lib/Makefile.am cleanly.
Applied patch lib/sg_pt_iscsi.c cleanly.
Applied patch lib/sg_pt_linux.c cleanly.
warning: 2 lines applied after fixing whitespace errors.
hcd@probe14:~/projects/sg3_utils$

4. Debug

   $ cd /home/hcd/projects/libiscsi/utils
   $ gdb .libs/iscsi-inq
   $ gdb r -i iqn.2010-12.org.sg3utils iscsi://172.16.1.56:3260/iqn.2016-01.com.hcd:pool-1-0/255

   And with sg3_xxx, go to build or install dir (--prefix=/home/hcd/local/bin)
   ./sg_inq iscsi://172.16.1.56:3260/iqn.2016-01.com.hcd:pool-1-0/255

   ./sg_dd if=/tmp/dummy.txt of=iscsi://172.16.1.56:3260/iqn.2016-01.com.hcd:pool-1-0/255:123 count=1

   On tgtd side:
   a. spc.c:spc_inquiry(): evpd set iSCSI standard inquiry data.
   b. sbc_lu_init(sbc.c) -> spc_lu_init (spc.c) to initialize the lun (struct scsi_lu).

   On libiscsi side:
   a. scsi-lowlevel.c:scsi_inquiry_unmarshall_standard(): un-marshalling the standard inquiry data. The data structure is struct scsi_inquiry_standard. Added LU_CONG bit after RMB bit. It is in the byte 1 (second byte), bit 6(The 7th bit, mask 0x40). The bit 7 (the 8th bit is for RMB, mask 0x80).

   ./iscsi-dd -s iscsi://172.16.1.56:3260/iqn.2016-01.com.hcd:pool-1-0/255 -d iscsi://172.16.1.56:3260/iqn.2016-01.com.hcd:pool-1-0/255 -b 1
   ./iscsi-test-cu iscsi://172.16.1.56/iqn.2016-01.com.hcd:pool-1-0/255:123 -t SCSI.Write10.Simple -V -d

   On ESXi side:
   After restart HCD, you need to rescan storage adapter:
   a. esxcli storage core adapter rescan --all
   Then list the device again:
   b. esxcli storage core device list.
   c. ESXi claims can recongnize 10 bit LUN ID in traditional LUN addressing to increase the 8-bit limit of 255 lun ids. 3ffh or 0 to 1023 (1024 total, 2^10)


