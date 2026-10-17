# MBRGPTNT changelog

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
