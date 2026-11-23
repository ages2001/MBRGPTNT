# MBRGPTNT changelog

## v1.2.0.0

Disks plugged in while Windows runs (GitHub issue #1: an 8 TB e-SATA disk
with one GPT NTFS partition on Windows XP 32-bit was found only when it was
connected before start-up).

- **Drive letters are now kept across restarts** (GitHub issue #3: on
  Windows 2000 a letter the user picked was lost on the next boot). Because
  the volumes this driver publishes are not PnP volumes, the Mount Manager
  does not persist their letters, so each boot handed out the first free
  letter again. MBRGPTNT now remembers the letter itself, keyed by a stable
  volume id - the GPT partition GUID, or the MBR disk signature plus the
  partition start LBA - under its own service key
  `HKLM\System\CurrentControlSet\Services\<driver>\Letters` (one value per
  volume). On start-up the stored letter is reused when it is free, a newly
  assigned letter is written back, and a letter the user changes at run time
  is captured on the next disk-check pass and persisted too. Applies to
  basic MBR/GPT volumes and to dynamic (LDM) volumes.
- **Pure-GPT paging file now works, and the Mount Manager owns the letters of
  fixed volumes.** On NT5+ (2000/XP) a legacy driver's volumes were invisible
  to the Mount Manager, so the Memory Manager refused them a paging file -
  pure GPT then showed XP's "Limited Virtual Memory" warning. Each **fixed**
  volume is now made Mount-Manager-aware the non-PnP way: it answers the
  MOUNTDEV query set (`IOCTL_MOUNTDEV_QUERY_UNIQUE_ID` / `_DEVICE_NAME` /
  `_STABLE_GUID` / `_SUGGESTED_LINK_NAME`) and posts a volume-arrival
  notification to `\Device\MountPointManager`. The Mount Manager then
  enumerates the volume, assigns and persists its drive letter natively (so
  issue #3 is handled by the OS on these systems, with the driver's own
  `\Letters` store as the suggested letter and the fallback where there is no
  Mount Manager - NT 3.x/4.0), and the paging file can be created on a pure-GPT
  volume. **Removable / hot-plug** volumes are not handed over: they keep the
  driver's own letter management so on-the-fly removal (issue #2) stays
  reliable, and a removable disk is never a paging target anyway.
- **Boot-start driver on every OS.** The installer now registers MBRGPTNT
  with `Start = 0` (`SERVICE_BOOT_START`, group *Boot Bus Extender*) on all
  targets, so the driver loads at boot and its MBR/GPT volumes - including a
  pure-GPT system or paging volume - exist before the session manager
  activates the paging file. `ErrorControl` stays *Normal*, so a load
  failure is logged and boot continues; `DriverEntry` does no disk work
  (enumeration is deferred to a worker thread), so early load order is safe.
- **Full PnP on Windows 2000 and later (new 2000/XP 32-bit build).** In
  addition to the non-PnP Mount-Manager path above, the NT5+ builds are a
  real PnP **bus driver**. The installer registers a root-enumerated device
  (`Root\mbrgptnt`, or install `mbrgptnt.inf`), the PnP manager calls the
  driver's `AddDevice`, and each live volume is reported as a child **PDO**
  that answers `QUERY_ID` / `QUERY_CAPABILITIES` (`RawDeviceOK`,
  `SurpriseRemovalOK`) / `QUERY_DEVICE_TEXT` and exposes
  `GUID_DEVINTERFACE_VOLUME`. That device interface is exactly what the
  Mount Manager watches, so on these systems the volumes behave as native
  PnP volumes - Device Manager presence, persistent `\??\Volume{GUID}`
  names, drive letters and pure-GPT paging - with clean arrival/removal via
  `IRP_MN_QUERY_DEVICE_RELATIONS`/`REMOVE_DEVICE` instead of polling. The
  driver is split by OS: `nt31`/`nt35`/`nt40` are the unchanged legacy boot
  drivers (no PnP imports, so they load on systems with no PnP manager),
  while `nt50` (2000/XP/2003 x86) and `x64` (XP x64/2003 x64) add the PnP
  bus. NT 3.x/4.0 keep the driver's own letter management and the legacy
  Mount-Manager notification.
