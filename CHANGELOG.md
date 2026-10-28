# MBRGPTNT changelog

## v1.1.0.0

- The installer adds an **MBRGPTNT** program group: Disk / Partition
  Manager, MBRGPTNT Command Line (a command prompt showing `mgptpart`'s
  commands) and Uninstall MBRGPTNT. On NT 3.x it is a common group in
  Program Manager, on NT 4.0 and later a Start menu folder. Reinstalling
  replaces the group instead of adding a second one.
- New uninstaller, `mbrgunin.exe`: after a confirmation it removes the
  program group, the service and the files; files in use are deleted at
  the next restart. The data on the disks is not touched.
- Installing over a running version works: files in use (the loaded
  driver, a running manager) are replaced at the next restart. Before, the
  installer stopped on NT 3.x when the driver file was in use.
- Every tool has its own icon: the manager's disk, with a command prompt
  badge for `mgptpart`, a green arrow for the installer and a red cross
  for the uninstaller - in 16 VGA colors for NT 3.x / 4.0 and in 32-bit
  color for XP.
- `mgptpart` help fits in 80 columns.
- **File-system detection covers 64 kinds** (it knew 15): besides FAT,
  NTFS, exFAT, ReFS, HPFS, ext2/3/4 and Btrfs now
  - Linux: XFS, JFS, ReiserFS, Reiser4, F2FS, NILFS2, bcachefs, GFS2,
    OCFS2, SquashFS, cramfs, EROFS, romfs, JFFS2, Minix, the original ext,
    Xiafs, Linux swap, LUKS 1 / 2, LVM2, Linux RAID (md 0.90, 1.0, 1.1,
    1.2);
  - Apple: HFS, HFS+, HFSX, APFS, MFS, Core Storage, Apple boot;
  - BSD / Unix: UFS, UFS2, ZFS, HAMMER, HAMMER2, BSD swap, System V, Xenix,
    UnixWare BFS, SGI EFS, VxFS, QNX6;
  - optical and others: **UDF** (UDF 1.02 - 2.60, blocks of 512 - 4096
    bytes; taken as UDF only when both its volume recognition sequence and
    its anchor at block 256 are valid), ISO 9660, BeFS, Amiga FFS, VMFS,
    BitLocker, BIOS boot.
  Each has its own color in the manager. A GPT partition with no file
  system found is named by its type GUID where that says more (Linux swap,
  LVM, RAID, LUKS; Apple HFS+, APFS, boot, Core Storage; FreeBSD UFS, ZFS,
  swap; Solaris ZFS; BIOS boot), an MBR one by its type byte (82, 8E, FD).
- **Disks without a partition table** holding one file system (a
  superfloppy, an ISO or UDF image, a whole-disk ext4 / XFS / LUKS) are
  shown as such instead of as garbage partitions; nothing is written to
  them and no volume is published. Initialize gives them a table.
- **CD / DVD drives** are listed in the manager and by `mgptpart list` /
  `show cd<n>`, with the file system on the disc (ISO 9660, UDF on CD, DVD
  and BD, HFS, ...) and its drive letter; shown only.
- **ReFS** detection is stricter: the whole boot-sector header ("FSRS"
  structure, sector and cluster size) must be valid, not only the name. A
  partition reformatted from ReFS to UDF or from UDF to ReFS keeps parts of
  the old file system; the boot sector, which belongs to the newer one,
  now decides, so such a partition is no longer shown as the wrong one.
- Volumes answer the size and position requests of Windows 2000 / XP
  themselves (`IOCTL_DISK_GET_LENGTH_INFO`, `IOCTL_DISK_GET_PARTITION_INFO_EX`,
  `IOCTL_DISK_SET_PARTITION_INFO_EX`, `IOCTL_STORAGE_GET_DEVICE_NUMBER`).
  Before, these went to the disk, which answered for the whole disk: a file
  system that took the volume size from them (UDF looks for its last
  anchor at the end of the volume) looked past the volume, and every volume
  of a disk reported the same device number.