- **A powered-off external disk is noticed and re-detected** (GitHub issue
  #2: an external e-SATA disk was connected on the fly only the first time;
  after switching it off and on again it was no longer found). A fixed
  external disk keeps its device object when switched off and answers its
  geometry from cache, so a geometry-only check never saw it go away and so
  never re-detected it. The disk-check pass now reads sector 0 of every disk
  (not only removable media), so a disk that is off is seen as gone and is
  published again when it returns; the signature read there also catches a
  disk swapped for another.
- **A disk that is not ready when it shows up is read again.** A disk
  plugged in while Windows runs can still be spinning up when the disk
  driver creates its device, and a disk past 2 TiB may not answer READ
  CAPACITY(16) or the read of its last sector yet. Before, the driver read
  such a disk once on arrival: a table it could not read, or a disk past
  2 TiB whose end did not answer, stayed without volumes (or with the part
  past 2 TiB out of reach) until the next restart. Now the disk is read
  again every 2 seconds for up to a minute, until its table and, past
  2 TiB, its real size and end can be read; its volumes and drive letters
  then appear. The same applies to disks that are slow at start-up.
- **Refresh looks for new hardware** (Windows 2000 / XP). Many controller
  drivers do not report a disk plugged in while Windows runs (e-SATA on
  controllers in IDE mode, SCSI cards, controller drivers without hot-plug
  support); Windows then sees the disk only after a restart or Device
  Manager's "Scan for hardware changes". Refresh (F5) in the manager and
  `mgptpart rescan` now start that scan themselves, wait for it, and then
  have MBRGPTNT read the new disk, so the disk appears with its volumes and
  drive letters. On NT 3.x / 4.0, whose disk drivers find disks only at
  start-up, Refresh works as before.
- **Explorer shows new and removed drive letters at once.** MBRGPTNT
  creates and removes its drive letters in the kernel, where nothing tells
  the shell; an open My Computer window kept showing a removed disk's
  letter and did not show a new one until it was refreshed. A new
  program, `mgptntfy.exe`, runs hidden in every logon session on NT 4.0
  and later (started from the Run key; `mbrginst` registers and starts
  it, `mbrgunin` ends and removes it), watches the driver's change counter
  and sends the volume arrival / removal messages for the letters that
  came or went. The manager and `mgptpart` send them too after every
  change they make.
- **Disks past 2 TiB that Windows 2000 reports as 0 bytes are used
  whole.** Behind a controller driver that answers READ CAPACITY(10) with
  FFFFFFFFh (SCSI cards, USB), Windows 2000's disk driver reports no size
  at all. MBRGPTNT took the device's real size from READ CAPACITY(16) but
  showed the disk read-only with no volumes; now every sector past the
  first goes through its own READ(16) / WRITE(16), as on a disk the driver
  reaches up to 2 TiB, and its volumes get drive letters. The disk row says
  "no size from the disk driver: all through MBRGPTNT's own 16-byte
  commands".
- **NT 3.1: IDE disks larger than about 500 MB had a wrong size.** NT
  3.1's IDE disk driver (atdisk) reports a disk whose BIOS geometry has
  255 heads with 65535 heads, so a 4 GB disk showed as 1.0 TB and an 8 GB
  one as 1.9 TB. On such a GPT disk a new table would have placed its
  backup copy at a sector that does not exist (atdisk wraps it onto
  another one). The geometry is taken with at most 255 heads now. A
  request the disk driver does not know (the disk-length request on NT
  3.x) is no longer taken as answered when the driver only reports
  success.
- Version information (1.2.0.0) in the driver and every tool.

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