- **Disks larger than 2 TiB used whole on 32-bit Windows.**
  - NT 3.x - XP i386 read and write disks with 32-bit sector numbers
    (READ(10) / WRITE(10)) and stop at 2 TiB. MBRGPTNT now asks such a disk
    for its real size (READ CAPACITY(16)) and reads and writes the part
    past 2 TiB itself with READ(16) / WRITE(16), sent through the disk
    driver's SCSI pass-through to the controller driver (AHCINT, a SCSI
    card). The whole disk is then used: GPT initialize, partitions of any
    size anywhere on the disk, volumes past or across 2 TiB mounted with
    drive letters, formatted and written by Windows (FAT32 and NTFS
    tested). Requests below 2 TiB keep the disk driver's normal path.
  - A partitioned and formatted disk (made on another system) works the
    same way: its volumes past 2 TiB get drive letters and can be read and
    written.
  - Where the controller driver cannot carry 16-byte commands (IDE / ATA
    disk drivers), the disk is shown with its real size, the part out of
    reach is hatched in the disk map, partitions reaching past it get no
    drive letter, and the table is not rewritten (a GPT written there
    would describe only 2 TiB, and its backup copy would land inside the
    disk); only MBR is offered on a blank disk.
  - Dynamic disks are not extended this way: a dynamic volume's pieces
    past 2 TiB count as missing there, and such a disk is not converted
    between basic and dynamic.
  - Windows 2000 reports a disk past 2 TiB as 0 bytes when the controller
    driver answers FFFFFFFFh; MBRGPTNT takes the size from the device.
  - The manager showed sizes past 2^32 sectors wrongly (cut to 32 bits: a
    3 TB disk appeared as about 750 GB) and placed such partitions wrongly
    in the disk map; `mgptpart` printed them in hexadecimal. Sector numbers
    are 64-bit everywhere now, and sizes show in KB, MB, GB, TB or PB.
  - The disk size is read from the disk (`IOCTL_DISK_GET_LENGTH_INFO`,
    else partition 0's length, checked by reading the last sector, or the
    table's own end when it can be read) instead of being computed from
    the cylinder count, which leaves out the partial last cylinder. A GPT
    written by MBRGPTNT has its backup on the real last sector wherever the
    disk driver reaches it (NT 3.x's atdisk does not).
  - A partition table is never written when a partition reaches into the
    GPT backup area at the end of the reachable disk; before, the backup
    copy could overwrite the end of the last partition.
  - MBR partitions end at sector 2^32 - 1 at most, the largest value an
    MBR entry holds.
  - Every size is shown in the largest unit it fills at least once: 1024
    MB and more as GB, 1024 GB and more as TB, 1024 TB and more as PB
    ("1.4 GB", "2.7 TB", "1.5 PB"), in the manager, its dialogs and
    `mgptpart` alike.
  - Sizes are entered in KB, MB, GB, TB or PB: the New partition dialog
    has TB and PB in its unit list, and `mgptpart create` takes a unit
    after the number (`750GB`, `2TB`; MB when there is none).
  - The New partition dialog says how large the free area is when the
    size entered does not fit, instead of a general failure message.
- Version information (1.1.0.0) in the driver and every tool.

## v1.0.0.0

First release. MBR, GPT and dynamic disk (LDM) driver and tools for
Windows NT 3.1, 3.5, 3.51, 4.0, 2000 and XP, as i386 and x64 binaries built
from one source.

### Driver

- **GPT**, read and write: GPT disks behind the in-box disk class driver are
  enumerated and each partition is published as a volume with a drive
  letter. Primary GPT with automatic backup-header fall-back.
- **MBR**: on 512-byte MBR disks the partitions the in-box driver of NT 3.x /
  4.0 does not serve are mounted too - `0E` FAT16, `0B` / `0C` FAT32, the
  logical drives inside an `0F` extended partition, the partitions after the
  first one on a USB stick. The driver asks the class driver which ranges it
  serves, so nothing is mounted twice.
- **Dynamic disks (LDM)** on MBR and GPT disks and on groups mixing both:
  simple, spanned, striped, mirrored and RAID-5 volumes are mounted with the
  drive letters the database asks for. A mirror or RAID-5 volume with one
  disk missing stays readable (degraded, read-only; RAID-5 data is rebuilt
  from parity). Requests inside one extent go straight to the member disk;
  the rest is split, assembled and computed by the driver.
  - Disk groups are managed: basic -> dynamic (MBR and GPT; partitions
    become simple volumes in place, keeping data and letters), dynamic ->
    basic, new volumes of every layout across the group's disks (RAID-5
    cleared first for correct parity), delete volume, change letter (kept
    in the database). Every change is written to every disk of the group.
  - Windows 2000 and later own MBR dynamic disks through their own LDM
    driver; there MBRGPTNT shows those groups and the letters Windows gave
    their volumes, and mounts and manages only groups whose disks are all
    GPT (which 2000 / XP i386 cannot read). On x64 Windows every group is
    left to Windows.
  - Groups made by MBRGPTNT are imported by Windows (tested with Windows XP
    x64 and Windows 2000); groups made by Windows 2000 and later are read,
    also when part of their database lies past the last whole cylinder that
    the disk driver of NT 3.x / 4.0 reaches.
- **x64**: a native driver for Windows XP x64 / Server 2003 x64. Windows
  reads GPT and dynamic disks there itself, so the x64 driver publishes no
  volumes; it reads, shows and edits partition tables, and after every
  table write Windows 2000 and later are told to read the disk again.
- **Table editing** through a control device (`\\.\MbrGptNt`) for GPT and
  MBR: create (auto-placed or at an offset; primaries, an extended
  container and logical drives inside it), delete, change type, set the
  active flag, reassign the drive letter, initialize an empty GPT or MBR
  table. The GPT backup and the MBR EBR chain are written correctly.
  Changes are applied live, even while other volumes on the same disk are
  in use.
- **The right MBR type byte** is chosen from a partition's size and
  position: FAT16 `04` / `06` / `0E`, FAT32 `0B` / `0C`, extended `05` /
  `0F`. MBR entries carry real CHS values inside the BIOS range.
- **Lossless GPT <-> MBR conversion**: partitions keep their start and
  length; when that is impossible (no room for the GPT headers, past 2 TiB,
  no gap for an EBR, a dynamic disk, the system disk) the reason is given.
- **File-system detection** from the volume itself: FAT12, FAT16, FAT32 (by
  cluster count), NTFS, HPFS, exFAT, ReFS, ext2, ext3, ext4, Btrfs, else
  RAW; also through the mapping of dynamic volumes.
- **Disks that come and go** (USB, card readers): checked every 2 seconds;
  new disks get their volumes and letters, removed or swapped ones lose
  them; volumes still in use are dismounted and removed once closed.
- **Protection** of the running system partition and disk: deleting,
  wiping and converting them are refused.
- FORMAT works on every volume the driver publishes (partition type,
  surface verify), dynamic volumes included.
- All sector sizes for basic disks: 512, 1024, 2048, 4096.

### Tools

- **mgptgui**, a graphical Disk / Partition Manager: icon toolbar (New,
  Delete, Set Active, Change Type, Drive Letter, To GPT / To MBR, To
  Dynamic / To Basic, Initialize, Refresh), partition table with scroll bar
  and keyboard navigation, disk map, context menu, a dialog for every
  operation and a result box after it, automatic refresh when disks or
  tables change. Dynamic disks are shown with their disk group, every
  volume piece with its volume name, position, layout and state; pieces of
  the selected volume are highlighted on all disks. Its own icon (16-color
  for NT 3.x / 4.0, 32-bit for XP). Plain USER32 / GDI, so it runs on NT 3.1.
- **mgptpart**, the command-line tool: `list`, `show`, `init`, `create`,
  `delete`, `settype`, `active`, `letter`, `convert`, `rescan`, and for
  dynamic disks `volumes`, `dynamic`, `basic`, `vcreate`, `vdelete`,
  `vletter`.
- **mbrginst**, the installer: copies the driver and the tools, registers
  the service.

### Notes

- The client editions of Windows show mirrored and RAID-5 volumes of an
  imported group as Failed; the Server editions use them.
- MBRGPTNT does not rebuild a mirror or RAID-5 volume on a new disk.
- Booting NT from a GPT or dynamic disk needs a loader that understands it
  and is out of scope for the driver.

## AHCINT 4Kn patch (ahcint-4kn/ahcint-4kn.patch)

- Teaches AHCINT to report a disk's real logical sector size (IDENTIFY
  word 106 / 117-118) in READ CAPACITY, MODE SENSE and the read/write
  length instead of a hard-coded 512, so 4Kn SATA disks present correctly.
  Needs verification on real 4Kn hardware (emulators expose 512-byte
  logical sectors).
