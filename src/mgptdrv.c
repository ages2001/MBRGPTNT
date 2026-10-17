/* mgptdrv.c - MBRGPTNT partition driver for Windows NT 3.1 - 2000/XP. */

#include "ddknt.h"
#include "gptcore.h"
#include "mgptioc.h"
#include "ldmcore.h"

#define MGPT_MAXDISK 16

void *memcpy(void *d, const void *s, SIZE_T n);
void *memset(void *d, int c, SIZE_T n);

void *GptPlatAlloc(gpt_u32 n) { return ExAllocatePool(NonPagedPool, (ULONG)n); }
void  GptPlatFree(void *p)    { ExFreePool(p); }

#define KIND_PART 0x50415254UL
#define KIND_CTRL 0x4354524CUL

typedef struct _PART_EXT {
    ULONG             Kind;
    struct _PART_EXT *Next;
    PDEVICE_OBJECT    Self;
    PDEVICE_OBJECT    LowerDevice;
    PFILE_OBJECT      LowerFile;
    LARGE_INTEGER     StartingOffset;
    LARGE_INTEGER     PartitionLength;
    ULONG             StartLbaLow;
    ULONG             BytesPerSector;
    ULONG             DiskNumber;
    ULONG             PartitionNumber;
    UCHAR             PartitionType;
    UCHAR             BootIndicator;
    UCHAR             Recognized;
    UCHAR             DriveLetter;
    UCHAR             Stale;          /* partition gone, volume still mounted */
    UCHAR             Gone;           /* stale and dismounted: letter removed,
                                         new opens and I/O refused until the
                                         device can be deleted */
    struct _LDMRT    *Ldm;            /* a dynamic (LDM) volume: its layout */
    char              DevName[40];    /* \Device\... name */
} PART_EXT, *PPART_EXT;

typedef struct _CTRL_EXT { ULONG Kind; } CTRL_EXT, *PCTRL_EXT;

typedef struct _ENUMCTX { PDEVICE_OBJECT Part0; ULONG BytesPerSector; gpt_u64 Limit; } ENUMCTX;

static PDRIVER_OBJECT g_Driver;
static PDEVICE_OBJECT g_Control;
static PPART_EXT      g_PartList;
static KEVENT         g_Lock;
static KEVENT         g_SyncLock;     /* one SyncDisk at a time */
static KEVENT         g_ScanLock;     /* one disk check / table write at a time */
static KEVENT         g_ScanReq;      /* asks the disk-check thread for a pass now */
static volatile ULONG g_Gen;          /* change counter reported to the tools */
static volatile ULONG g_PassStarted, g_PassDone;
static int            g_PollRunning;
static int            g_LdmDirty;

/* what was last seen of each disk, to notice arrival, removal and swaps */
typedef struct { int present; ULONG bps, removable, ident; gpt_u64 total; } DISKID;
static DISKID g_known[16];

#if defined(GPTNT_DBG)
#ifndef GPTNT_COMBASE
#define GPTNT_COMBASE 0x3F8
#endif
static void DbgCh(char c){int spin=0;while(spin++<100000){unsigned char lsr;__asm__ __volatile__("inb %1,%0":"=a"(lsr):"Nd"((unsigned short)(GPTNT_COMBASE+5)));if(lsr&0x20)break;}__asm__ __volatile__("outb %0,%1"::"a"((unsigned char)c),"Nd"((unsigned short)GPTNT_COMBASE));}
static void DbgStr(const char *s){while(*s)DbgCh(*s++);}
static void DbgHex(ULONG v){const char*h="0123456789ABCDEF";int i;for(i=28;i>=0;i-=4)DbgCh(h[(v>>i)&0xF]);}
#define D(s)  DbgStr(s)
#define DH(v) DbgHex((ULONG)(v))
#else
#define D(s)
#define DH(v)
#endif

static NTSTATUS SyncTransfer(ULONG Major, PDEVICE_OBJECT Device, PVOID Buf, ULONG Len, PLARGE_INTEGER Offset)
{
    KEVENT ev; IO_STATUS_BLOCK iosb; PIRP irp; NTSTATUS st;
    KeInitializeEvent(&ev, NotificationEvent, FALSE);
    irp = IoBuildSynchronousFsdRequest(Major, Device, Buf, Len, Offset, &ev, &iosb);
    if (!irp) return STATUS_INSUFFICIENT_RESOURCES;
    /* raw access to removable media must not wait for a file system verify */
    IoGetNextIrpStackLocation(irp)->Flags |= SL_OVERRIDE_VERIFY_VOLUME;
    st = IoCallDriver(Device, irp);
    if (st == STATUS_PENDING) { KeWaitForSingleObject(&ev, Executive, KernelMode, FALSE, NULL); st = iosb.Status; }
    return st;
}

static NTSTATUS SyncIoctl(ULONG Code, PDEVICE_OBJECT Device, PVOID In, ULONG InLen, PVOID Out, ULONG OutLen)
{
    KEVENT ev; IO_STATUS_BLOCK iosb; PIRP irp; NTSTATUS st;
    KeInitializeEvent(&ev, NotificationEvent, FALSE);
    irp = IoBuildDeviceIoControlRequest(Code, Device, In, InLen, Out, OutLen, FALSE, &ev, &iosb);
    if (!irp) return STATUS_INSUFFICIENT_RESOURCES;
    IoGetNextIrpStackLocation(irp)->Flags |= SL_OVERRIDE_VERIFY_VOLUME;
    st = IoCallDriver(Device, irp);
    if (st == STATUS_PENDING) { KeWaitForSingleObject(&ev, Executive, KernelMode, FALSE, NULL); st = iosb.Status; }
    return st;
}

static int EnumRead(void *Ctx, gpt_u64 Lba, gpt_u32 Count, void *Buf)
{
    ENUMCTX *e = (ENUMCTX *)Ctx; LARGE_INTEGER off; NTSTATUS st;
    off.QuadPart = (gpt_u64)Lba * (gpt_u64)e->BytesPerSector;
    st = SyncTransfer(IRP_MJ_READ, e->Part0, Buf, Count * e->BytesPerSector, &off);
    if (!NT_SUCCESS(st)) { D("read fail lba="); DH((ULONG)Lba); D(" n="); DH(Count); D(" st="); DH(st); D(" buf="); DH((ULONG_PTR)Buf); D("\n"); }
    return NT_SUCCESS(st) ? 0 : 1;
}
static int EnumWrite(void *Ctx, gpt_u64 Lba, gpt_u32 Count, const void *Buf)
{
    ENUMCTX *e = (ENUMCTX *)Ctx; LARGE_INTEGER off;
    /* LDM database copies past the end the disk driver reports (the last
       partial cylinder on NT 3.x / 4.0) cannot be reached: the copies
       inside it are written */
    if (e->Limit) { if (Lba >= e->Limit) return 0; if (Lba + Count > e->Limit) Count = (gpt_u32)(e->Limit - Lba); }
    off.QuadPart = (gpt_u64)Lba * (gpt_u64)e->BytesPerSector;
    return NT_SUCCESS(SyncTransfer(IRP_MJ_WRITE, e->Part0, (PVOID)Buf, Count * e->BytesPerSector, &off)) ? 0 : 1;
}

static NTSTATUS MakeUnicode(const char *Ansi, PUNICODE_STRING Out)
{ ANSI_STRING a; RtlInitAnsiString(&a, Ansi); return RtlAnsiStringToUnicodeString(Out, &a, TRUE); }

static NTSTATUS OpenPartition0(ULONG DiskNumber, PFILE_OBJECT *File, PDEVICE_OBJECT *Dev)
{
    char name[48]; UNICODE_STRING uname; NTSTATUS st;
    sprintf(name, "\\Device\\Harddisk%lu\\Partition0", (unsigned long)DiskNumber);
    if (!NT_SUCCESS(MakeUnicode(name, &uname))) return STATUS_INSUFFICIENT_RESOURCES;
    st = IoGetDeviceObjectPointer(&uname, 0, File, Dev);
    RtlFreeUnicodeString(&uname);
    return st;
}

static void LockList(void)   { KeWaitForSingleObject(&g_Lock, Executive, KernelMode, FALSE, NULL); }
static void UnlockList(void) { KeSetEvent(&g_Lock, 0, FALSE); }

static void CopyStackDown(PIO_STACK_LOCATION dst, PIO_STACK_LOCATION src, PDEVICE_OBJECT Lower)
{
    dst->MajorFunction = src->MajorFunction; dst->MinorFunction = src->MinorFunction;
    dst->Flags = src->Flags; dst->Control = 0; dst->Parameters = src->Parameters;
    dst->DeviceObject = Lower; dst->FileObject = src->FileObject;
}

static NTSTATUS Complete(PIRP Irp, NTSTATUS Status, ULONG Info)
{ Irp->IoStatus.Status = Status; Irp->IoStatus.Information = Info; IoCompleteRequest(Irp, IO_NO_INCREMENT); return Status; }

#define STATUS_NO_SUCH_DEVICE ((NTSTATUS)0xC000000EL)
static NTSTATUS NTAPI DispCreateClose(PDEVICE_OBJECT Dev, PIRP Irp)
{
    PPART_EXT ext = (PPART_EXT)Dev->DeviceExtension;
    /* a Gone volume refuses new opens, except this driver's own (kernel
       mode) open that dismounts the file system still mounted on it */
    if (ext->Kind == KIND_PART && ext->Gone && Irp->RequestorMode != KernelMode &&
        IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CREATE)
        return Complete(Irp, STATUS_NO_SUCH_DEVICE, 0);
    return Complete(Irp, STATUS_SUCCESS, 0);
}

static NTSTATUS LdmReadWrite(PPART_EXT ext, PIRP Irp);
static NTSTATUS LdmDeviceControl(PPART_EXT ext, PIRP Irp, ULONG code, PVOID sys, ULONG inlen, ULONG outlen);

static NTSTATUS NTAPI DispReadWrite(PDEVICE_OBJECT Dev, PIRP Irp)
{
    PPART_EXT ext = (PPART_EXT)Dev->DeviceExtension;
    PIO_STACK_LOCATION cur = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nxt = IoGetNextIrpStackLocation(Irp);
    LARGE_INTEGER off = cur->Parameters.ReadWrite.ByteOffset;
    ULONG len = cur->Parameters.ReadWrite.Length;
    LARGE_INTEGER end;
    if (ext->Kind != KIND_PART) return Complete(Irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    if (ext->Gone) return Complete(Irp, STATUS_NO_SUCH_DEVICE, 0);
    if (ext->Ldm) return LdmReadWrite(ext, Irp);
    end.QuadPart = off.QuadPart + (gpt_u64)len;
    if (off.QuadPart < 0 || end.QuadPart > ext->PartitionLength.QuadPart) {
        if (off.QuadPart >= ext->PartitionLength.QuadPart) return Complete(Irp, STATUS_END_OF_FILE, 0);
        return Complete(Irp, STATUS_INVALID_PARAMETER, 0);
    }
    if (len == 0) return Complete(Irp, STATUS_SUCCESS, 0);
    CopyStackDown(nxt, cur, ext->LowerDevice);
    nxt->Parameters.ReadWrite.ByteOffset.QuadPart = off.QuadPart + ext->StartingOffset.QuadPart;
    return IoCallDriver(ext->LowerDevice, Irp);
}

static NTSTATUS ControlIoctl(PIRP Irp, ULONG Code, PVOID Sys, ULONG InLen, ULONG OutLen);
static NTSTATUS SetVolumeType(PPART_EXT ext, UCHAR type);

static NTSTATUS NTAPI DispDeviceControl(PDEVICE_OBJECT Dev, PIRP Irp)
{
    PIO_STACK_LOCATION cur = IoGetCurrentIrpStackLocation(Irp);
    ULONG code = cur->Parameters.DeviceIoControl.IoControlCode;
    ULONG outlen = cur->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG inlen = cur->Parameters.DeviceIoControl.InputBufferLength;
    PVOID sys = Irp->AssociatedIrp.SystemBuffer;
    PPART_EXT ext = (PPART_EXT)Dev->DeviceExtension;
    PIO_STACK_LOCATION nxt;
    if (ext->Kind == KIND_CTRL) return ControlIoctl(Irp, code, sys, inlen, outlen);
    if (ext->Gone) return Complete(Irp, STATUS_NO_SUCH_DEVICE, 0);
    if (ext->Ldm) return LdmDeviceControl(ext, Irp, code, sys, inlen, outlen);
    switch (code) {
    case IOCTL_DISK_GET_PARTITION_INFO: {
        PPARTITION_INFORMATION pi = (PPARTITION_INFORMATION)sys;
        if (outlen < sizeof(PARTITION_INFORMATION)) return Complete(Irp, STATUS_BUFFER_TOO_SMALL, 0);
        pi->StartingOffset = ext->StartingOffset; pi->PartitionLength = ext->PartitionLength;
        pi->HiddenSectors = ext->StartLbaLow; pi->PartitionNumber = ext->PartitionNumber;
        /* report the classic equivalent of an LBA type, for the file
           systems of the releases that predate the LBA types */
        pi->PartitionType = (ext->PartitionType == 0x0E) ? 0x06 : ext->PartitionType;
        pi->BootIndicator = ext->BootIndicator ? TRUE : FALSE;
        pi->RecognizedPartition = ext->Recognized ? TRUE : FALSE; pi->RewritePartition = FALSE;
        return Complete(Irp, STATUS_SUCCESS, sizeof(PARTITION_INFORMATION));
    }
    case IOCTL_DISK_IS_WRITABLE: return Complete(Irp, STATUS_SUCCESS, 0);
    case IOCTL_DISK_SET_PARTITION_INFO:
        /* FORMAT sets the type byte of the volume it formats */
        if (inlen < 1) return Complete(Irp, STATUS_INVALID_PARAMETER, 0);
        return Complete(Irp, SetVolumeType(ext, *(UCHAR *)sys), 0);
    case IOCTL_DISK_VERIFY:
        /* VERIFY_INFORMATION.StartingOffset is volume-relative */
        if (inlen >= sizeof(LARGE_INTEGER) + sizeof(ULONG))
            ((PLARGE_INTEGER)sys)->QuadPart += ext->StartingOffset.QuadPart;
        nxt = IoGetNextIrpStackLocation(Irp);
        CopyStackDown(nxt, cur, ext->LowerDevice);
        return IoCallDriver(ext->LowerDevice, Irp);
    case IOCTL_DISK_SET_DRIVE_LAYOUT: return Complete(Irp, STATUS_NOT_SUPPORTED, 0);
    default:
        nxt = IoGetNextIrpStackLocation(Irp);
        CopyStackDown(nxt, cur, ext->LowerDevice);
        return IoCallDriver(ext->LowerDevice, Irp);
    }
}

static NTSTATUS NTAPI DispPassThrough(PDEVICE_OBJECT Dev, PIRP Irp)
{
    PPART_EXT ext = (PPART_EXT)Dev->DeviceExtension;
    PIO_STACK_LOCATION cur = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nxt = IoGetNextIrpStackLocation(Irp);
    if (ext->Kind != KIND_PART || ext->Gone) return Complete(Irp, STATUS_SUCCESS, 0);
    CopyStackDown(nxt, cur, ext->LowerDevice);
    return IoCallDriver(ext->LowerDevice, Irp);
}

/* Assign the first free drive letter in ascending order, starting at C:.
   IoCreateSymbolicLink fails with a name collision for a letter already in
   use (the system's own C:, D:, ... volumes), so the first that succeeds is
   the first free one. */
static UCHAR AssignDriveLetter(const char *DeviceName)
{
    char link[32]; UNICODE_STRING ul, ut; NTSTATUS st; UCHAR c;
    for (c = 'C'; c <= 'Z'; c++) {
        sprintf(link, "\\DosDevices\\%c:", c);
        if (!NT_SUCCESS(MakeUnicode(link, &ul))) return 0;
        if (!NT_SUCCESS(MakeUnicode(DeviceName, &ut))) { RtlFreeUnicodeString(&ul); return 0; }
        st = IoCreateSymbolicLink(&ul, &ut);
        RtlFreeUnicodeString(&ul); RtlFreeUnicodeString(&ut);
        if (NT_SUCCESS(st)) return c;
    }
    return 0;
}

static void PublishPartition(PDEVICE_OBJECT Part0, ULONG DiskNumber, ULONG PartNumber, GPT_PARTITION *P, ULONG Bps)
{
    char name[48]; UNICODE_STRING uname; PDEVICE_OBJECT dev, p0dev; PFILE_OBJECT p0file; PPART_EXT ext; NTSTATUS st;
    (void)Part0;
    sprintf(name, "\\Device\\Harddisk%lu\\Partition%lu", (unsigned long)DiskNumber, (unsigned long)PartNumber);
    if (!NT_SUCCESS(MakeUnicode(name, &uname))) return;
    st = IoCreateDevice(g_Driver, sizeof(PART_EXT), &uname, FILE_DEVICE_DISK, 0, FALSE, &dev);
    RtlFreeUnicodeString(&uname);
    if (!NT_SUCCESS(st)) { D("  create "); D(name); D(" failed st="); DH(st); D("\n"); return; }
    if (!NT_SUCCESS(OpenPartition0(DiskNumber, &p0file, &p0dev))) { IoDeleteDevice(dev); return; }
    ext = (PPART_EXT)dev->DeviceExtension;
    ext->Kind = KIND_PART; ext->Self = dev; ext->LowerDevice = p0dev; ext->LowerFile = p0file;
    ext->StartingOffset.QuadPart = (gpt_u64)P->StartLba * (gpt_u64)Bps;
    ext->PartitionLength.QuadPart = (gpt_u64)P->SectorCount * (gpt_u64)Bps;
    ext->StartLbaLow = (ULONG)P->StartLba; ext->BytesPerSector = Bps;
    ext->DiskNumber = DiskNumber; ext->PartitionNumber = PartNumber;
    ext->PartitionType = P->MbrType; ext->BootIndicator = P->BootIndicator ? 1 : 0;
    ext->Recognized = P->Recognized ? 1 : 0; ext->DriveLetter = 0; ext->Stale = 0; ext->Gone = 0;
    ext->Ldm = NULL; { int k = 0; while (name[k] && k < 39) { ext->DevName[k] = name[k]; k++; } ext->DevName[k] = 0; }
    dev->Flags |= DO_DIRECT_IO;
    dev->StackSize = (CCHAR)(p0dev->StackSize + 1);
    dev->AlignmentRequirement = p0dev->AlignmentRequirement;
    dev->Flags &= ~DO_DEVICE_INITIALIZING;
    if (P->MbrType != GPT_MBRTYPE_EFISYS && P->MbrType != GPT_MBRTYPE_EXTENDED && P->MbrType != GPT_MBRTYPE_EXTENDL)
        ext->DriveLetter = AssignDriveLetter(name);
    LockList(); ext->Next = g_PartList; g_PartList = ext; UnlockList();
    D("  "); D(name); D(" type="); DH(P->MbrType); D(" letter="); DH(ext->DriveLetter); D("\n");
}

/* First \Device\HarddiskN\PartitionM name not taken by any device. A name
   is free only if it is not one of this driver's volumes and opening it
   fails with "name not found" - opening a mounted volume can fail for other
   reasons (the file system answers), and that name is still taken. */
static ULONG FirstFreePartitionIndex(ULONG DiskNumber)
{
    char name[48]; UNICODE_STRING uname; PFILE_OBJECT file; PDEVICE_OBJECT dev; NTSTATUS st; ULONG m;
    PPART_EXT p; int ours;
    for (m = 1; m < 64; m++) {
        ours = 0;
        LockList();
        for (p = g_PartList; p; p = p->Next)
            if (p->DiskNumber == DiskNumber && p->PartitionNumber == m) { ours = 1; break; }
        UnlockList();
        if (ours) continue;
        sprintf(name, "\\Device\\Harddisk%lu\\Partition%lu", (unsigned long)DiskNumber, (unsigned long)m);
        if (!NT_SUCCESS(MakeUnicode(name, &uname))) return m;
        st = IoGetDeviceObjectPointer(&uname, 0, &file, &dev);
        RtlFreeUnicodeString(&uname);
        if (NT_SUCCESS(st)) { ObDereferenceObject(file); continue; }
        if (st == (NTSTATUS)0xC0000034L /* OBJECT_NAME_NOT_FOUND */ ||
            st == (NTSTATUS)0xC000003AL /* OBJECT_PATH_NOT_FOUND */) return m;
    }
    return 64;
}

static ULONG ParseDisk(ULONG DiskNumber, GPT_DISK *disk)
{
    PFILE_OBJECT file; PDEVICE_OBJECT part0; DISK_GEOMETRY geo; ENUMCTX ctx; ULONG bps; gpt_u64 total; NTSTATUS st;
    if (!NT_SUCCESS(OpenPartition0(DiskNumber, &file, &part0))) return 0;
    st = SyncIoctl(IOCTL_DISK_GET_DRIVE_GEOMETRY, part0, NULL, 0, &geo, sizeof(geo));
    if (!NT_SUCCESS(st)) { ObDereferenceObject(file); return 0; }
    bps = geo.BytesPerSector;
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) { ObDereferenceObject(file); return 0; }
    total = (gpt_u64)geo.Cylinders.QuadPart * (gpt_u64)geo.TracksPerCylinder * (gpt_u64)geo.SectorsPerTrack;
    ctx.Part0 = part0; ctx.BytesPerSector = bps; ctx.Limit = 0;
    st = GptParse(EnumRead, &ctx, bps, total, disk);
    ObDereferenceObject(file);
    if (st != GPT_OK) return 0;
    disk->Heads = geo.TracksPerCylinder;
    disk->SectorsPerTrack = geo.SectorsPerTrack;
    return bps;
}

static int DiskOwned(GPT_DISK *disk, ULONG bps)
{
    if (disk->IsGpt) return 1;
    if (bps != 512 && disk->Count > 0) return 1;
    return 0;
}

/* ---- control device ---- */

static void FillMgptPart(MGPT_PART *mp, GPT_PARTITION *p)
{
    memset(mp, 0, sizeof(*mp));
    mp->StartLba = p->StartLba; mp->SectorCount = p->SectorCount; mp->MbrType = p->MbrType;
    mp->FsKind = p->FsKind;
    mp->Flags = (p->BootIndicator ? MGPT_F_BOOT : 0) | (p->IsLogical ? MGPT_F_LOGICAL : 0) |
                (p->Recognized ? MGPT_F_RECOGNIZED : 0) | (p->IsGptEntry ? MGPT_F_GPTENTRY : 0);
    mp->DriveLetter = 0;
}

static int IsDynamicTable(GPT_DISK *disk);
static void LdmLayout(ULONG dn, GPT_DISK *disk, MGPT_LAYOUT *lay);

static NTSTATUS DoList(PVOID sys, ULONG outlen, ULONG *info)
{
    MGPT_DISKLIST *dl = (MGPT_DISKLIST *)sys; GPT_DISK *disk; ULONG d, bps;
    if (outlen < sizeof(MGPT_DISKLIST)) return STATUS_BUFFER_TOO_SMALL;
    disk = (GPT_DISK *)ExAllocatePool(NonPagedPool, sizeof(GPT_DISK));
    if (!disk) return STATUS_INSUFFICIENT_RESOURCES;
    dl->Count = 0;
    for (d = 0; d < MGPT_MAXDISK; d++) {
        bps = ParseDisk(d, disk);
        if (bps == 0) continue;
        { MGPT_DISKINFO *di = &dl->Disk[dl->Count];
          di->DiskNumber = d; di->SectorSize = bps; di->TotalSectors = disk->DiskSectors;
          di->IsGpt = disk->IsGpt; di->PartCount = disk->Count; di->Owned = (ULONG)DiskOwned(disk, bps);
          di->Dynamic = (ULONG)IsDynamicTable(disk);
          dl->Count++; }
    }
    ExFreePool(disk);
    *info = sizeof(MGPT_DISKLIST);
    return STATUS_SUCCESS;
}

static int IsExtType(gpt_u8 t);
static int LayoutPartNumber(GPT_DISK *disk, ULONG index);
static int ClassPartNumber(ULONG DiskNumber, GPT_DISK *disk, ULONG index, ULONG bps);
static int IsProtectedPartition(ULONG DiskNumber, GPT_DISK *disk, ULONG index, ULONG bps);
static void BuildLetterMap(void);
static UCHAR MapLookup(ULONG disk, ULONG part);
static UCHAR MapLookupOff(ULONG disk, gpt_u64 off);
static UCHAR OwnedLetterAt(ULONG disk, gpt_u64 startLba, ULONG bps);

static NTSTATUS DoGetLayout(PVOID sys, ULONG inlen, ULONG outlen, ULONG *info)
{
    MGPT_LAYOUT *lay = (MGPT_LAYOUT *)sys; GPT_DISK *disk; ULONG dn, bps, i; int owned;
    if (inlen < sizeof(ULONG)) return STATUS_INVALID_PARAMETER;
    if (outlen < sizeof(MGPT_LAYOUT)) return STATUS_BUFFER_TOO_SMALL;
    dn = *(ULONG *)sys;
    disk = (GPT_DISK *)ExAllocatePool(NonPagedPool, sizeof(GPT_DISK));
    if (!disk) return STATUS_INSUFFICIENT_RESOURCES;
    bps = ParseDisk(dn, disk);
    if (bps == 0) { ExFreePool(disk); return STATUS_DEVICE_NOT_READY; }
    lay->DiskNumber = dn; lay->SectorSize = bps; lay->TotalSectors = disk->DiskSectors;
    lay->IsGpt = disk->IsGpt; lay->Count = disk->Count;
    lay->ChsLimit = GptChsLimit(disk);
    owned = DiskOwned(disk, bps);
    lay->Dynamic = 0; lay->LdmGroup = 0; lay->LdmWritable = 0; lay->LdmMissing = 0;
    lay->PubStart = 0; lay->PubSize = 0; lay->DgName[0] = 0;
    if (IsDynamicTable(disk)) {
        LdmLayout(dn, disk, lay);
        if (lay->Dynamic) { ExFreePool(disk); *info = sizeof(MGPT_LAYOUT); return STATUS_SUCCESS; }
        lay->Dynamic = 1; lay->LdmGroup = 0xFFFFFFFFUL;   /* database not readable */
    }
    BuildLetterMap();
    for (i = 0; i < disk->Count && i < MGPT_MAX_PARTS; i++) {
        UCHAR letter = 0;
        FillMgptPart(&lay->Part[i], &disk->Part[i]);
        if (!IsExtType(disk->Part[i].MbrType)) {
            /* a volume of ours (any disk), else the class driver's */
            letter = OwnedLetterAt(dn, disk->Part[i].StartLba, bps);
            if (!letter && !owned) { int pn = ClassPartNumber(dn, disk, i, bps); if (pn) letter = MapLookup(dn, (ULONG)pn); }
            if (!letter) letter = MapLookupOff(dn, disk->Part[i].StartLba * bps);
        }
        lay->Part[i].DriveLetter = letter;
        if (IsProtectedPartition(dn, disk, i, bps)) lay->Part[i].Flags |= MGPT_F_PROTECTED;
    }
    ExFreePool(disk);
    *info = sizeof(MGPT_LAYOUT);
    return STATUS_SUCCESS;
}

/* in-use detection, to avoid tearing down a mounted volume device */
#define VPB_MOUNTED 0x0001
typedef struct _VPB_MIN { USHORT Type, Size, Flags, VolumeLabelLength; PVOID DeviceObject, RealDevice; ULONG SerialNumber, ReferenceCount; } VPB_MIN;
static int DeviceInUse(PDEVICE_OBJECT dev)
{ VPB_MIN *vpb = (VPB_MIN *)dev->Vpb; if (dev->ReferenceCount > 0) return 1; if (vpb && (vpb->Flags & VPB_MOUNTED)) return 1; return 0; }
/* ---- locate the running system partition (any disk / index) ---------
   Resolve \SystemRoot (and the \ArcName indirection) to the physical
   \Device\HarddiskN\PartitionM, then its starting offset. This is the
   partition the OS is running from; deleting it, or wiping its disk, is
   refused. Falls back to the active partition on disk 0 if resolution
   fails. */

static WCHAR g_lb1[280], g_lb2[280];
static int   g_sysKnown = 0, g_sysOk = 0;
static ULONG g_sysDisk = 0, g_sysPartNum = 0;

static NTSTATUS ResolveLink(PCWSTR linkName, PWSTR outBuf, USHORT outChars)
{
    UNICODE_STRING un, target; OBJECT_ATTRIBUTES oa; HANDLE h; NTSTATUS st; ULONG ret = 0;
    RtlInitUnicodeString(&un, linkName);
    InitObjAttr(&oa, &un);
    st = ZwOpenSymbolicLinkObject(&h, SYMBOLIC_LINK_QUERY, &oa);
    if (!NT_SUCCESS(st)) return st;
    target.Buffer = outBuf; target.Length = 0; target.MaximumLength = (USHORT)((outChars - 1) * sizeof(WCHAR));
    st = ZwQuerySymbolicLinkObject(h, &target, &ret);
    ZwClose(h);
    if (NT_SUCCESS(st)) outBuf[target.Length / sizeof(WCHAR)] = 0;
    return st;
}

/* find ASCII pattern in a wide, null-terminated string; return index of
   first char after the match, or -1 */
static int WFind(const WCHAR *s, const char *pat)
{
    int i, j;
    for (i = 0; s[i]; i++) {
        for (j = 0; pat[j]; j++) {
            WCHAR c = s[i + j];
            char pc = pat[j];
            if (c >= 'A' && c <= 'Z') c = (WCHAR)(c + 32);
            if (pc >= 'A' && pc <= 'Z') pc = (char)(pc + 32);
            if (c != (WCHAR)pc) break;
        }
        if (!pat[j]) return i + j;
    }
    return -1;
}
static ULONG WNum(const WCHAR *s, int from)
{ ULONG v = 0; while (s[from] >= '0' && s[from] <= '9') { v = v * 10 + (ULONG)(s[from] - '0'); from++; } return v; }

static void ComputeSystemPartition(void)
{
    NTSTATUS st; const WCHAR *dp; int hd, pt; ULONG diskN, partM;

    g_sysKnown = 1; g_sysOk = 0;
    st = ResolveLink(L"\\SystemRoot", g_lb1, 280);
    D("sysroot st="); DH(st); D("\n");
    if (!NT_SUCCESS(st)) return;

    if (WFind(g_lb1, "Harddisk") >= 0) {
        dp = g_lb1;
    } else {
        /* ArcName form: strip the trailing "\<dir>" then resolve it */
        int i, cut = -1;
        for (i = 0; g_lb1[i]; i++) if (g_lb1[i] == '\\') cut = i;
        if (cut > 0) g_lb1[cut] = 0;
        st = ResolveLink(g_lb1, g_lb2, 280);
        D("arc st="); DH(st); D("\n");
        if (!NT_SUCCESS(st)) return;
        dp = g_lb2;
    }

    hd = WFind(dp, "Harddisk");
    pt = WFind(dp, "Partition");
    if (hd < 0 || pt < 0) return;
    diskN = WNum(dp, hd);
    partM = WNum(dp, pt);
    g_sysDisk = diskN;
    g_sysPartNum = partM;      /* NT device partition number (1-based) */
    g_sysOk = 1;
    D("sys disk="); DH(diskN); D(" partNum="); DH(partM); D(" ok\n");
}

/* ---- what the in-box disk class driver serves ------------------------
   On NT 3.x / 4.0 the class driver (atdisk, scsidisk, ...) names each of its
   partition devices \Device\HarddiskN\PartitionM. Walking its device list
   (from the driver object behind Partition0) and asking every device for
   its PARTITION_INFORMATION directly - not through a file system - tells
   exactly which disk ranges it has volumes for. This is how partitions it
   does not handle are found: an LBA type it does not mount (0E / 0B / 0C on
   NT 3.1) or logical drives in a container it does not follow (0F on
   NT 3.1). Windows 2000+ names these devices differently; the view then
   stays empty and such disks are left to the class driver, which handles
   the LBA types itself. Rebuilt (from a system thread) before every
   update of a disk, so a disk that appears later is covered too. On a
   release whose class driver has no such devices at start-up (Windows
   2000+, where devices come and go with Plug and Play) the list is never
   walked again. */

#define CV_MAX 64
typedef struct { ULONG disk, part; gpt_u64 off, len; PDEVICE_OBJECT dev; } CLASSVOL;
static CLASSVOL g_cv[CV_MAX];
static ULONG    g_cvN;
static UCHAR    g_nameBuf[640];
static UCHAR    g_piBuf[64];

static int g_cvWalk = -1;     /* -1 not yet tried, 0 never again, 1 walk */

static void BuildClassView(void)
{
    PDRIVER_OBJECT seen[4]; ULONG nseen = 0, d, k;
    g_cvN = 0;
    if (g_cvWalk == 0) return;
    for (d = 0; d < MGPT_MAXDISK; d++) {
        PFILE_OBJECT file; PDEVICE_OBJECT part0, dev; PDRIVER_OBJECT drv; int dup = 0;
        if (!NT_SUCCESS(OpenPartition0(d, &file, &part0))) continue;
        drv = part0->DriverObject;
        ObDereferenceObject(file);
        for (k = 0; k < nseen; k++) if (seen[k] == drv) dup = 1;
        if (dup || drv == g_Driver || nseen >= 4) continue;
        seen[nseen++] = drv;
        for (dev = drv->DeviceObject; dev && g_cvN < CV_MAX; dev = dev->NextDevice) {
            ULONG ret = 0, n; PUNICODE_STRING nm = (PUNICODE_STRING)g_nameBuf; WCHAR *w; int hd, pt;
            if (!NT_SUCCESS(ObQueryNameString(dev, g_nameBuf, sizeof(g_nameBuf) - 4, &ret))) continue;
            if (!nm->Buffer || !nm->Length) continue;
            w = nm->Buffer; n = nm->Length / sizeof(WCHAR);
            if ((UCHAR *)(w + n + 1) > g_nameBuf + sizeof(g_nameBuf)) continue;
            w[n] = 0;
            hd = WFind(w, "\\Device\\Harddisk"); pt = WFind(w, "\\Partition");
            if (hd < 0 || pt < 0 || WNum(w, pt) == 0) continue;
            /* the class driver's PARTITION_INFORMATION is 32 bytes (8-byte
               aligned); offset at 0, length at 8 */
            if (!NT_SUCCESS(SyncIoctl(IOCTL_DISK_GET_PARTITION_INFO, dev, NULL, 0, g_piBuf, 32))) continue;
            g_cv[g_cvN].disk = WNum(w, hd);
            g_cv[g_cvN].part = WNum(w, pt);
            g_cv[g_cvN].off  = *(gpt_u64 *)g_piBuf;
            g_cv[g_cvN].len  = *(gpt_u64 *)(g_piBuf + 8);
            g_cv[g_cvN].dev  = dev;
            D("  class vol disk="); DH(g_cv[g_cvN].disk); D(" part="); DH(g_cv[g_cvN].part);
            D(" lba="); DH((ULONG)(g_cv[g_cvN].off / 512)); D("\n");
            g_cvN++;
        }
    }
    if (g_cvWalk < 0) g_cvWalk = g_cvN ? 1 : 0;
}

/* index of the class volume covering exactly (disk, offset, length), or -1 */
static int ClassFind(ULONG disk, gpt_u64 off, gpt_u64 len)
{
    ULONG i;
    for (i = 0; i < g_cvN; i++)
        if (g_cv[i].disk == disk && g_cv[i].off == off && g_cv[i].len == len) return (int)i;
    return -1;
}

static int IsExtType(gpt_u8 t) { return t == GPT_MBRTYPE_EXTENDED || t == GPT_MBRTYPE_EXTENDL; }

/* The NT device partition number (\Device\HarddiskD\Partition<n>) of the
   layout entry at Index as the class driver numbers them: from the class
   view when there is one, else by counting - non-extended partitions are
   numbered 1..N in layout order (primaries in slot order, then logicals);
   the extended container itself has no number. */
static int LayoutPartNumber(GPT_DISK *disk, ULONG index);
static int ClassPartNumber(ULONG DiskNumber, GPT_DISK *disk, ULONG index, ULONG bps)
{
    int ci;
    if (index >= disk->Count) return 0;
    if (g_cvN) {
        ci = ClassFind(DiskNumber, (gpt_u64)disk->Part[index].StartLba * bps,
                       (gpt_u64)disk->Part[index].SectorCount * bps);
        return ci < 0 ? 0 : (int)g_cv[ci].part;
    }
    return LayoutPartNumber(disk, index);
}


/* The NT device partition number (\Device\HarddiskD\Partition<n>) of the
   layout entry at Index: non-extended partitions are numbered 1..N in
   layout order (primaries in slot order, then logicals); the extended
   container itself has no number. */
static int LayoutPartNumber(GPT_DISK *disk, ULONG index)
{
    ULONG i, n = 0;
    if (index >= disk->Count) return 0;
    if (IsExtType(disk->Part[index].MbrType)) return 0;
    for (i = 0; i <= index; i++)
        if (!IsExtType(disk->Part[i].MbrType)) n++;
    return (int)n;
}

/* Is (DiskNumber, partition at Index) the running system partition, or the
   active partition on the system disk? */
static int IsProtectedPartition(ULONG DiskNumber, GPT_DISK *disk, ULONG index, ULONG bps)
{
    if (!g_sysKnown) ComputeSystemPartition();
    if (index >= disk->Count) return 0;
    if (g_sysOk) {
        if (DiskNumber == g_sysDisk) {
            if ((ULONG)ClassPartNumber(DiskNumber, disk, index, bps) == g_sysPartNum) return 1; /* the OS partition */
            if (disk->Part[index].BootIndicator) return 1;                      /* active/NTLDR partition on the system disk */
        }
        return 0;
    }
    /* fallback: active partition on disk 0 */
    return (DiskNumber == 0 && disk->Part[index].BootIndicator) ? 1 : 0;
}

/* Is this disk the one the system runs from (so wiping it is refused)? */
static int IsSystemDisk(ULONG DiskNumber, GPT_DISK *disk)
{
    ULONG i;
    if (!g_sysKnown) ComputeSystemPartition();
    if (g_sysOk) return (DiskNumber == g_sysDisk) ? 1 : 0;
    if (DiskNumber == 0)
        for (i = 0; i < disk->Count; i++) if (disk->Part[i].BootIndicator) return 1;
    return 0;
}

/* ---- drive-letter reporting ----------------------------------------
   Build a map of letter -> (disk, NT partition number) by resolving each
   \DosDevices\<X>: symbolic link to its \Device\HarddiskN\PartitionM
   target. Used to show a letter for class-driver volumes; letters for the
   disks this driver owns come straight from the published PART_EXT. */

static WCHAR g_lmbuf[280];
#define LMAP_MAX 24
static struct { ULONG disk, part; gpt_u64 off; UCHAR letter; } g_lmap[LMAP_MAX];
static UCHAR g_extBuf[8 + 24 * 4];
static ULONG g_lmapN;

static void BuildLetterMap(void)
{
    UCHAR c;
    g_lmapN = 0;
    for (c = 'C'; c <= 'Z'; c++) {
        WCHAR link[20]; int i = 0; const char *pfx = "\\DosDevices\\";
        int hd, pt;
        while (pfx[i]) { link[i] = (WCHAR)(UCHAR)pfx[i]; i++; }
        link[i++] = (WCHAR)c; link[i++] = (WCHAR)':'; link[i] = 0;
        if (!NT_SUCCESS(ResolveLink(link, g_lmbuf, 280))) continue;
        hd = WFind(g_lmbuf, "Harddisk");
        pt = WFind(g_lmbuf, "Partition");
        if (hd >= 0 && pt >= 0) {
            if (g_lmapN < LMAP_MAX) {
                g_lmap[g_lmapN].disk = WNum(g_lmbuf, hd);
                g_lmap[g_lmapN].part = WNum(g_lmbuf, pt);
                g_lmap[g_lmapN].off = ~(gpt_u64)0;
                g_lmap[g_lmapN].letter = c;
                g_lmapN++;
            }
        } else if (WFind(g_lmbuf, "HarddiskVolume") >= 0 && g_lmapN < LMAP_MAX) {
            /* Windows 2000 and later: \Device\HarddiskVolumeN - ask the volume
               where it lies */
            char a[24]; UNICODE_STRING un; PFILE_OBJECT f; PDEVICE_OBJECT dev;
            sprintf(a, "\\DosDevices\\%c:", c);
            if (!NT_SUCCESS(MakeUnicode(a, &un))) continue;
            if (NT_SUCCESS(IoGetDeviceObjectPointer(&un, FILE_READ_ATTRIBUTES, &f, &dev))) {
                g_extBuf[0] = g_extBuf[1] = g_extBuf[2] = g_extBuf[3] = 0;
                if (NT_SUCCESS(SyncIoctl(0x00560000UL /* IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS */, dev, NULL, 0, g_extBuf, sizeof(g_extBuf))) &&
                    *(ULONG *)g_extBuf == 1) {
                    g_lmap[g_lmapN].disk = *(ULONG *)(g_extBuf + 8);
                    g_lmap[g_lmapN].part = 0;
                    g_lmap[g_lmapN].off = *(gpt_u64 *)(g_extBuf + 16);
                    g_lmap[g_lmapN].letter = c;
                    g_lmapN++;
                }
                ObDereferenceObject(f);
            }
            RtlFreeUnicodeString(&un);
        }
    }
}

static UCHAR MapLookup(ULONG disk, ULONG part)
{ ULONG i; for (i = 0; i < g_lmapN; i++) if (part && g_lmap[i].disk == disk && g_lmap[i].part == part) return g_lmap[i].letter; return 0; }
static UCHAR MapLookupOff(ULONG disk, gpt_u64 off)
{ ULONG i; for (i = 0; i < g_lmapN; i++) if (g_lmap[i].disk == disk && g_lmap[i].off == off) return g_lmap[i].letter; return 0; }

/* letter of an owned partition, matched by its starting offset */
static UCHAR OwnedLetterAt(ULONG disk, gpt_u64 startLba, ULONG bps)
{
    PPART_EXT p; UCHAR l = 0; gpt_u64 off = (gpt_u64)startLba * (gpt_u64)bps;
    LockList();
    for (p = g_PartList; p; p = p->Next)
        if (p->DiskNumber == disk && !p->Stale && (gpt_u64)p->StartingOffset.QuadPart == off) { l = p->DriveLetter; break; }
    UnlockList();
    return l;
}

struct _LDMRT;
static void LdmFreeRt(struct _LDMRT *rt);
static void RetireStale(ULONG DiskNumber);

static void UnlinkLetter(PPART_EXT p)
{
    if (p->DriveLetter) {
        char link[32]; UNICODE_STRING ul;
        sprintf(link, "\\DosDevices\\%c:", p->DriveLetter);
        if (NT_SUCCESS(MakeUnicode(link, &ul))) { IoDeleteSymbolicLink(&ul); RtlFreeUnicodeString(&ul); }
        p->DriveLetter = 0;
    }
}

static void VolumeName(PPART_EXT p, char *name)
{ int i = 0; while (p->DevName[i]) { name[i] = p->DevName[i]; i++; } name[i] = 0; }

/* Remove one published volume: its drive letter link and device object. */
static void RemoveVolume(PPART_EXT p)
{
    if (p->DriveLetter) {
        char l[32]; UNICODE_STRING ul;
        sprintf(l, "\\DosDevices\\%c:", p->DriveLetter);
        if (NT_SUCCESS(MakeUnicode(l, &ul))) { IoDeleteSymbolicLink(&ul); RtlFreeUnicodeString(&ul); }
    }
    if (p->LowerFile) ObDereferenceObject(p->LowerFile);
    if (p->Ldm) LdmFreeRt(p->Ldm);
    IoDeleteDevice(p->Self);
}

/* Send a file-system control request (FSCTL) for an open volume. NT 3.1
   exports no ZwFsControlFile, so the IRP is built here and sent to the
   file system's volume device. */
static NTSTATUS FsControl(PFILE_OBJECT fo, ULONG code)
{
    PDEVICE_OBJECT dev = IoGetRelatedDeviceObject(fo);
    KEVENT ev; IO_STATUS_BLOCK iosb; PIRP irp; PIO_STACK_LOCATION sl; NTSTATUS st;
    KeInitializeEvent(&ev, NotificationEvent, FALSE);
    irp = IoBuildDeviceIoControlRequest(code, dev, NULL, 0, NULL, 0, FALSE, &ev, &iosb);
    if (!irp) return STATUS_INSUFFICIENT_RESOURCES;
    sl = IoGetNextIrpStackLocation(irp);
    sl->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
    sl->MinorFunction = IRP_MN_USER_FS_REQUEST;
    sl->FileObject = fo;
    st = IoCallDriver(dev, irp);
    if (st == STATUS_PENDING) { KeWaitForSingleObject(&ev, Executive, KernelMode, FALSE, NULL); st = iosb.Status; }
    return st;
}

/* A file system usually mounts a volume as soon as anything touches it,
   so "mounted" alone does not mean "in use". Lock the volume - which only
   succeeds when no file on it is open - and dismount it, as FORMAT does.
   Returns 1 when the volume was idle and is now dismounted. */
static int DismountIfIdle(const char *devName)
{
    UNICODE_STRING un; OBJECT_ATTRIBUTES oa; HANDLE h; IO_STATUS_BLOCK iosb; NTSTATUS st; PFILE_OBJECT fo; int ok = 0;
    if (!NT_SUCCESS(MakeUnicode(devName, &un))) return 0;
    InitObjAttr(&oa, &un);
    st = ZwOpenFile(&h, SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_NONALERT);
    RtlFreeUnicodeString(&un);
    if (!NT_SUCCESS(st)) return 0;
    if (NT_SUCCESS(ObReferenceObjectByHandle(h, 0, NULL, KernelMode, (PVOID *)&fo, NULL))) {
        st = FsControl(fo, FSCTL_LOCK_VOLUME);
        D("  lock "); D(devName); D(" st="); DH(st); D("\n");
        if (NT_SUCCESS(st)) { FsControl(fo, FSCTL_DISMOUNT_VOLUME); ok = 1; }
        ObDereferenceObject(fo);
    }
    ZwClose(h);
    return ok;
}

/* types a volume is published for on a disk the class driver reads */
static int IsDataType(gpt_u8 t)
{ return t == 0x01 || t == 0x04 || t == 0x06 || t == 0x07 || t == 0x0B || t == 0x0C || t == 0x0E; }

/* Does the class driver serve this partition with a usable volume? It
   does if it has a device for exactly this range - except for the LBA /
   FAT32 types, which the old class drivers create a device for but do not
   mount or letter; those count only when the class volume has a letter. */
static int ClassServes(ULONG dn, gpt_u64 off, gpt_u64 len, gpt_u8 type)
{
    int ci = ClassFind(dn, off, len);
    if (ci < 0) return 0;
    if (type == 0x0B || type == 0x0C || type == 0x0E) return MapLookup(dn, g_cv[ci].part) != 0;
    return 1;
}

/* Should this driver publish a volume for partition g? */
static int WantVolume(ULONG dn, GPT_PARTITION *g, ULONG bps, int owned, int classDisk)
{
#if defined(_WIN64)
    /* x64 Windows reads GPT, dynamic disks and every sector size itself */
    (void)dn; (void)g; (void)bps; (void)owned; (void)classDisk;
    return 0;
#endif
    if (IsExtType(g->MbrType)) return 0;
    /* GPT reserved / LDM partitions and MBR dynamic disks hold no file system */
    if (g->FsKind == GPT_FS_MSR || g->FsKind == GPT_FS_LDMMETA || g->FsKind == GPT_FS_LDMDATA ||
        g->MbrType == GPT_MBRTYPE_LDM) return 0;
    if (owned) return 1;
    if (!classDisk || !IsDataType(g->MbrType)) return 0;
    return !ClassServes(dn, (gpt_u64)g->StartLba * bps, (gpt_u64)g->SectorCount * bps, g->MbrType);
}

/* Bring the published volumes of a disk in line with its (new) table,
   without touching volumes that are in use:
     - a volume whose partition is gone is removed, unless it is mounted /
       open (deleting such a device would bugcheck the old kernels); that
       one stays until the next restart;
     - a volume whose partition is still there is kept, with its letter,
       and its type / boot flag refreshed;
     - a partition with no volume yet is published and given the first
       free drive letter.
   Disks this driver owns (GPT, non-512 MBR) get a volume for every
   partition. On a 512-byte MBR disk read by an NT 3.x / 4.0 class driver,
   only the partitions that class driver does not serve get one (see
   ClassServes) - including partitions created since start-up, which the
   class driver only sees after a restart. A class volume whose partition
   was deleted loses its drive letter at once when it is not in use. */
static void SyncDisk(ULONG DiskNumber)
{
    GPT_DISK *disk; ULONG bps, i; int owned, classDisk;
    PPART_EXT p;
    PFILE_OBJECT file; PDEVICE_OBJECT part0;

    disk = (GPT_DISK *)ExAllocatePool(NonPagedPool, sizeof(GPT_DISK));
    if (!disk) return;
    KeWaitForSingleObject(&g_SyncLock, Executive, KernelMode, FALSE, NULL);
    BuildClassView();
    bps = ParseDisk(DiskNumber, disk);
    owned = (bps != 0) && DiskOwned(disk, bps);
    classDisk = (bps == 512) && !owned && !disk->IsGpt && g_cvN > 0;
    if (classDisk) BuildLetterMap();

    /* 1. mark the volumes whose partition is gone (or that the class driver
          serves now); refresh the others */
    LockList();
    for (p = g_PartList; p; p = p->Next) {
        int keep = 0;
        if (p->DiskNumber != DiskNumber) continue;
        if (p->Gone) { p->Stale = 1; continue; }   /* never serves again */
        if (bps)
            for (i = 0; i < disk->Count; i++) {
                GPT_PARTITION *g = &disk->Part[i];
                if (!WantVolume(DiskNumber, g, bps, owned, classDisk)) continue;
                if ((gpt_u64)p->StartingOffset.QuadPart == (gpt_u64)g->StartLba * bps &&
                    (gpt_u64)p->PartitionLength.QuadPart == (gpt_u64)g->SectorCount * bps) {
                    keep = 1;
                    p->PartitionType = g->MbrType;
                    p->BootIndicator = g->BootIndicator ? 1 : 0;
                    p->Recognized = g->Recognized ? 1 : 0;
                    break;
                }
            }
        p->Stale = (UCHAR)(keep ? 0 : 1);
    }
    UnlockList();

    /* 1a. a stale volume that is mounted but has no open file is
           dismounted (the list only changes under g_SyncLock, so it can be
           walked here without the list lock while doing I/O); 1b. the
           stale volumes that are free now are deleted; one still in use (a
           file on it is open) keeps its letter until it is closed */
    RetireStale(DiskNumber);

    /* 1c. a class volume whose partition was deleted (or moved) keeps its
           device until the restart; its drive letter goes now, once the
           volume is idle (dismounted first if mounted) */
    if (classDisk)
        for (i = 0; i < g_cvN; i++) {
            ULONG j; int still = 0; UCHAR l;
            if (g_cv[i].disk != DiskNumber) continue;
            l = MapLookup(DiskNumber, g_cv[i].part);
            if (!l) continue;
            for (j = 0; j < disk->Count; j++)
                if ((gpt_u64)disk->Part[j].StartLba * bps == g_cv[i].off &&
                    (gpt_u64)disk->Part[j].SectorCount * bps == g_cv[i].len) { still = 1; break; }
            if (still) continue;
            if (DeviceInUse(g_cv[i].dev)) {
                char name[48];
                sprintf(name, "\\Device\\Harddisk%lu\\Partition%lu", (unsigned long)DiskNumber, (unsigned long)g_cv[i].part);
                if (!DismountIfIdle(name)) continue;
            }
            {
                char link[32]; UNICODE_STRING ul;
                sprintf(link, "\\DosDevices\\%c:", l);
                if (NT_SUCCESS(MakeUnicode(link, &ul))) { IoDeleteSymbolicLink(&ul); RtlFreeUnicodeString(&ul); }
            }
        }

    /* 2. publish wanted partitions that have no volume yet */
    if (bps && (owned || classDisk) && NT_SUCCESS(OpenPartition0(DiskNumber, &file, &part0))) {
        for (i = 0; i < disk->Count; i++) {
            GPT_PARTITION *g = &disk->Part[i];
            gpt_u64 off = (gpt_u64)g->StartLba * bps;
            int have = 0;
            if (!WantVolume(DiskNumber, g, bps, owned, classDisk)) continue;
            LockList();
            for (p = g_PartList; p; p = p->Next)
                if (p->DiskNumber == DiskNumber && !p->Stale && (gpt_u64)p->StartingOffset.QuadPart == off) { have = 1; break; }
            UnlockList();
            if (!have) PublishPartition(part0, DiskNumber, FirstFreePartitionIndex(DiskNumber), g, bps);
        }
        ObDereferenceObject(file);
    }
    KeSetEvent(&g_SyncLock, 0, FALSE);
    ExFreePool(disk);
}

/* ---- dynamic disks (LDM) ---------------------------------------------
   A dynamic disk keeps a database (the last 1 MB of an MBR disk, the LDM
   metadata partition of a GPT disk) that describes the volumes of its disk
   group: simple, spanned, striped, mirrored and RAID-5 volumes made of
   extents on one or more disks. The groups are rebuilt from the disks after
   every change (LdmSync, in a system thread); each volume that can be read
   with the disks present gets a device \Device\MbrGptNtDm<n> that maps its
   I/O onto the member disks. A volume that lost redundancy (a mirror or
   RAID-5 volume with one member missing) is published read-only.

   Windows 2000 and later run their own LDM driver (dmio) for MBR dynamic
   disks: a group with an MBR member is left to it ("native") and only shown.
   x64 Windows also reads GPT dynamic disks itself: every group is native. */

#define LDM_DISKNUM   0xFFFFFFFEUL        /* PART_EXT.DiskNumber of a dynamic volume */
#define LDM_GROUPS    MGPT_LDM_GROUPS
#define LDM_BOUNCE    (64 * 1024)

void *memcpy(void *d, const void *s, SIZE_T n)
{ volatile UCHAR *a = (volatile UCHAR *)d; const UCHAR *b = (const UCHAR *)s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int c, SIZE_T n)
{ volatile UCHAR *a = (volatile UCHAR *)d; while (n--) *a++ = (UCHAR)c; return d; }

static void MemZero(void *d, ULONG n) { memset(d, 0, n); }
/* 64-bit unsigned division without the compiler's helper (NT 3.1 has none) */
static gpt_u64 UDiv(gpt_u64 n, ULONG d)
{
    gpt_u64 q = 0, r = 0; int i;
    if ((d & (d - 1)) == 0) { while (d > 1) { n >>= 1; d >>= 1; } return n; }
    for (i = 63; i >= 0; i--) { r = (r << 1) | ((n >> i) & 1); if (r >= d) { r -= d; q |= (gpt_u64)1 << i; } }
    return q;
}
static int  MemSame(const void *a, const void *b, ULONG n)
{ const UCHAR *x = (const UCHAR *)a, *y = (const UCHAR *)b; while (n--) if (*x++ != *y++) return 0; return 1; }
static int  StrSame(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void StrCopy(char *d, const char *s, ULONG max) { ULONG i = 0; while (s[i] && i + 1 < max) { d[i] = s[i]; i++; } d[i] = 0; }

typedef struct _LDMGRP {
    LDM_DB  *Db;
    char     DgGuid[40];
    int      Native;                      /* Windows' own LDM driver owns it */
    ULONG    NtDisk[LDM_MAX_DISKS];       /* per Db->Disk[]: \Device\Harddisk<n> */
    gpt_u8   Present[LDM_MAX_DISKS];
    gpt_u8   Stale[LDM_MAX_DISKS];        /* its database copy is older */
    gpt_u8   IsGpt[LDM_MAX_DISKS];
    gpt_u32  Bps[LDM_MAX_DISKS];
    gpt_u64  PubStart[LDM_MAX_DISKS];     /* member sectors */
    gpt_u64  PubSize[LDM_MAX_DISKS];
    gpt_u64  Total[LDM_MAX_DISKS];        /* sectors the disk driver reaches */
    LDM_HDR *Hdr[LDM_MAX_DISKS];
} LDMGRP;

typedef struct _LDMRT {
    LDM_VMAP       Map;
    gpt_u8         Guid[16];
    char           DgGuid[40];
    gpt_u8         Present[LDM_MAX_DISKS];
    ULONG          NtDisk[LDM_MAX_DISKS];
    PDEVICE_OBJECT Dev[LDM_MAX_DISKS];
    PFILE_OBJECT   File[LDM_MAX_DISKS];
    gpt_u64        FirstOff;              /* byte offset of the first extent */
    int            ReadOnly;
} LDMRT;

static LDMGRP *g_grp[LDM_GROUPS];
static int     g_ngrp;
static KEVENT  g_LdmLock;                 /* g_grp */
static KEVENT  g_LdmIoLock;               /* g_bounce / g_scratch (volume I/O) */
static KEVENT  g_LdmScanBufLock;          /* g_scanScratch */
static UCHAR  *g_bounce, *g_scratch, *g_scanScratch, *g_zero;
static ULONG   g_dmSeq;
#if !defined(_WIN64)
static int     g_osLdm = -1;
#endif
static WCHAR   g_osbuf[280];

static void LdmFreeRt(LDMRT *rt)
{
    int i;
    for (i = 0; i < LDM_MAX_DISKS; i++) if (rt->File[i]) ObDereferenceObject(rt->File[i]);
    ExFreePool(rt);
}

/* Does this Windows have its own LDM driver for MBR dynamic disks? On
   Windows 2000 and later \Device\HarddiskN\Partition0 is a symbolic link. */
static int OsHasLdm(void)
{
#if defined(_WIN64)
    return 1;
#else
    if (g_osLdm < 0) g_osLdm = NT_SUCCESS(ResolveLink(L"\\Device\\Harddisk0\\Partition0", g_osbuf, 280)) ? 1 : 0;
    return g_osLdm;
#endif
}

static int LdmBuffers(void)
{
    if (!g_bounce) g_bounce = (UCHAR *)ExAllocatePool(NonPagedPool, LDM_BOUNCE);
    if (!g_scratch) g_scratch = (UCHAR *)ExAllocatePool(NonPagedPool, 2 * LDM_SCRATCH_BYTES);
    if (!g_scanScratch) g_scanScratch = (UCHAR *)ExAllocatePool(NonPagedPool, 2 * LDM_SCRATCH_BYTES + 4096);
    return g_bounce && g_scratch && g_scanScratch;
}

/* ---- I/O to member disks from the read / write path ------------------
   A volume request may come in as paging I/O (at APC level), where an IRP
   that completes through the thread's APC would never finish. These IRPs
   have a completion routine that only signals an event; the IRP is freed
   here. The buffer must be nonpaged. */
static NTSTATUS NTAPI RawDone(PDEVICE_OBJECT Dev, PIRP Irp, PVOID Ctx)
{ (void)Dev; (void)Irp; KeSetEvent((PKEVENT)Ctx, 0, FALSE); return STATUS_MORE_PROCESSING_REQUIRED; }

static NTSTATUS RawIo(PDEVICE_OBJECT dev, int write, gpt_u64 off, ULONG len, PVOID buf)
{
    PIRP irp; PMDL mdl; KEVENT ev; PIO_STACK_LOCATION sl; NTSTATUS st;
    irp = IoAllocateIrp(dev->StackSize, FALSE);
    if (!irp) return STATUS_INSUFFICIENT_RESOURCES;
    mdl = IoAllocateMdl(buf, len, FALSE, FALSE, NULL);
    if (!mdl) { IoFreeIrp(irp); return STATUS_INSUFFICIENT_RESOURCES; }
    MmBuildMdlForNonPagedPool(mdl);
    KeInitializeEvent(&ev, NotificationEvent, FALSE);
    irp->MdlAddress = mdl;
    irp->UserBuffer = buf;
    irp->Tail.Overlay.Thread = KeGetCurrentThread();
    sl = IoGetNextIrpStackLocation(irp);
    sl->MajorFunction = (UCHAR)(write ? IRP_MJ_WRITE : IRP_MJ_READ);
    sl->Flags = SL_OVERRIDE_VERIFY_VOLUME;
    sl->Parameters.ReadWrite.Length = len;
    sl->Parameters.ReadWrite.ByteOffset.QuadPart = (LONGLONG)off;
    sl->CompletionRoutine = (PVOID)RawDone;
    sl->Context = &ev;
    sl->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;
    sl->DeviceObject = dev;
    st = IoCallDriver(dev, irp);
    KeWaitForSingleObject(&ev, Executive, KernelMode, FALSE, NULL);
    st = irp->IoStatus.Status;
    IoFreeMdl(mdl);
    IoFreeIrp(irp);
    return st;
}

/* LdmVolIo member I/O for a published volume, through the bounce buffer */
static int LdmVolDiskIo(void *ctx, gpt_u32 d, int write, gpt_u64 off, gpt_u32 len, void *buf)
{
    LDMRT *rt = (LDMRT *)ctx; UCHAR *b = (UCHAR *)buf;
    if (d >= LDM_MAX_DISKS || !rt->Dev[d]) return 1;
    while (len) {
        ULONG n = len > LDM_BOUNCE ? LDM_BOUNCE : len;
        if (write) memcpy(g_bounce, b, n);
        if (!NT_SUCCESS(RawIo(rt->Dev[d], write, off, n, g_bounce))) return 1;
        if (!write) memcpy(b, g_bounce, n);
        off += n; b += n; len -= n;
    }
    return 0;
}

static NTSTATUS LdmReadWrite(PPART_EXT ext, PIRP Irp)
{
    LDMRT *rt = ext->Ldm;
    PIO_STACK_LOCATION cur = IoGetCurrentIrpStackLocation(Irp), nxt;
    int write = cur->MajorFunction == IRP_MJ_WRITE;
    gpt_u64 off = (gpt_u64)cur->Parameters.ReadWrite.ByteOffset.QuadPart, boff;
    ULONG len = cur->Parameters.ReadWrite.Length; gpt_u32 d; PVOID buf; int rc;
    if (cur->Parameters.ReadWrite.ByteOffset.QuadPart < 0 || off + len > rt->Map.Size) {
        if (off >= rt->Map.Size) return Complete(Irp, STATUS_END_OF_FILE, 0);
        return Complete(Irp, STATUS_INVALID_PARAMETER, 0);
    }
    if (len == 0) return Complete(Irp, STATUS_SUCCESS, 0);
    if ((off | len) & 511) return Complete(Irp, STATUS_INVALID_PARAMETER, 0);
    if (write && rt->ReadOnly) return Complete(Irp, STATUS_MEDIA_WRITE_PROTECTED, 0);
    /* one contiguous range on one disk: pass the request on */
    if (LdmMapContig(&rt->Map, rt->Present, write, off, len, &d, &boff) && rt->Dev[d]) {
        nxt = IoGetNextIrpStackLocation(Irp);
        CopyStackDown(nxt, cur, rt->Dev[d]);
        nxt->Parameters.ReadWrite.ByteOffset.QuadPart = (LONGLONG)boff;
        return IoCallDriver(rt->Dev[d], Irp);
    }
    /* stripes, mirror writes, RAID-5: done here, one request at a time */
    buf = Irp->MdlAddress ? MmGetSystemAddress(Irp->MdlAddress) : NULL;
    if (!buf) return Complete(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);
    KeWaitForSingleObject(&g_LdmIoLock, Executive, KernelMode, FALSE, NULL);
    rc = LdmVolIo(&rt->Map, rt->Present, LdmVolDiskIo, rt, write, off, len, buf, g_scratch);
    KeSetEvent(&g_LdmIoLock, 0, FALSE);
    return Complete(Irp, rc ? (NTSTATUS)0xC0000185L /* STATUS_IO_DEVICE_ERROR */ : STATUS_SUCCESS, rc ? 0 : len);
}

static NTSTATUS LdmSetType(LDMRT *rt, UCHAR type);

static NTSTATUS LdmDeviceControl(PPART_EXT ext, PIRP Irp, ULONG code, PVOID sys, ULONG inlen, ULONG outlen)
{
    LDMRT *rt = ext->Ldm; PIO_STACK_LOCATION cur = IoGetCurrentIrpStackLocation(Irp), nxt;
    switch (code) {
    case IOCTL_DISK_GET_PARTITION_INFO: {
        PPARTITION_INFORMATION pi = (PPARTITION_INFORMATION)sys;
        if (outlen < sizeof(PARTITION_INFORMATION)) return Complete(Irp, STATUS_BUFFER_TOO_SMALL, 0);
        pi->StartingOffset.QuadPart = (LONGLONG)rt->FirstOff; pi->PartitionLength = ext->PartitionLength;
        pi->HiddenSectors = (ULONG)(rt->FirstOff >> 9); pi->PartitionNumber = ext->PartitionNumber;
        pi->PartitionType = (ext->PartitionType == 0x0E) ? 0x06 : ext->PartitionType;
        pi->BootIndicator = FALSE; pi->RecognizedPartition = TRUE; pi->RewritePartition = FALSE;
        return Complete(Irp, STATUS_SUCCESS, sizeof(PARTITION_INFORMATION));
    }
    case IOCTL_DISK_IS_WRITABLE:
        return Complete(Irp, rt->ReadOnly ? STATUS_MEDIA_WRITE_PROTECTED : STATUS_SUCCESS, 0);
    case IOCTL_DISK_SET_PARTITION_INFO: {
        NTSTATUS st;
        if (inlen < 1) return Complete(Irp, STATUS_INVALID_PARAMETER, 0);
        st = LdmSetType(rt, *(UCHAR *)sys);
        if (NT_SUCCESS(st)) ext->PartitionType = *(UCHAR *)sys;
        return Complete(Irp, st, 0);
    }
    case IOCTL_DISK_VERIFY: return Complete(Irp, STATUS_SUCCESS, 0);
    case IOCTL_DISK_SET_DRIVE_LAYOUT:
    case IOCTL_DISK_GET_DRIVE_LAYOUT: return Complete(Irp, STATUS_NOT_SUPPORTED, 0);
    default:
        /* geometry, media checks: answered by the first member disk */
        nxt = IoGetNextIrpStackLocation(Irp);
        CopyStackDown(nxt, cur, ext->LowerDevice);
        return IoCallDriver(ext->LowerDevice, Irp);
    }
}

/* ---- reading volumes outside the I/O path (file system detection) ---- */
typedef struct _SCANCTX { LDMGRP *g; LDM_VMAP *m; } SCANCTX;

static int ScanDiskIo(void *ctx, gpt_u32 d, int write, gpt_u64 off, gpt_u32 len, void *buf)
{
    SCANCTX *c = (SCANCTX *)ctx; PFILE_OBJECT f; PDEVICE_OBJECT dev; LARGE_INTEGER o; NTSTATUS st;
    if (write || d >= LDM_MAX_DISKS || !c->g->Present[d]) return 1;
    if (!NT_SUCCESS(OpenPartition0(c->g->NtDisk[d], &f, &dev))) return 1;
    o.QuadPart = (LONGLONG)off;
    st = SyncTransfer(IRP_MJ_READ, dev, buf, len, &o);
    ObDereferenceObject(f);
    return NT_SUCCESS(st) ? 0 : 1;
}

static int ScanVolRead(void *ctx, gpt_u64 lba, gpt_u32 count, void *buf)
{
    SCANCTX *c = (SCANCTX *)ctx;
    return LdmVolIo(c->m, c->g->Present, ScanDiskIo, c, 0, lba * 512, count * 512, buf, g_scanScratch + 4096) ? 1 : 0;
}

/* file system kind (GPT_FS_*) of volume vi, 0 when unreadable */
static UCHAR LdmVolFs(LDMGRP *g, int vi, LDM_VMAP *m)
{
    SCANCTX c; int rec = 0; gpt_u8 kind = 0;
    if (!g_scanScratch || LdmMapState(m, g->Present) == LDM_FAILED) return 0;
    c.g = g; c.m = m;
    KeWaitForSingleObject(&g_LdmScanBufLock, Executive, KernelMode, FALSE, NULL);
    GptDetectFs(ScanVolRead, &c, 512, 0, m->Size / 512, g_scanScratch, &rec, &kind);
    KeSetEvent(&g_LdmScanBufLock, 0, FALSE);
    return kind;
}

static int LdmMap(LDMGRP *g, int vi, LDM_VMAP *m)
{
    MemZero(m, sizeof(*m));
    return LdmBuildMap(g->Db, vi, g->PubStart, g->Bps, m);
}

static void LdmFreeGroup(LDMGRP *g)
{
    int i;
    if (!g) return;
    for (i = 0; i < LDM_MAX_DISKS; i++) if (g->Hdr[i]) ExFreePool(g->Hdr[i]);
    if (g->Db) { LdmFreeDb(g->Db); ExFreePool(g->Db); }
    ExFreePool(g);
}

/* Read every dynamic disk and assemble the disk groups. */
#define SCAN_MAX MGPT_MAXDISK
typedef struct { ULONG nt; int grp; gpt_u64 committed, total; LDM_HDR *hdr; int ok; } SCANMEM;

static int LdmScan(LDMGRP **out)
{
    GPT_DISK *disk; LDM_DB *tmp; SCANMEM mem[SCAN_MAX]; int nm = 0, n = 0, i, k; ULONG d;
    disk = (GPT_DISK *)ExAllocatePool(NonPagedPool, sizeof(GPT_DISK));
    tmp = (LDM_DB *)ExAllocatePool(NonPagedPool, sizeof(LDM_DB));
    if (!disk || !tmp) { if (disk) ExFreePool(disk); if (tmp) ExFreePool(tmp); return 0; }
    for (d = 0; d < MGPT_MAXDISK; d++) {
        PFILE_OBJECT f; PDEVICE_OBJECT p0; ENUMCTX ctx; ULONG bps; LDM_HDR *h; int gi = -1;
        bps = ParseDisk(d, disk);
        if (!bps) continue;
        for (i = 0; i < (int)disk->Count; i++)
            if (disk->Part[i].MbrType == GPT_MBRTYPE_LDM) break;
        if (i == (int)disk->Count) continue;
        if (!NT_SUCCESS(OpenPartition0(d, &f, &p0))) continue;
        h = (LDM_HDR *)ExAllocatePool(NonPagedPool, sizeof(LDM_HDR));
        ctx.Part0 = p0; ctx.BytesPerSector = bps; ctx.Limit = 0;
        if (!h || LdmReadHeader(EnumRead, &ctx, bps, disk->DiskSectors, disk, h) != GPT_OK) {
            if (h) ExFreePool(h);
            ObDereferenceObject(f); continue;
        }
        D("ldm disk "); DH(d); D(" group "); D(h->DgName); D("\n");
        for (k = 0; k < n; k++) if (StrSame(out[k]->DgGuid, h->DgGuid)) gi = k;
        if (gi < 0 && n < LDM_GROUPS) {
            LDMGRP *g = (LDMGRP *)ExAllocatePool(NonPagedPool, sizeof(LDMGRP));
            if (g) { MemZero(g, sizeof(*g)); StrCopy(g->DgGuid, h->DgGuid, 40); gi = n; out[n++] = g; }
        }
        if (gi < 0 || nm >= SCAN_MAX) { ExFreePool(h); ObDereferenceObject(f); continue; }
        MemZero(tmp, sizeof(*tmp));
        mem[nm].nt = d; mem[nm].grp = gi; mem[nm].hdr = h; mem[nm].committed = 0; mem[nm].ok = 0;
        mem[nm].total = disk->DiskSectors;
        k = LdmReadDb(EnumRead, &ctx, h, tmp);
        D("  db rc="); DH(k); D(" committed="); DH((ULONG)tmp->Committed); D("\n");
        if (k == GPT_OK) {
            LDMGRP *g = out[gi];
            mem[nm].committed = tmp->Committed; mem[nm].ok = 1;
            if (!g->Db) g->Db = (LDM_DB *)ExAllocatePool(NonPagedPool, sizeof(LDM_DB));
            else if (tmp->Committed > g->Db->Committed) LdmFreeDb(g->Db);
            else { LdmFreeDb(tmp); goto next; }
            if (g->Db) memcpy(g->Db, tmp, sizeof(LDM_DB)); else LdmFreeDb(tmp);
        }
    next:
        nm++;
        ObDereferenceObject(f);
    }
    ExFreePool(disk); ExFreePool(tmp);

    /* members of each group, by disk GUID */
    for (k = 0; k < n; k++) {
        LDMGRP *g = out[k];
        if (!g->Db) continue;
        for (i = 0; i < (int)g->Db->NDisks; i++) {
            char gs[40]; int j;
            g->NtDisk[i] = 0xFFFFFFFFUL;
            LdmGuidToString(g->Db->Disk[i].Guid, gs);
            for (j = 0; j < nm; j++)
                if (mem[j].grp == k && mem[j].hdr && StrSame(mem[j].hdr->DiskGuid, gs)) {
                    g->NtDisk[i] = mem[j].nt; g->Present[i] = 1;
                    g->Stale[i] = (!mem[j].ok || mem[j].committed < g->Db->Committed) ? 1 : 0;
                    g->IsGpt[i] = (gpt_u8)mem[j].hdr->IsGpt; g->Bps[i] = mem[j].hdr->SectorSize;
                    g->PubStart[i] = mem[j].hdr->PubStart; g->PubSize[i] = mem[j].hdr->PubSize;
                    g->Hdr[i] = mem[j].hdr; mem[j].hdr = NULL; g->Total[i] = mem[j].total;
                    break;
                }
            if (!g->Bps[i]) g->Bps[i] = 512;
        }
        g->Native = 0;
#if defined(_WIN64)
        g->Native = 1;
#else
        if (OsHasLdm()) for (i = 0; i < (int)g->Db->NDisks; i++) if (g->Present[i] && !g->IsGpt[i]) g->Native = 1;
#endif
    }
    for (i = 0; i < nm; i++) if (mem[i].hdr) ExFreePool(mem[i].hdr);
    /* drop groups whose database could not be read */
    for (k = 0, i = 0; k < n; k++) { if (out[k]->Db) out[i++] = out[k]; else LdmFreeGroup(out[k]); }
    return i;
}

static int LdmFindVol(LDMGRP *g, const gpt_u8 *guid)
{
    int v;
    for (v = 0; v < (int)g->Db->NVols; v++) if (MemSame(g->Db->Vol[v].Guid, guid, 16)) return v;
    return -1;
}

/* letter of a free drive letter link, else 0 */
static UCHAR LinkLetter(UCHAR c, const char *devName)
{
    char link[32]; UNICODE_STRING ul, ut; NTSTATUS st;
    sprintf(link, "\\DosDevices\\%c:", c);
    if (!NT_SUCCESS(MakeUnicode(link, &ul))) return 0;
    if (!NT_SUCCESS(MakeUnicode(devName, &ut))) { RtlFreeUnicodeString(&ul); return 0; }
    st = IoCreateSymbolicLink(&ul, &ut);
    RtlFreeUnicodeString(&ul); RtlFreeUnicodeString(&ut);
    return NT_SUCCESS(st) ? c : 0;
}

/* Publish volume vi of group g. */
static void LdmPublish(LDMGRP *g, int vi, LDM_VMAP *m)
{
    char name[40]; UNICODE_STRING un; PDEVICE_OBJECT dev; PPART_EXT ext; LDMRT *rt; NTSTATUS st;
    LDM_VOL *v = &g->Db->Vol[vi]; int i, state; CCHAR stack = 1; ULONG align = 0; PDEVICE_OBJECT first = NULL;
    rt = (LDMRT *)ExAllocatePool(NonPagedPool, sizeof(LDMRT));
    if (!rt) return;
    MemZero(rt, sizeof(*rt));
    memcpy(&rt->Map, m, sizeof(LDM_VMAP));
    memcpy(rt->Guid, v->Guid, 16); StrCopy(rt->DgGuid, g->DgGuid, 40);
    for (i = 0; i < LDM_MAX_DISKS; i++) {
        rt->NtDisk[i] = g->NtDisk[i];
        if (i >= (int)g->Db->NDisks || !g->Present[i] || !LdmMapUsesDisk(m, (gpt_u32)i)) continue;
        if (!NT_SUCCESS(OpenPartition0(g->NtDisk[i], &rt->File[i], &rt->Dev[i]))) { rt->File[i] = NULL; rt->Dev[i] = NULL; continue; }
        rt->Present[i] = 1;
        if (rt->Dev[i]->StackSize >= stack) stack = (CCHAR)(rt->Dev[i]->StackSize + 1);
        if (rt->Dev[i]->AlignmentRequirement > align) align = rt->Dev[i]->AlignmentRequirement;
        if (!first) first = rt->Dev[i];
    }
    state = LdmMapState(&rt->Map, rt->Present);
    if (state == LDM_FAILED || !first) { LdmFreeRt(rt); return; }
    rt->ReadOnly = (state != LDM_HEALTHY);
    rt->FirstOff = m->Plex[0].N ? m->Plex[0].X[0].ByteStart : 0;
    sprintf(name, "\\Device\\MbrGptNtDm%lu", (unsigned long)++g_dmSeq);
    if (!NT_SUCCESS(MakeUnicode(name, &un))) { LdmFreeRt(rt); return; }
    st = IoCreateDevice(g_Driver, sizeof(PART_EXT), &un, FILE_DEVICE_DISK, 0, FALSE, &dev);
    RtlFreeUnicodeString(&un);
    if (!NT_SUCCESS(st)) { LdmFreeRt(rt); return; }
    ext = (PPART_EXT)dev->DeviceExtension;
    MemZero(ext, sizeof(*ext));
    ext->Kind = KIND_PART; ext->Self = dev; ext->LowerDevice = first; ext->LowerFile = NULL;
    ext->StartingOffset.QuadPart = 0; ext->PartitionLength.QuadPart = (LONGLONG)m->Size;
    ext->BytesPerSector = 512; ext->DiskNumber = LDM_DISKNUM; ext->PartitionNumber = g_dmSeq;
    ext->PartitionType = v->PartType; ext->Recognized = 1; ext->Ldm = rt;
    StrCopy(ext->DevName, name, sizeof(ext->DevName));
    dev->Flags |= DO_DIRECT_IO;
    dev->StackSize = stack;
    dev->AlignmentRequirement = align;
    dev->Flags &= ~DO_DEVICE_INITIALIZING;
    /* the letter the database asks for when it is free, else the first free one */
    if (v->Hint[0] >= 'C' && v->Hint[0] <= 'Z') ext->DriveLetter = LinkLetter((UCHAR)v->Hint[0], name);
    else if (v->Hint[0] >= 'c' && v->Hint[0] <= 'z') ext->DriveLetter = LinkLetter((UCHAR)(v->Hint[0] - 32), name);
    if (!ext->DriveLetter) ext->DriveLetter = AssignDriveLetter(name);
    LockList(); ext->Next = g_PartList; g_PartList = ext; UnlockList();
    D("  "); D(name); D(" "); D(v->Name); D(" letter="); DH(ext->DriveLetter); D("\n");
}

/* Dismount the stale volumes of DiskNumber that are mounted but idle, and
   delete those nothing references any more (see SyncDisk). */
static void RetireStale(ULONG DiskNumber)
{
    PPART_EXT p, *pp, drop = NULL;
    for (p = g_PartList; p; p = p->Next)
        if (p->DiskNumber == DiskNumber && p->Stale && !p->Gone && DeviceInUse(p->Self)) {
            char name[48]; int t;
            VolumeName(p, name);
            if (DismountIfIdle(name)) {
                p->Gone = 1;
                UnlinkLetter(p);
                for (t = 0; t < 20 && DeviceInUse(p->Self); t++) { LARGE_INTEGER dl; dl.QuadPart = -500000; KeDelayExecutionThread(KernelMode, FALSE, &dl); }
            }
        }
    LockList();
    pp = &g_PartList;
    while (*pp) {
        p = *pp;
        if (p->DiskNumber == DiskNumber && p->Stale && !DeviceInUse(p->Self)) { *pp = p->Next; p->Next = drop; drop = p; }
        else pp = &p->Next;
    }
    UnlockList();
    while (drop) { p = drop; drop = drop->Next; RemoveVolume(p); }
}

/* Rebuild the disk groups and bring the dynamic volumes in line with them.
   Runs in a system thread. */
static void LdmSync(void)
{
    LDMGRP *ng[LDM_GROUPS], *old[LDM_GROUPS]; int nn, no, k, v; PPART_EXT p;
    LDM_VMAP *m = (LDM_VMAP *)ExAllocatePool(NonPagedPool, sizeof(LDM_VMAP));
    if (!m) return;
    KeWaitForSingleObject(&g_SyncLock, Executive, KernelMode, FALSE, NULL);
    nn = LdmScan(ng);
    if (nn) LdmBuffers();

    /* 1. a volume that is gone, or whose layout or members changed, is stale */
    for (p = g_PartList; p; p = p->Next) {
        int keep = 0;
        if (!p->Ldm || p->Gone) { if (p->Ldm) p->Stale = 1; continue; }
        for (k = 0; k < nn && !keep; k++) {
            LDMGRP *g = ng[k]; LDMRT *rt = p->Ldm; int i, st;
            if (g->Native || !StrSame(g->DgGuid, rt->DgGuid)) continue;
            v = LdmFindVol(g, rt->Guid);
            if (v < 0 || LdmMap(g, v, m) != GPT_OK) break;
            st = LdmMapState(m, g->Present);
            if (st == LDM_FAILED || !MemSame(m, &rt->Map, sizeof(LDM_VMAP))) break;
            if (rt->ReadOnly != (st != LDM_HEALTHY)) break;
            keep = 1;
            for (i = 0; i < (int)g->Db->NDisks && i < LDM_MAX_DISKS; i++)
                if (LdmMapUsesDisk(m, (gpt_u32)i) && (rt->Present[i] != g->Present[i] || (g->Present[i] && rt->NtDisk[i] != g->NtDisk[i]))) keep = 0;
            if (keep) p->PartitionType = g->Db->Vol[v].PartType;
        }
        p->Stale = (UCHAR)(keep ? 0 : 1);
    }
    RetireStale(LDM_DISKNUM);

    /* 2. publish the volumes that have no device yet */
    for (k = 0; k < nn; k++) {
        LDMGRP *g = ng[k];
        if (g->Native) continue;
        for (v = 0; v < (int)g->Db->NVols; v++) {
            int have = 0;
            LockList();
            for (p = g_PartList; p; p = p->Next)
                if (p->Ldm && !p->Stale && StrSame(p->Ldm->DgGuid, g->DgGuid) && MemSame(p->Ldm->Guid, g->Db->Vol[v].Guid, 16)) have = 1;
            UnlockList();
            if (have || LdmMap(g, v, m) != GPT_OK || LdmMapState(m, g->Present) == LDM_FAILED) continue;
            LdmPublish(g, v, m);
        }
    }

    /* 3. the new groups replace the old ones */
    KeWaitForSingleObject(&g_LdmLock, Executive, KernelMode, FALSE, NULL);
    no = g_ngrp;
    for (k = 0; k < no; k++) old[k] = g_grp[k];
    for (k = 0; k < nn; k++) g_grp[k] = ng[k];
    g_ngrp = nn;
    KeSetEvent(&g_LdmLock, 0, FALSE);
    for (k = 0; k < no; k++) LdmFreeGroup(old[k]);
    KeSetEvent(&g_SyncLock, 0, FALSE);
    ExFreePool(m);
}


/* ---- disk arrival, removal and swaps ---------------------------------
   A disk can appear after start-up (a USB disk plugged in, media put in a
   card reader or a USB disk slot) and disappear again. A system thread
   checks every disk every 2 s - cheaply: open Partition0 and ask for the
   geometry, which the class driver answers from memory for fixed disks.
   Removable media is also identified by its disk signature / GPT disk
   GUID (2 sectors read), so swapping one stick for another is noticed.
   A new disk gets its volumes published (SyncDisk); a disk that is gone
   or swapped loses all of its volumes: dismounted when idle, drive letter
   removed, I/O refused, the device deleted once nothing references it. */


static int ProbeDisk(ULONG d, DISKID *id)
{
    PFILE_OBJECT file; PDEVICE_OBJECT part0; DISK_GEOMETRY geo; NTSTATUS st; ULONG bps;
    id->present = 0; id->bps = 0; id->removable = 0; id->ident = 0; id->total = 0;
    if (!NT_SUCCESS(OpenPartition0(d, &file, &part0))) return 0;
    st = SyncIoctl(IOCTL_DISK_GET_DRIVE_GEOMETRY, part0, NULL, 0, &geo, sizeof(geo));
    bps = geo.BytesPerSector;
    if (!NT_SUCCESS(st) || (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096)) { ObDereferenceObject(file); return 0; }
    id->bps = bps;
    id->total = (gpt_u64)geo.Cylinders.QuadPart * (gpt_u64)geo.TracksPerCylinder * (gpt_u64)geo.SectorsPerTrack;
    id->removable = (geo.MediaType == RemovableMedia) ? 1 : 0;
    if (id->removable) {
        UCHAR *buf = (UCHAR *)ExAllocatePool(NonPagedPool, 2 * bps);
        LARGE_INTEGER off; UCHAR key[20]; ULONG i;
        if (!buf) { ObDereferenceObject(file); return 0; }
        off.QuadPart = 0;
        st = SyncTransfer(IRP_MJ_READ, part0, buf, 2 * bps, &off);
        if (!NT_SUCCESS(st)) { ExFreePool(buf); ObDereferenceObject(file); return 0; }
        for (i = 0; i < 20; i++) key[i] = 0;
        for (i = 0; i < 4; i++) key[i] = buf[0x1B8 + i];                       /* MBR disk signature */
        if (buf[bps] == 'E' && buf[bps + 1] == 'F' && buf[bps + 2] == 'I' && buf[bps + 3] == ' ')
            for (i = 0; i < 16; i++) key[4 + i] = buf[bps + 56 + i];            /* GPT disk GUID */
        id->ident = GptCrc32(key, 20);
        ExFreePool(buf);
    }
    ObDereferenceObject(file);
    id->present = 1;
    return 1;
}

static int SameDisk(const DISKID *a, const DISKID *b)
{ return a->bps == b->bps && a->total == b->total && a->removable == b->removable && a->ident == b->ident; }

/* The disk behind DiskNumber is gone (or is another disk now): retire every
   volume published for it. Returns 1 if a drive letter was removed. */
static int DropDisk(ULONG DiskNumber)
{
    PPART_EXT p, *pp, drop = NULL; int changed = 0; char name[48];
    KeWaitForSingleObject(&g_SyncLock, Executive, KernelMode, FALSE, NULL);
    for (p = g_PartList; p; p = p->Next) {
        if (p->DiskNumber != DiskNumber || p->Gone) continue;
        if (DeviceInUse(p->Self)) { VolumeName(p, name); DismountIfIdle(name); }
        p->Stale = 1; p->Gone = 1;
        if (p->DriveLetter) { UnlinkLetter(p); changed = 1; }
    }
    LockList();
    pp = &g_PartList;
    while (*pp) {
        p = *pp;
        if (p->DiskNumber == DiskNumber && p->Gone && !DeviceInUse(p->Self)) { *pp = p->Next; p->Next = drop; drop = p; }
        else pp = &p->Next;
    }
    UnlockList();
    while (drop) { p = drop; drop = drop->Next; RemoveVolume(p); }
    KeSetEvent(&g_SyncLock, 0, FALSE);
    return changed;
}

/* Volumes kept because they were in use when their partition (or disk)
   went away: dismount each one as soon as its last file is closed, remove
   its letter, and delete the device once nothing references it.
   Returns 1 if a drive letter was removed. */
static int SweepStale(void)
{
    PPART_EXT p, *pp, drop = NULL; int changed = 0; char name[48];
    KeWaitForSingleObject(&g_SyncLock, Executive, KernelMode, FALSE, NULL);
    for (p = g_PartList; p; p = p->Next) {
        VPB_MIN *vpb;
        if (!p->Stale || !DeviceInUse(p->Self)) continue;
        vpb = (VPB_MIN *)p->Self->Vpb;
        if (!vpb || !(vpb->Flags & VPB_MOUNTED)) continue;     /* only referenced: wait */
        VolumeName(p, name);
        if (DismountIfIdle(name)) {
            p->Gone = 1;
            if (p->DriveLetter) { UnlinkLetter(p); changed = 1; }
        }
    }
    LockList();
    pp = &g_PartList;
    while (*pp) {
        p = *pp;
        if (p->Stale && !DeviceInUse(p->Self)) { *pp = p->Next; p->Next = drop; drop = p; }
        else pp = &p->Next;
    }
    UnlockList();
    while (drop) { p = drop; drop = drop->Next; if (p->DriveLetter) changed = 1; RemoveVolume(p); }
    KeSetEvent(&g_SyncLock, 0, FALSE);
    return changed;
}

/* One pass over all disks. Runs in the disk-check system thread. */
static void CheckDisks(void)
{
    ULONG d, pass; DISKID id; int changed = 0;
    KeWaitForSingleObject(&g_ScanLock, Executive, KernelMode, FALSE, NULL);
    pass = ++g_PassStarted;
    for (d = 0; d < MGPT_MAXDISK; d++) {
        DISKID *k = &g_known[d];
        ProbeDisk(d, &id);
        if (!id.present && !k->present) continue;
        if (id.present && k->present && SameDisk(&id, k)) continue;
        D("disk "); DH(d); D(id.present ? (k->present ? " swapped\n" : " arrived\n") : " removed\n");
        if (k->present) DropDisk(d);
        *k = id;
        if (id.present) SyncDisk(d);
        changed = 1;
    }
    if (g_LdmDirty) changed = 1;
    if (changed) { g_LdmDirty = 0; LdmSync(); }
    if (SweepStale()) changed = 1;
    if (changed) g_Gen++;
    g_PassDone = pass;
    KeSetEvent(&g_ScanLock, 0, FALSE);
}

/* IOCTL_MGPT_RESCAN: have the disk-check thread run a pass that starts
   after this request, and wait for it (at most 15 s). */
static void RescanNow(void)
{
    ULONG want = g_PassStarted + 1; int t;
    if (!g_PollRunning) return;
    KeSetEvent(&g_ScanReq, 0, FALSE);
    for (t = 0; t < 300 && (LONG)(g_PassDone - want) < 0; t++) {
        LARGE_INTEGER dl; dl.QuadPart = -500000;      /* 50 ms */
        KeDelayExecutionThread(KernelMode, FALSE, &dl);
    }
}

/* Run SyncDisk in a system worker thread and wait for it. A control
   request runs in the caller's thread (previous mode = user), and on NT 3.1
   IoCreateDevice then rejects the kernel-mode device name with an access
   violation; a system worker thread has a kernel previous mode. */
typedef struct _SYNCREQ { WORK_QUEUE_ITEM Item; ULONG Disk; KEVENT Done; } SYNCREQ;

static VOID NTAPI SyncWorker(PVOID ctx)
{
    SYNCREQ *r = (SYNCREQ *)ctx;
    if (r->Disk != 0xFFFFFFFFUL) SyncDisk(r->Disk);
    LdmSync();
    KeSetEvent(&r->Done, 0, FALSE);
}

static void SyncDiskInSystem(ULONG DiskNumber)
{
    SYNCREQ *r = (SYNCREQ *)ExAllocatePool(NonPagedPool, sizeof(SYNCREQ));
    if (!r) return;
    r->Disk = DiskNumber;
    KeInitializeEvent(&r->Done, NotificationEvent, FALSE);
    r->Item.List.Flink = NULL; r->Item.List.Blink = NULL;
    r->Item.WorkerRoutine = SyncWorker; r->Item.Parameter = r;
    ExQueueWorkItem(&r->Item, DelayedWorkQueue);
    KeWaitForSingleObject(&r->Done, Executive, KernelMode, FALSE, NULL);
    ExFreePool(r);
}

static NTSTATUS WriteAndReenumLocked(ULONG DiskNumber, GPT_DISK *disk);
static void TellDiskDriver(PDEVICE_OBJECT part0);
/* Write a table and update the disk's volumes. Holds the disk-check lock,
   so the check thread does not take a rewritten disk (a new GPT disk GUID
   after a conversion, say) for a swapped one. */
static NTSTATUS WriteAndReenum(ULONG DiskNumber, GPT_DISK *disk)
{
    NTSTATUS st;
    KeWaitForSingleObject(&g_ScanLock, Executive, KernelMode, FALSE, NULL);
    st = WriteAndReenumLocked(DiskNumber, disk);
    if (DiskNumber < MGPT_MAXDISK) ProbeDisk(DiskNumber, &g_known[DiskNumber]);
    g_Gen++;
    KeSetEvent(&g_ScanLock, 0, FALSE);
    return st;
}

static NTSTATUS WriteAndReenumLocked(ULONG DiskNumber, GPT_DISK *disk)
{
    PFILE_OBJECT file; PDEVICE_OBJECT part0; ENUMCTX ctx; int rc;
    if (!NT_SUCCESS(OpenPartition0(DiskNumber, &file, &part0))) return STATUS_DEVICE_NOT_READY;
    if (!disk->Heads) {
        /* a freshly initialized table: take the CHS geometry from the disk */
        DISK_GEOMETRY geo;
        if (NT_SUCCESS(SyncIoctl(IOCTL_DISK_GET_DRIVE_GEOMETRY, part0, NULL, 0, &geo, sizeof(geo)))) {
            disk->Heads = geo.TracksPerCylinder; disk->SectorsPerTrack = geo.SectorsPerTrack;
        }
    }
    ctx.Part0 = part0; ctx.BytesPerSector = disk->SectorSize; ctx.Limit = 0;
    rc = GptWriteDisk(EnumRead, EnumWrite, &ctx, disk);
    TellDiskDriver(part0);
    ObDereferenceObject(file);
    if (rc != GPT_OK) return STATUS_UNSUCCESSFUL;
    SyncDiskInSystem(DiskNumber);
    return STATUS_SUCCESS;
}

/* Windows 2000 and later keep the partition table in memory: have the
   disk driver read it again after it was rewritten */
static int OsHasLdm(void);
static void TellDiskDriver(PDEVICE_OBJECT part0)
{
    if (OsHasLdm()) SyncIoctl(IOCTL_DISK_UPDATE_PROPERTIES, part0, NULL, 0, NULL, 0);
}

static GPT_DISK *LoadDisk(ULONG dn, ULONG *bpsOut)
{
    GPT_DISK *disk = (GPT_DISK *)ExAllocatePool(NonPagedPool, sizeof(GPT_DISK));
    ULONG bps;
    if (!disk) return NULL;
    bps = ParseDisk(dn, disk);
    if (bps == 0) { ExFreePool(disk); return NULL; }
    if (bpsOut) *bpsOut = bps;
    return disk;
}

/* Write a new type byte for the partition behind one of our volumes
   (IOCTL_DISK_SET_PARTITION_INFO). Only the table changes; the volume
   keeps its offset and length, so it is not re-enumerated. */
static NTSTATUS SetVolumeType(PPART_EXT ext, UCHAR type)
{
    GPT_DISK *disk; ULONG bps, i; PFILE_OBJECT file; PDEVICE_OBJECT part0; ENUMCTX ctx; int rc;
    disk = LoadDisk(ext->DiskNumber, &bps);
    if (!disk) return STATUS_DEVICE_NOT_READY;
    for (i = 0; i < disk->Count; i++)
        if ((gpt_u64)disk->Part[i].StartLba * bps == (gpt_u64)ext->StartingOffset.QuadPart && !IsExtType(disk->Part[i].MbrType))
            break;
    if (i == disk->Count || IsExtType(type) || type == 0) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    /* FORMAT asks for the plain FAT type; give the right one of its family
       for this partition's size and position (this driver mounts all) */
    if (!disk->IsGpt && GptTypeFamily(type))
        type = GptPickType(disk, GptTypeFamily(type), disk->Part[i].StartLba, disk->Part[i].SectorCount);
    GptSetType(disk, i, type);
    if (!NT_SUCCESS(OpenPartition0(ext->DiskNumber, &file, &part0))) { ExFreePool(disk); return STATUS_DEVICE_NOT_READY; }
    ctx.Part0 = part0; ctx.BytesPerSector = bps; ctx.Limit = 0;
    rc = GptWriteDisk(EnumRead, EnumWrite, &ctx, disk);
    TellDiskDriver(part0);
    ObDereferenceObject(file);
    ExFreePool(disk);
    if (rc != GPT_OK) return STATUS_UNSUCCESSFUL;
    ext->PartitionType = type;
    return STATUS_SUCCESS;
}

static NTSTATUS DoSetLayout(PVOID sys, ULONG inlen)
{
    MGPT_LAYOUT *lay = (MGPT_LAYOUT *)sys; GPT_DISK *disk; PFILE_OBJECT file; PDEVICE_OBJECT part0;
    ULONG bps, i, pass; gpt_u64 total; DISK_GEOMETRY geo; NTSTATUS st;
    if (inlen < sizeof(MGPT_LAYOUT)) return STATUS_INVALID_PARAMETER;
    if (lay->Count > MGPT_MAX_PARTS) return STATUS_INVALID_PARAMETER;
    {
        GPT_DISK *cur = LoadDisk(lay->DiskNumber, NULL); int dyn;
        if (!cur) return STATUS_DEVICE_NOT_READY;
        dyn = IsDynamicTable(cur); ExFreePool(cur);
        if (dyn) return STATUS_NOT_SUPPORTED;
    }
    if (!NT_SUCCESS(OpenPartition0(lay->DiskNumber, &file, &part0))) return STATUS_DEVICE_NOT_READY;
    st = SyncIoctl(IOCTL_DISK_GET_DRIVE_GEOMETRY, part0, NULL, 0, &geo, sizeof(geo));
    if (!NT_SUCCESS(st)) { ObDereferenceObject(file); return st; }
    bps = geo.BytesPerSector;
    total = (gpt_u64)geo.Cylinders.QuadPart * (gpt_u64)geo.TracksPerCylinder * (gpt_u64)geo.SectorsPerTrack;
    ObDereferenceObject(file);
    disk = (GPT_DISK *)ExAllocatePool(NonPagedPool, sizeof(GPT_DISK));
    if (!disk) return STATUS_INSUFFICIENT_RESOURCES;
    GptSeed((gpt_u32)total ^ 0x9E3779B9u);
    GptInitDisk(disk, lay->IsGpt ? GPT_SCHEME_GPT : GPT_SCHEME_MBR, bps, total);
    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < lay->Count; i++) {
            int isLog = (lay->Part[i].Flags & MGPT_F_LOGICAL) ? 1 : 0;
            if ((pass == 0 && isLog) || (pass == 1 && !isLog)) continue;
            GptAddPartition(disk, lay->Part[i].StartLba, lay->Part[i].SectorCount,
                            (gpt_u8)lay->Part[i].MbrType, (gpt_u8)((lay->Part[i].Flags & MGPT_F_BOOT) ? 1 : 0), NULL);
        }
    st = WriteAndReenum(lay->DiskNumber, disk);
    ExFreePool(disk);
    return st;
}

static NTSTATUS DoCreate(PVOID sys, ULONG inlen)
{
    MGPT_CREATE *c = (MGPT_CREATE *)sys; GPT_DISK *disk; ULONG bps; NTSTATUS st;
    if (inlen < sizeof(MGPT_CREATE)) return STATUS_INVALID_PARAMETER;
    disk = LoadDisk(c->DiskNumber, &bps);
    if (!disk) return STATUS_DEVICE_NOT_READY;
    if (IsDynamicTable(disk)) { ExFreePool(disk); return STATUS_NOT_SUPPORTED; }   /* see IOCTL_MGPT_LDM* */
    GptSeed((gpt_u32)disk->DiskSectors ^ 0x1234ABCDu);
    if (c->MbrType >= MGPT_TYPE_FAT16) {
        /* a family: place the partition, then pick the exact type byte from
           where it landed and how big it is */
        ULONG fam = c->MbrType; gpt_u32 idx;
        if (fam > MGPT_TYPE_EXTENDED || (disk->IsGpt && fam == MGPT_TYPE_EXTENDED)) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
        if (GptAddPartition(disk, c->StartLba, c->SectorCount,
                            (gpt_u8)(fam == MGPT_TYPE_EXTENDED ? GPT_MBRTYPE_EXTENDED : GPT_MBRTYPE_FAT16),
                            (gpt_u8)(c->Boot ? 1 : 0), &idx) != GPT_OK)
            { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
        if (disk->IsGpt)
            GptSetType(disk, idx, (gpt_u8)(fam == MGPT_TYPE_FAT32 ? GPT_MBRTYPE_FAT32L : GPT_MBRTYPE_FAT16));
        else
            GptSetType(disk, idx, GptPickType(disk, fam, disk->Part[idx].StartLba, disk->Part[idx].SectorCount));
    } else {
        if (c->MbrType == 0) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
        if (GptAddPartition(disk, c->StartLba, c->SectorCount, (gpt_u8)c->MbrType, (gpt_u8)(c->Boot ? 1 : 0), NULL) != GPT_OK)
            { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    }
    st = WriteAndReenum(c->DiskNumber, disk);
    ExFreePool(disk);
    return st;
}

static NTSTATUS DoDelete(PVOID sys, ULONG inlen)
{
    MGPT_DELETE *dd = (MGPT_DELETE *)sys; GPT_DISK *disk; ULONG bps; NTSTATUS st;
    if (inlen < sizeof(MGPT_DELETE)) return STATUS_INVALID_PARAMETER;
    disk = LoadDisk(dd->DiskNumber, &bps);
    if (!disk) return STATUS_DEVICE_NOT_READY;
    if (IsDynamicTable(disk)) { ExFreePool(disk); return STATUS_NOT_SUPPORTED; }   /* see IOCTL_MGPT_LDM* */
    if (dd->Index >= disk->Count) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    /* protect the running system / boot partition (active partition on
       the boot disk) from deletion. */
    if (IsProtectedPartition(dd->DiskNumber, disk, dd->Index, bps)) {
        ExFreePool(disk);
        return STATUS_ACCESS_DENIED;
    }
    /* an extended partition that still holds logical drives would leave
       them without a container: delete those first */
    if (!disk->IsGpt && IsExtType(disk->Part[dd->Index].MbrType)) {
        ULONG k;
        for (k = 0; k < disk->Count; k++)
            if (disk->Part[k].IsLogical) { ExFreePool(disk); return (NTSTATUS)0xC0000101L; /* STATUS_DIRECTORY_NOT_EMPTY */ }
    }
    if (GptDeletePartition(disk, dd->Index) != GPT_OK) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    st = WriteAndReenum(dd->DiskNumber, disk);
    ExFreePool(disk);
    return st;
}

static NTSTATUS DoInit(PVOID sys, ULONG inlen)
{
    MGPT_INIT *in = (MGPT_INIT *)sys; GPT_DISK *disk; ULONG bps; gpt_u64 total; NTSTATUS st;
    if (inlen < sizeof(MGPT_INIT)) return STATUS_INVALID_PARAMETER;
    disk = LoadDisk(in->DiskNumber, &bps);
    if (!disk) return STATUS_DEVICE_NOT_READY;
    total = disk->DiskSectors;
    /* wiping the whole disk: refuse if it is the boot disk with the active
       system partition. */
    (void)bps;
    if (IsSystemDisk(in->DiskNumber, disk)) {
        ExFreePool(disk);
        return STATUS_ACCESS_DENIED;
    }
    GptSeed((gpt_u32)total ^ 0x55AA1234u);
    GptInitDisk(disk, in->Scheme ? GPT_SCHEME_GPT : GPT_SCHEME_MBR, bps, total);
    st = WriteAndReenum(in->DiskNumber, disk);
    ExFreePool(disk);
    return st;
}

static NTSTATUS DoSetType(PVOID sys, ULONG inlen)
{
    MGPT_SETTYPE *s = (MGPT_SETTYPE *)sys; GPT_DISK *disk; NTSTATUS st;
    if (inlen < sizeof(MGPT_SETTYPE)) return STATUS_INVALID_PARAMETER;
    disk = LoadDisk(s->DiskNumber, NULL);
    if (!disk) return STATUS_DEVICE_NOT_READY;
    if (IsDynamicTable(disk)) { ExFreePool(disk); return STATUS_NOT_SUPPORTED; }   /* see IOCTL_MGPT_LDM* */
    if (GptSetType(disk, s->Index, (gpt_u8)s->MbrType) != GPT_OK) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    st = WriteAndReenum(s->DiskNumber, disk);
    ExFreePool(disk);
    return st;
}

static NTSTATUS DoSetActive(PVOID sys, ULONG inlen)
{
    MGPT_SETACTIVE *s = (MGPT_SETACTIVE *)sys; GPT_DISK *disk; NTSTATUS st;
    if (inlen < sizeof(MGPT_SETACTIVE)) return STATUS_INVALID_PARAMETER;
    disk = LoadDisk(s->DiskNumber, NULL);
    if (!disk) return STATUS_DEVICE_NOT_READY;
    if (IsDynamicTable(disk)) { ExFreePool(disk); return STATUS_NOT_SUPPORTED; }   /* see IOCTL_MGPT_LDM* */
    if (GptSetActive(disk, s->Index, (gpt_u8)(s->Active ? 1 : 0)) != GPT_OK) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    st = WriteAndReenum(s->DiskNumber, disk);
    ExFreePool(disk);
    return st;
}

/* change (or remove) a partition's drive letter; the running system
   partition is refused. Works on owned volumes (this driver's symbolic
   links) and class-driver volumes alike. */
static NTSTATUS DoSetLetter(PVOID sys, ULONG inlen)
{
    MGPT_SETLETTER *s = (MGPT_SETLETTER *)sys; GPT_DISK *disk; ULONG bps;
    int owned, partNum; UCHAR newL, oldL = 0; PPART_EXT ext = NULL;
    char dev[48], link[32]; UNICODE_STRING ul, ut; NTSTATUS st;
    if (inlen < sizeof(MGPT_SETLETTER)) return STATUS_INVALID_PARAMETER;
    disk = LoadDisk(s->DiskNumber, &bps);
    if (!disk) return STATUS_DEVICE_NOT_READY;
    if (IsDynamicTable(disk)) { ExFreePool(disk); return STATUS_NOT_SUPPORTED; }   /* see IOCTL_MGPT_LDM* */
    if (s->Index >= disk->Count) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    if (IsExtType(disk->Part[s->Index].MbrType)) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    if (IsProtectedPartition(s->DiskNumber, disk, s->Index, bps)) { ExFreePool(disk); return STATUS_ACCESS_DENIED; }

    newL = (UCHAR)s->Letter;
    if (newL >= 'a' && newL <= 'z') newL = (UCHAR)(newL - 32);
    if (newL && (newL < 'C' || newL > 'Z')) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }

    owned = DiskOwned(disk, bps);
    /* a volume of ours (owned disk, or a partition the class driver does
       not serve), else the class driver's own volume */
    {
        gpt_u64 off = (gpt_u64)disk->Part[s->Index].StartLba * (gpt_u64)bps;
        LockList();
        for (ext = g_PartList; ext; ext = ext->Next)
            if (ext->DiskNumber == s->DiskNumber && !ext->Stale && (gpt_u64)ext->StartingOffset.QuadPart == off) break;
        UnlockList();
    }
    if (ext) { oldL = ext->DriveLetter; partNum = (int)ext->PartitionNumber; }
    else if (owned) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
    else {
        partNum = ClassPartNumber(s->DiskNumber, disk, s->Index, bps);
        if (partNum == 0) { ExFreePool(disk); return STATUS_INVALID_PARAMETER; }
        BuildLetterMap();
        oldL = MapLookup(s->DiskNumber, (ULONG)partNum);
    }

    sprintf(dev, "\\Device\\Harddisk%lu\\Partition%lu", (unsigned long)s->DiskNumber, (unsigned long)partNum);

    if (oldL) {
        sprintf(link, "\\DosDevices\\%c:", oldL);
        if (NT_SUCCESS(MakeUnicode(link, &ul))) { IoDeleteSymbolicLink(&ul); RtlFreeUnicodeString(&ul); }
    }
    if (newL) {
        if (!NT_SUCCESS(MakeUnicode(dev, &ut))) { ExFreePool(disk); return STATUS_INSUFFICIENT_RESOURCES; }
        sprintf(link, "\\DosDevices\\%c:", newL);
        if (!NT_SUCCESS(MakeUnicode(link, &ul))) { RtlFreeUnicodeString(&ut); ExFreePool(disk); return STATUS_INSUFFICIENT_RESOURCES; }
        st = IoCreateSymbolicLink(&ul, &ut);
        RtlFreeUnicodeString(&ul); RtlFreeUnicodeString(&ut);
        if (!NT_SUCCESS(st)) {
            /* letter in use: put the old link back */
            if (oldL) {
                sprintf(link, "\\DosDevices\\%c:", oldL);
                if (NT_SUCCESS(MakeUnicode(link, &ul))) {
                    if (NT_SUCCESS(MakeUnicode(dev, &ut))) { IoCreateSymbolicLink(&ul, &ut); RtlFreeUnicodeString(&ut); }
                    RtlFreeUnicodeString(&ul);
                }
            }
            ExFreePool(disk);
            return st;
        }
    }
    if (ext) ext->DriveLetter = newL;
    ExFreePool(disk);
    return STATUS_SUCCESS;
}

/* convert the disk between MBR and GPT without moving data. On a layout
   that cannot be converted losslessly, the reason is returned (and the
   disk is left untouched). The running system disk is refused. */
static NTSTATUS DoConvert(PVOID sys, ULONG inlen, ULONG *info)
{
    MGPT_CONVERT *c = (MGPT_CONVERT *)sys; GPT_DISK *src, *dst; ULONG bps;
    NTSTATUS st; int reason = 0, rc;
    if (inlen < sizeof(MGPT_CONVERT)) return STATUS_INVALID_PARAMETER;
    src = LoadDisk(c->DiskNumber, &bps);
    if (!src) return STATUS_DEVICE_NOT_READY;
    if (IsSystemDisk(c->DiskNumber, src)) {
        c->Reason = MGPT_CVT_SYSTEM; *info = sizeof(MGPT_CONVERT);
        ExFreePool(src); return STATUS_SUCCESS;
    }
    dst = (GPT_DISK *)ExAllocatePool(NonPagedPool, sizeof(GPT_DISK));
    if (!dst) { ExFreePool(src); return STATUS_INSUFFICIENT_RESOURCES; }
    GptSeed((gpt_u32)src->DiskSectors ^ 0x7A5B3C1Du);
    rc = GptConvertScheme(src, c->Scheme ? GPT_SCHEME_GPT : GPT_SCHEME_MBR, dst, &reason);
    if (rc != GPT_OK) {
        c->Reason = (ULONG)reason; *info = sizeof(MGPT_CONVERT);
        ExFreePool(dst); ExFreePool(src); return STATUS_SUCCESS;
    }
    st = WriteAndReenum(c->DiskNumber, dst);
    ExFreePool(dst); ExFreePool(src);
    c->Reason = NT_SUCCESS(st) ? MGPT_CVT_OK : MGPT_CVT_WRITE;
    *info = sizeof(MGPT_CONVERT);
    return STATUS_SUCCESS;
}


/* ---- dynamic disks: information and changes -------------------------- */

static ULONG g_seed;
static void RandGuid(gpt_u8 *g)
{
    int i;
    if (!g_seed) { LARGE_INTEGER t; KeQuerySystemTime(&t); g_seed = (ULONG)t.QuadPart ^ (ULONG)(t.QuadPart >> 32) ^ 0x5A17C3E1UL; }
    for (i = 0; i < 16; i++) g[i] = (gpt_u8)(RtlRandom(&g_seed) >> 7);
    g[7] = (gpt_u8)((g[7] & 0x0F) | 0x40);
    g[8] = (gpt_u8)((g[8] & 0x3F) | 0x80);
}

/* "<Computername>Dg0", as Windows names the disk group it creates */
static void GroupName(char *out)
{
    UNICODE_STRING kn, vn; OBJECT_ATTRIBUTES oa; HANDLE h; ULONG ret; int n = 0, i;
    static UCHAR info[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + 80];
    RtlInitUnicodeString(&kn, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\ComputerName\\ComputerName");
    RtlInitUnicodeString(&vn, L"ComputerName");
    InitObjAttr(&oa, &kn);
    if (NT_SUCCESS(ZwOpenKey(&h, KEY_READ, &oa))) {
        if (NT_SUCCESS(ZwQueryValueKey(h, &vn, KeyValuePartialInformation, info, sizeof(info), &ret))) {
            KEY_VALUE_PARTIAL_INFORMATION *pi = (KEY_VALUE_PARTIAL_INFORMATION *)info;
            WCHAR *w = (WCHAR *)pi->Data;
            for (i = 0; i < (int)(pi->DataLength / 2) && w[i] && n < 15; i++) {
                WCHAR c = w[i];
                if (c >= 'A' && c <= 'Z' && n > 0) c = (WCHAR)(c + 32);
                if (c >= 'a' && c <= 'z' && n == 0) c = (WCHAR)(c - 32);
                out[n++] = (c < 0x20 || c > 0x7E) ? '_' : (char)c;
            }
        }
        ZwClose(h);
    }
    if (!n) { StrCopy(out, "MbrGptNt", 32); n = 8; }
    StrCopy(out + n, "Dg0", 32 - n);
}

/* a drive letter nothing uses, or 0 */
static UCHAR FreeLetter(void)
{
    UCHAR c; WCHAR link[20]; int i; const char *pfx = "\\DosDevices\\";
    for (c = 'C'; c <= 'Z'; c++) {
        for (i = 0; pfx[i]; i++) link[i] = (WCHAR)(UCHAR)pfx[i];
        link[i++] = (WCHAR)c; link[i++] = ':'; link[i] = 0;
        if (!NT_SUCCESS(ResolveLink(link, g_osbuf, 280))) return c;
    }
    return 0;
}

static PPART_EXT LdmExtByGuid(const char *dg, const gpt_u8 *guid)
{
    PPART_EXT p;
    LockList();
    for (p = g_PartList; p; p = p->Next)
        if (p->Ldm && !p->Stale && StrSame(p->Ldm->DgGuid, dg) && MemSame(p->Ldm->Guid, guid, 16)) break;
    UnlockList();
    return p;
}

/* The letter Windows' own LDM driver gave a volume: the drive letter whose
   volume reports the volume's first extent. */
#define IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS 0x00560000UL
static WCHAR g_nlbuf[280];
static UCHAR NativeLetter(LDMGRP *g, LDM_VMAP *m)
{
    UCHAR c; ULONG nt; gpt_u64 off;
    static UCHAR ext[16 + 24 * 8];
    if (!m->NPlex || !m->Plex[0].N) return 0;
    nt = g->NtDisk[m->Plex[0].X[0].Disk]; off = m->Plex[0].X[0].ByteStart;
    for (c = 'C'; c <= 'Z'; c++) {
        WCHAR link[20]; int i; const char *pfx = "\\DosDevices\\";
        char a[24]; UNICODE_STRING un; PFILE_OBJECT f; PDEVICE_OBJECT dev; ULONG k, cnt;
        for (i = 0; pfx[i]; i++) link[i] = (WCHAR)(UCHAR)pfx[i];
        link[i++] = (WCHAR)c; link[i++] = ':'; link[i] = 0;
        if (!NT_SUCCESS(ResolveLink(link, g_nlbuf, 280)) || WFind(g_nlbuf, "DmVolumes") < 0) continue;
        sprintf(a, "\\DosDevices\\%c:", c);
        if (!NT_SUCCESS(MakeUnicode(a, &un))) continue;
        if (!NT_SUCCESS(IoGetDeviceObjectPointer(&un, FILE_READ_ATTRIBUTES, &f, &dev))) { RtlFreeUnicodeString(&un); continue; }
        RtlFreeUnicodeString(&un);
        MemZero(ext, sizeof(ext));
        if (NT_SUCCESS(SyncIoctl(IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, dev, NULL, 0, ext, sizeof(ext)))) {
            cnt = *(ULONG *)ext;
            for (k = 0; k < cnt && k < 8; k++) {
                UCHAR *e = ext + 8 + 24 * k;
                if (*(ULONG *)e == nt && *(gpt_u64 *)(e + 8) == off) { ObDereferenceObject(f); return c; }
            }
        }
        ObDereferenceObject(f);
    }
    return 0;
}

static ULONG VolLetter(LDMGRP *g, int v, LDM_VMAP *m)
{
    PPART_EXT p;
    if (g->Native) return OsHasLdm() ? NativeLetter(g, m) : 0;
    p = LdmExtByGuid(g->DgGuid, g->Db->Vol[v].Guid);
    return p ? p->DriveLetter : 0;
}

static int AllPresent(LDMGRP *g)
{ ULONG i; for (i = 0; i < g->Db->NDisks; i++) if (!g->Present[i]) return 0; return 1; }

/* the group index and database disk index of NT disk dn, or -1 */
static int LdmGroupOf(ULONG dn, int *di)
{
    int k, i;
    for (k = 0; k < g_ngrp; k++)
        for (i = 0; i < (int)g_grp[k]->Db->NDisks; i++)
            if (g_grp[k]->Present[i] && g_grp[k]->NtDisk[i] == dn) { if (di) *di = i; return k; }
    return -1;
}

static int IsDynamicTable(GPT_DISK *disk)
{ ULONG i; for (i = 0; i < disk->Count; i++) if (disk->Part[i].MbrType == GPT_MBRTYPE_LDM) return 1; return 0; }

static ULONG VolPieces(LDM_VMAP *m)
{ ULONG p, n = 0; for (p = 0; p < m->NPlex; p++) n += m->Plex[p].N; return n; }

/* MGPT_LDMINFO */
static NTSTATUS DoLdmInfo(PVOID sys, ULONG outlen, ULONG *info)
{
    MGPT_LDMINFO *li = (MGPT_LDMINFO *)sys; int k, i, v; LDM_VMAP *m; LDM_FREE *runs;
    if (outlen < sizeof(MGPT_LDMINFO)) return STATUS_BUFFER_TOO_SMALL;
    m = (LDM_VMAP *)ExAllocatePool(NonPagedPool, sizeof(LDM_VMAP));
    runs = (LDM_FREE *)ExAllocatePool(NonPagedPool, 64 * sizeof(LDM_FREE));
    if (!m || !runs) { if (m) ExFreePool(m); if (runs) ExFreePool(runs); return STATUS_INSUFFICIENT_RESOURCES; }
    MemZero(li, sizeof(MGPT_LDMINFO));
    KeWaitForSingleObject(&g_LdmLock, Executive, KernelMode, FALSE, NULL);
    for (k = 0; k < g_ngrp && k < MGPT_LDM_GROUPS; k++) {
        LDMGRP *g = g_grp[k]; MGPT_LDMGROUP *o = &li->Group[k];
        StrCopy(o->Name, g->Db->DgName, 32); StrCopy(o->Guid, g->DgGuid, 40);
        o->Writable = (!g->Native && AllPresent(g)) ? 1 : 0;
        o->Native = (ULONG)g->Native;
        o->NDisks = g->Db->NDisks; o->NVols = g->Db->NVols;
        for (i = 0; i < (int)g->Db->NDisks && i < MGPT_LDM_DISKS; i++) {
            MGPT_LDMDISK *d = &o->Disk[i]; int n, r;
            gpt_u64 pub512 = g->PubSize[i] * g->Bps[i] / 512;
            StrCopy(d->Name, g->Db->Disk[i].Name, 16);
            d->NtDisk = g->Present[i] ? g->NtDisk[i] : 0xFFFFFFFFUL;
            d->Stale = g->Stale[i]; d->PubSize = pub512;
            if (g->Present[i]) {
                n = LdmFreeRuns(g->Db, i, pub512, runs, 64);
                for (r = 0; r < n; r++) {
                    d->FreeTotal += runs[r].Size;
                    if (runs[r].Size > d->FreeLargest) d->FreeLargest = runs[r].Size;
                }
            }
        }
        for (v = 0; v < (int)g->Db->NVols && v < MGPT_LDM_VOLS; v++) {
            MGPT_LDMVOL *o2 = &o->Vol[v]; LDM_VOL *vv = &g->Db->Vol[v]; int st;
            StrCopy(o2->Name, vv->Name, 16); memcpy(o2->Guid, vv->Guid, 16);
            o2->Type = (ULONG)vv->Type; o2->PartType = vv->PartType; o2->Size = vv->Size;
            if (LdmMap(g, v, m) == GPT_OK) {
                st = LdmMapState(m, g->Present);
                o2->State = st == LDM_HEALTHY ? MGPT_LDMS_HEALTHY : st == LDM_DEGRADED ? MGPT_LDMS_DEGRADED : MGPT_LDMS_FAILED;
                o2->Pieces = VolPieces(m);
                o2->FsKind = LdmVolFs(g, v, m);
                o2->Letter = VolLetter(g, v, m);
            } else o2->State = MGPT_LDMS_FAILED;
        }
    }
    li->Count = (ULONG)(g_ngrp < MGPT_LDM_GROUPS ? g_ngrp : MGPT_LDM_GROUPS);
    KeSetEvent(&g_LdmLock, 0, FALSE);
    ExFreePool(m); ExFreePool(runs);
    *info = sizeof(MGPT_LDMINFO);
    return STATUS_SUCCESS;
}

/* Fill the layout of dynamic disk dn: the table entries that are not the
   LDM container (EFI system, reserved, LDM metadata), then one entry per
   extent of a dynamic volume on this disk, sorted by position. */
static void LdmLayout(ULONG dn, GPT_DISK *disk, MGPT_LAYOUT *lay)
{
    int k, di, i, v, n = 0; LDM_VMAP *m; LDMGRP *g; ULONG bps = disk->SectorSize;
    m = (LDM_VMAP *)ExAllocatePool(NonPagedPool, sizeof(LDM_VMAP));
    if (!m) return;
    KeWaitForSingleObject(&g_LdmLock, Executive, KernelMode, FALSE, NULL);
    k = LdmGroupOf(dn, &di);
    if (k < 0) { KeSetEvent(&g_LdmLock, 0, FALSE); ExFreePool(m); return; }
    g = g_grp[k];
    lay->Dynamic = 1; lay->LdmGroup = (ULONG)k;
    lay->LdmWritable = (!g->Native && AllPresent(g)) ? 1 : 0;
    for (i = 0; i < (int)g->Db->NDisks; i++) if (!g->Present[i]) lay->LdmMissing++;
    lay->PubStart = g->PubStart[di]; lay->PubSize = g->PubSize[di];
    StrCopy(lay->DgName, g->Db->DgName, 32);
    for (i = 0; i < (int)disk->Count && n < MGPT_MAX_PARTS; i++) {
        GPT_PARTITION *p = &disk->Part[i];
        if (p->MbrType == GPT_MBRTYPE_LDM && p->FsKind != GPT_FS_LDMMETA) continue;
        MemZero(&lay->Part[n], sizeof(MGPT_PART));
        FillMgptPart(&lay->Part[n], p);
        n++;
    }
    for (v = 0; v < (int)g->Db->NVols; v++) {
        int st = LDM_FAILED, pl, x; ULONG letter = 0, pieces = 0; UCHAR fs = 0;
        if (LdmMap(g, v, m) != GPT_OK) continue;
        st = LdmMapState(m, g->Present);
        if (!LdmMapUsesDisk(m, (gpt_u32)di)) continue;
        pieces = VolPieces(m); fs = LdmVolFs(g, v, m); letter = VolLetter(g, v, m);
        for (pl = 0; pl < (int)m->NPlex; pl++)
            for (x = 0; x < (int)m->Plex[pl].N; x++) {
                LDM_X *e = &m->Plex[pl].X[x]; MGPT_PART *o;
                if (e->Disk != (gpt_u32)di || n >= MGPT_MAX_PARTS) continue;
                o = &lay->Part[n++];
                MemZero(o, sizeof(*o));
                o->StartLba = UDiv(e->ByteStart, bps); o->SectorCount = UDiv(e->Size, bps);
                o->MbrType = g->Db->Vol[v].PartType; o->FsKind = fs;
                o->Flags = MGPT_F_LDM | MGPT_F_RECOGNIZED; o->DriveLetter = letter;
                o->LdmGroup = (ULONG)k; o->LdmVol = (ULONG)v; o->LdmType = (ULONG)g->Db->Vol[v].Type;
                o->LdmState = st == LDM_HEALTHY ? MGPT_LDMS_HEALTHY : st == LDM_DEGRADED ? MGPT_LDMS_DEGRADED : MGPT_LDMS_FAILED;
                o->LdmPiece = (m->Type == LDM_VT_MIRROR) ? (ULONG)pl : (m->Type == LDM_VT_STRIPED || m->Type == LDM_VT_RAID5) ? e->Column : (ULONG)x;
                o->LdmPieces = pieces; o->LdmVolSize = g->Db->Vol[v].Size;
                StrCopy(o->VolName, g->Db->Vol[v].Name, 16);
            }
    }
    KeSetEvent(&g_LdmLock, 0, FALSE);
    ExFreePool(m);
    /* by position */
    for (i = 1; i < n; i++) {
        MGPT_PART t; int j = i;
        memcpy(&t, &lay->Part[i], sizeof(t));
        while (j > 0 && lay->Part[j - 1].StartLba > t.StartLba) { memcpy(&lay->Part[j], &lay->Part[j - 1], sizeof(t)); j--; }
        memcpy(&lay->Part[j], &t, sizeof(t));
    }
    lay->Count = (ULONG)n;
}

/* ---- changing a disk group -------------------------------------------
   Every change is made to a fresh copy of the database read from the
   newest member and written to every member (all must be present), with
   the disk-check lock held. The volumes are updated afterwards (LdmSync). */

static LDM_DB *LdmLoadWork(LDMGRP *g)
{
    LDM_DB *db; int i, best = -1; PFILE_OBJECT f; PDEVICE_OBJECT p0; ENUMCTX ctx; int rc;
    for (i = 0; i < (int)g->Db->NDisks; i++)
        if (g->Present[i] && g->Hdr[i] && !g->Stale[i]) { best = i; break; }
    if (best < 0) return NULL;
    db = (LDM_DB *)ExAllocatePool(NonPagedPool, sizeof(LDM_DB));
    if (!db) return NULL;
    MemZero(db, sizeof(*db));
    if (!NT_SUCCESS(OpenPartition0(g->NtDisk[best], &f, &p0))) { ExFreePool(db); return NULL; }
    ctx.Part0 = p0; ctx.BytesPerSector = g->Bps[best]; ctx.Limit = 0;
    rc = LdmReadDb(EnumRead, &ctx, g->Hdr[best], db);
    ObDereferenceObject(f);
    if (rc != GPT_OK) { LdmFreeDb(db); ExFreePool(db); return NULL; }
    return db;
}

static void LdmDropWork(LDM_DB *db) { if (db) { LdmFreeDb(db); ExFreePool(db); } }

/* Write db to the members of g (skip: a database disk index to leave out),
   and to one more disk (a disk being added). */
static int LdmCommit(LDMGRP *g, LDM_DB *db, int skip, ULONG extraNt, LDM_HDR *extraHdr)
{
    LARGE_INTEGER now; int i, bad = 0, done = 0;
    KeQuerySystemTime(&now);
    for (i = 0; g && i < (int)g->Db->NDisks; i++) {
        PFILE_OBJECT f; PDEVICE_OBJECT p0; ENUMCTX ctx;
        if (i == skip || !g->Present[i] || !g->Hdr[i]) continue;
        /* a database the disk driver cannot reach completely (it lies past
           the last whole cylinder, which NT 3.x / 4.0 do not address) keeps
           its older copy; the disks with a newer one win */
        if (g->Hdr[i]->PrivStart + g->Hdr[i]->ConfStart + g->Hdr[i]->ConfSize + 2 > g->Total[i]) continue;
        done++;
        if (!NT_SUCCESS(OpenPartition0(g->NtDisk[i], &f, &p0))) { bad = 1; continue; }
        ctx.Part0 = p0; ctx.BytesPerSector = g->Bps[i]; ctx.Limit = g->Total[i];
        if (LdmWriteDisk(EnumWrite, &ctx, g->Hdr[i], db, (gpt_u64)now.QuadPart) != GPT_OK) { bad = 1; D("ldm write failed, disk "); DH(g->NtDisk[i]); D("\n"); }
        ObDereferenceObject(f);
    }
    if (extraHdr) {
        PFILE_OBJECT f; PDEVICE_OBJECT p0; ENUMCTX ctx;
        done++;
        if (!NT_SUCCESS(OpenPartition0(extraNt, &f, &p0))) return 1;
        ctx.Part0 = p0; ctx.BytesPerSector = 512; ctx.Limit = 0;
        if (LdmWriteDisk(EnumWrite, &ctx, extraHdr, db, (gpt_u64)now.QuadPart) != GPT_OK) bad = 1;
        ObDereferenceObject(f);
    }
    return bad || !done;
}

/* SyncDisk / LdmSync in a system worker thread (see SyncDiskInSystem) */
static void LdmSyncInSystem(void) { SyncDiskInSystem(0xFFFFFFFFUL); }

static ULONG LdmReason(int rc)
{
    switch (rc) {
    case GPT_OK: return MGPT_LDMR_OK;
    case LDM_ERR_SPACE: return MGPT_LDMR_SPACE;
    case LDM_ERR_FULL: return MGPT_LDMR_FULL;
    case LDM_ERR_MISSING: return MGPT_LDMR_MISSING;
    case LDM_ERR_INUSE: return MGPT_LDMR_NOTEMPTY;
    }
    return MGPT_LDMR_PARAM;
}

/* zero [off, off+len) of NT disk nt */
static int ZeroRange(ULONG nt, gpt_u64 off, gpt_u64 len)
{
    PFILE_OBJECT f; PDEVICE_OBJECT p0; LARGE_INTEGER o; int bad = 0;
    if (!g_zero) { g_zero = (UCHAR *)ExAllocatePool(NonPagedPool, LDM_BOUNCE); if (!g_zero) return 1; MemZero(g_zero, LDM_BOUNCE); }
    if (!NT_SUCCESS(OpenPartition0(nt, &f, &p0))) return 1;
    while (len && !bad) {
        ULONG n = len > LDM_BOUNCE ? LDM_BOUNCE : (ULONG)len;
        o.QuadPart = (LONGLONG)off;
        if (!NT_SUCCESS(SyncTransfer(IRP_MJ_WRITE, p0, g_zero, n, &o))) bad = 1;
        off += n; len -= n;
    }
    ObDereferenceObject(f);
    return bad;
}

static void LdmLock(void)   { KeWaitForSingleObject(&g_ScanLock, Executive, KernelMode, FALSE, NULL); }
static void LdmUnlock(void) { g_Gen++; KeSetEvent(&g_ScanLock, 0, FALSE); }

static NTSTATUS DoLdmCreate(PVOID sys, ULONG inlen, ULONG *info)
{
    MGPT_LDMCREATE *c = (MGPT_LDMCREATE *)sys; LDMGRP *g; LDM_DB *db = NULL; LDM_NEWVOL *s; LDM_VMAP *m;
    gpt_u64 pub512[LDM_MAX_DISKS]; int i, vi, rc; UCHAR letter; gpt_u8 guid[16];
    if (inlen < sizeof(MGPT_LDMCREATE)) return STATUS_INVALID_PARAMETER;
    *info = sizeof(MGPT_LDMCREATE);
    c->Letter = 0; c->VolSize = 0;
    s = (LDM_NEWVOL *)ExAllocatePool(NonPagedPool, sizeof(LDM_NEWVOL));
    m = (LDM_VMAP *)ExAllocatePool(NonPagedPool, sizeof(LDM_VMAP));
    if (!s || !m) { if (s) ExFreePool(s); if (m) ExFreePool(m); return STATUS_INSUFFICIENT_RESOURCES; }
    LdmLock();
    c->Reason = MGPT_LDMR_PARAM;
    if (c->Group >= (ULONG)g_ngrp || c->NDisks == 0 || c->NDisks > MGPT_LDM_DISKS || c->Type < 1 || c->Type > 5) goto out;
    g = g_grp[c->Group];
    if (g->Native) { c->Reason = MGPT_LDMR_NATIVE; goto out; }
    if (!AllPresent(g)) { c->Reason = MGPT_LDMR_MISSING; goto out; }
    db = LdmLoadWork(g);
    if (!db) { c->Reason = MGPT_LDMR_WRITE; goto out; }
    if (db->NDisks != g->Db->NDisks) { c->Reason = MGPT_LDMR_MISSING; goto out; }
    MemZero(s, sizeof(*s));
    s->Type = (int)c->Type; s->NDisks = c->NDisks; s->Stripe = c->Stripe; s->PartType = 0x07;
    for (i = 0; i < (int)c->NDisks; i++) {
        if (c->Disk[i] >= db->NDisks) goto out;
        s->Disk[i] = c->Disk[i]; s->Size[i] = c->Size[i];
    }
    for (i = 0; i < (int)db->NDisks; i++) pub512[i] = g->PubSize[i] * g->Bps[i] / 512;
    RandGuid(guid); memcpy(s->Guid, guid, 16);
    letter = FreeLetter();
    if (letter) { s->Hint[0] = (char)letter; s->Hint[1] = ':'; s->Hint[2] = 0; }
    rc = LdmAddVolume(db, s, pub512, &vi);
    if (rc != GPT_OK) { c->Reason = LdmReason(rc); goto out; }
    c->VolSize = db->Vol[vi].Size;
    /* a new volume starts out zeroed: RAID-5 entirely (so the parity is
       right), the others at their start (no old file system shows up) */
    MemZero(m, sizeof(*m));
    if (LdmBuildMap(db, vi, g->PubStart, g->Bps, m) == GPT_OK) {
        int pl, x;
        for (pl = 0; pl < (int)m->NPlex; pl++)
            for (x = 0; x < (int)m->Plex[pl].N; x++) {
                LDM_X *e = &m->Plex[pl].X[x];
                gpt_u64 n = (c->Type == MGPT_LDM_RAID5) ? e->Size : (e->Size < 1048576 ? e->Size : 1048576);
                ZeroRange(g->NtDisk[e->Disk], e->ByteStart, n);
            }
    }
    c->Reason = LdmCommit(g, db, -1, 0, NULL) ? MGPT_LDMR_WRITE : MGPT_LDMR_OK;
out:
    LdmDropWork(db);
    LdmUnlock();
    if (c->Reason == MGPT_LDMR_OK) {
        PPART_EXT p; LdmSyncInSystem();
        KeWaitForSingleObject(&g_LdmLock, Executive, KernelMode, FALSE, NULL);
        if (c->Group < (ULONG)g_ngrp && (p = LdmExtByGuid(g_grp[c->Group]->DgGuid, guid)) != NULL) c->Letter = p->DriveLetter;
        KeSetEvent(&g_LdmLock, 0, FALSE);
    }
    ExFreePool(s); ExFreePool(m);
    return STATUS_SUCCESS;
}

/* drop the MBR entries Windows keeps for a volume made from a basic
   partition ("retained"), when that volume is deleted */
static void DropRetained(LDMGRP *g, LDM_DB *db, int vi)
{
    ULONG c, p; GPT_DISK *disk = NULL;
    for (c = 0; c < db->NComps; c++) {
        if (db->Comp[c].VolId != db->Vol[vi].Id) continue;
        for (p = 0; p < db->NParts; p++) {
            LDM_PART *pt = &db->Part[p]; int di; ULONG bps, i; gpt_u64 lba, cnt;
            if (pt->CompId != db->Comp[c].Id) continue;
            for (di = 0; di < (int)db->NDisks; di++) if (db->Disk[di].Id == pt->DiskId) break;
            if (di >= (int)db->NDisks || di >= (int)g->Db->NDisks || !g->Present[di] || g->IsGpt[di]) continue;
            if (!disk) disk = (GPT_DISK *)ExAllocatePool(NonPagedPool, sizeof(GPT_DISK));
            if (!disk || !(bps = ParseDisk(g->NtDisk[di], disk)) || disk->IsGpt) continue;
            lba = g->PubStart[di] + UDiv(pt->Start * 512, bps); cnt = UDiv(pt->Size * 512, bps);
            for (i = 0; i < disk->Count; i++)
                if (disk->Part[i].MbrType == GPT_MBRTYPE_LDM && disk->Part[i].StartLba == lba && disk->Part[i].SectorCount == cnt) {
                    PFILE_OBJECT f; PDEVICE_OBJECT p0; ENUMCTX ctx;
                    GptDeletePartition(disk, i);
                    if (NT_SUCCESS(OpenPartition0(g->NtDisk[di], &f, &p0))) {
                        ctx.Part0 = p0; ctx.BytesPerSector = bps; ctx.Limit = 0;
                        GptWriteDisk(EnumRead, EnumWrite, &ctx, disk);
                        ObDereferenceObject(f);
                    }
                    break;
                }
        }
    }
    if (disk) ExFreePool(disk);
}

/* Take a volume out of service for a change: dismounted (it must be idle),
   letter removed, I/O refused. Returns 0 when it is in use. */
static int RetireVolume(PPART_EXT p)
{
    if (DeviceInUse(p->Self) && !DismountIfIdle(p->DevName)) return 0;
    p->Stale = 1; p->Gone = 1;
    UnlinkLetter(p);
    return 1;
}

static NTSTATUS DoLdmDelete(PVOID sys, ULONG inlen, ULONG *info)
{
    MGPT_LDMVOLREQ *r = (MGPT_LDMVOLREQ *)sys; LDMGRP *g; LDM_DB *db = NULL; int vi, k; PPART_EXT p;
    if (inlen < sizeof(MGPT_LDMVOLREQ)) return STATUS_INVALID_PARAMETER;
    *info = sizeof(MGPT_LDMVOLREQ);
    LdmLock();
    r->Reason = MGPT_LDMR_PARAM;
    if (r->Group >= (ULONG)g_ngrp) goto out;
    g = g_grp[r->Group];
    if (g->Native) { r->Reason = MGPT_LDMR_NATIVE; goto out; }
    if (!AllPresent(g)) { r->Reason = MGPT_LDMR_MISSING; goto out; }
    db = LdmLoadWork(g);
    if (!db) { r->Reason = MGPT_LDMR_WRITE; goto out; }
    for (vi = 0; vi < (int)db->NVols; vi++) if (MemSame(db->Vol[vi].Guid, r->Guid, 16)) break;
    if (vi >= (int)db->NVols) goto out;
    p = LdmExtByGuid(g->DgGuid, r->Guid);
    if (p) {
        KeWaitForSingleObject(&g_SyncLock, Executive, KernelMode, FALSE, NULL);
        k = RetireVolume(p);
        KeSetEvent(&g_SyncLock, 0, FALSE);
        if (!k) { r->Reason = MGPT_LDMR_INUSE; goto out; }
    }
    if (db->Vol[vi].Flags & 0x800000u) DropRetained(g, db, vi);
    LdmDeleteVolume(db, vi);
    r->Reason = LdmCommit(g, db, -1, 0, NULL) ? MGPT_LDMR_WRITE : MGPT_LDMR_OK;
out:
    LdmDropWork(db);
    LdmUnlock();
    LdmSyncInSystem();
    return STATUS_SUCCESS;
}

static NTSTATUS DoLdmLetter(PVOID sys, ULONG inlen, ULONG *info)
{
    MGPT_LDMVOLREQ *r = (MGPT_LDMVOLREQ *)sys; LDMGRP *g; LDM_DB *db = NULL; PPART_EXT p; UCHAR newL, oldL; int vi;
    if (inlen < sizeof(MGPT_LDMVOLREQ)) return STATUS_INVALID_PARAMETER;
    *info = sizeof(MGPT_LDMVOLREQ);
    newL = (UCHAR)r->Letter;
    if (newL >= 'a' && newL <= 'z') newL = (UCHAR)(newL - 32);
    LdmLock();
    r->Reason = MGPT_LDMR_PARAM;
    if (r->Group >= (ULONG)g_ngrp || (newL && (newL < 'C' || newL > 'Z'))) goto out;
    g = g_grp[r->Group];
    if (g->Native) { r->Reason = MGPT_LDMR_NATIVE; goto out; }
    p = LdmExtByGuid(g->DgGuid, r->Guid);
    if (!p) goto out;
    oldL = p->DriveLetter;
    if (newL != oldL) {
        UnlinkLetter(p);
        if (newL && !LinkLetter(newL, p->DevName)) {
            if (oldL) p->DriveLetter = LinkLetter(oldL, p->DevName);
            r->Reason = MGPT_LDMR_INUSE; goto out;
        }
        p->DriveLetter = newL;
    }
    r->Reason = MGPT_LDMR_OK;
    /* remember it in the database, for the next start */
    if (AllPresent(g) && (db = LdmLoadWork(g)) != NULL) {
        for (vi = 0; vi < (int)db->NVols; vi++)
            if (MemSame(db->Vol[vi].Guid, r->Guid, 16)) {
                char h[4]; h[0] = (char)newL; h[1] = ':'; h[2] = 0;
                if (LdmSetVolumeHint(db, vi, newL ? h : "") == GPT_OK) LdmCommit(g, db, -1, 0, NULL);
                break;
            }
    }
out:
    LdmDropWork(db);
    LdmUnlock();
    return STATUS_SUCCESS;
}

/* IOCTL_DISK_SET_PARTITION_INFO on a dynamic volume (FORMAT): store the
   type byte in the volume record */
static NTSTATUS LdmSetType(LDMRT *rt, UCHAR type)
{
    LDMGRP *g = NULL; LDM_DB *db = NULL; int k, vi;
    if (type == 0 || IsExtType(type)) return STATUS_INVALID_PARAMETER;
    LdmLock();
    for (k = 0; k < g_ngrp; k++) if (StrSame(g_grp[k]->DgGuid, rt->DgGuid)) g = g_grp[k];
    if (g && !g->Native && AllPresent(g) && (db = LdmLoadWork(g)) != NULL)
        for (vi = 0; vi < (int)db->NVols; vi++)
            if (MemSame(db->Vol[vi].Guid, rt->Guid, 16)) {
                if (db->Vol[vi].PartType != type && LdmSetVolumeType(db, vi, type) == GPT_OK && !LdmCommit(g, db, -1, 0, NULL))
                    { g_LdmDirty = 1; KeSetEvent(&g_ScanReq, 0, FALSE); }
                break;
            }
    LdmDropWork(db);
    LdmUnlock();
    return STATUS_SUCCESS;
}

/* Take the volumes of a basic disk being converted out of service: every
   partition must be idle; its letter goes to the dynamic volume. */
static int ReleaseBasicVolumes(ULONG dn, GPT_DISK *disk, ULONG bps, UCHAR *letters)
{
    ULONG i; PPART_EXT p;
    BuildClassView(); BuildLetterMap();
    for (i = 0; i < disk->Count; i++) {
        gpt_u64 off = disk->Part[i].StartLba * bps; char name[48]; int pn;
        letters[i] = 0;
        if (IsExtType(disk->Part[i].MbrType)) continue;
        LockList();
        for (p = g_PartList; p; p = p->Next)
            if (p->DiskNumber == dn && !p->Stale && (gpt_u64)p->StartingOffset.QuadPart == off) break;
        UnlockList();
        if (p) {
            letters[i] = p->DriveLetter;
            KeWaitForSingleObject(&g_SyncLock, Executive, KernelMode, FALSE, NULL);
            pn = RetireVolume(p);
            KeSetEvent(&g_SyncLock, 0, FALSE);
            if (!pn) return 0;
            continue;
        }
        pn = ClassPartNumber(dn, disk, i, bps);
        if (!pn) continue;
        sprintf(name, "\\Device\\Harddisk%lu\\Partition%lu", (unsigned long)dn, (unsigned long)pn);
        {
            int ci = ClassFind(dn, off, disk->Part[i].SectorCount * bps);
            if (ci >= 0 && DeviceInUse(g_cv[ci].dev) && !DismountIfIdle(name)) return 0;
            if (ci < 0 && !DismountIfIdle(name)) {
                /* not a class volume we know: fine if it cannot be opened */
            }
        }
        letters[i] = MapLookup(dn, (ULONG)pn);
        if (letters[i]) {
            char link[32]; UNICODE_STRING ul;
            sprintf(link, "\\DosDevices\\%c:", letters[i]);
            if (NT_SUCCESS(MakeUnicode(link, &ul))) { IoDeleteSymbolicLink(&ul); RtlFreeUnicodeString(&ul); }
        }
    }
    return 1;
}

static void SetGptName(GPT_PARTITION *p, const char *s)
{ int i; for (i = 0; i < 36; i++) p->Name[i] = 0; for (i = 0; s[i] && i < 35; i++) p->Name[i] = (gpt_u16)(UCHAR)s[i]; }

static ULONG ToDynamic(ULONG dn)
{
    GPT_DISK *disk; ULONG bps, i, reason = MGPT_LDMR_PARAM; LDMGRP *g = NULL; LDM_DB *db = NULL; LDM_HDR *h = NULL;
    LDM_NEWVOL *s = NULL; gpt_u64 pubStart, pubSize, privStart, pub512[LDM_MAX_DISKS];
    UCHAR letters[GPT_MAX_PARTITIONS]; gpt_u8 dguid[16]; int k, di, vi, isGpt;
    PFILE_OBJECT f; PDEVICE_OBJECT p0; ENUMCTX ctx;

    disk = LoadDisk(dn, &bps);
    if (!disk) return MGPT_LDMR_PARAM;
    isGpt = disk->IsGpt;
    if (IsDynamicTable(disk)) { reason = MGPT_LDMR_SAME; goto out; }
    if (IsSystemDisk(dn, disk)) { reason = MGPT_LDMR_SYSTEM; goto out; }
    if (bps != 512) { reason = MGPT_LDMR_SECTOR; goto out; }
#if defined(_WIN64)
    reason = MGPT_LDMR_NATIVE; goto out;
#endif
    if (!isGpt && OsHasLdm()) { reason = MGPT_LDMR_NATIVE; goto out; }
    /* the disk group to join: ours, else a new one */
    for (k = 0; k < g_ngrp; k++) if (!g_grp[k]->Native) { g = g_grp[k]; break; }
    if (!g && g_ngrp) { reason = MGPT_LDMR_FOREIGN; goto out; }
    if (g && !AllPresent(g)) { reason = MGPT_LDMR_MISSING; goto out; }

    if (!isGpt) {
        gpt_u64 cyl = (gpt_u64)(disk->Heads ? disk->Heads : 255) * (disk->SectorsPerTrack ? disk->SectorsPerTrack : 63), maxEnd = 63;
        if (disk->DiskSectors < 4096 + 63) { reason = MGPT_LDMR_NOROOM; goto out; }
        privStart = disk->DiskSectors - 2048;
        for (i = 0; i < disk->Count; i++) {
            GPT_PARTITION *p = &disk->Part[i];
            if (p->IsLogical || IsExtType(p->MbrType)) { reason = MGPT_LDMR_LOGICAL; goto out; }
            if (p->StartLba < 63) { reason = MGPT_LDMR_NOROOM; goto out; }
            if (p->StartLba + p->SectorCount > maxEnd) maxEnd = p->StartLba + p->SectorCount;
        }
        if (maxEnd > privStart) { reason = MGPT_LDMR_NOROOM; goto out; }
        pubStart = 63;
        pubSize = UDiv(privStart, (ULONG)cyl) * cyl;
        if (pubSize < maxEnd) pubSize = maxEnd;
        pubSize -= pubStart;
    } else {
        /* the database goes to the end of the reserved partition (made
           1 MB smaller) or to the end of the largest free area; volumes
           live in the largest area between the EFI system, reserved and
           metadata partitions that holds all data partitions */
        gpt_u64 first = 34, last = disk->DiskSectors - 34, a = 0, b = 0, cur; int msr = -1;
        ULONG idx;
        for (i = 0; i < disk->Count; i++) if (disk->Part[i].FsKind == GPT_FS_MSR) msr = (int)i;
        if (msr >= 0 && disk->Part[msr].SectorCount >= 4096) {
            disk->Part[msr].SectorCount -= 2048;
            privStart = disk->Part[msr].StartLba + disk->Part[msr].SectorCount;
        } else {
            gpt_u64 gs, gl = GptLargestFreeGap(disk, &gs);
            if (gl < 2048) { reason = MGPT_LDMR_NOROOM; goto out; }
            privStart = gs + gl - 2048;
        }
        idx = disk->Count;
        if (idx >= GPT_MAX_PARTITIONS) { reason = MGPT_LDMR_FULL; goto out; }
        MemZero(&disk->Part[idx], sizeof(GPT_PARTITION));
        disk->Part[idx].StartLba = privStart; disk->Part[idx].SectorCount = 2048;
        memcpy(disk->Part[idx].TypeGuid, LdmGuidMetadata, 16); RandGuid(disk->Part[idx].UniqueGuid);
        disk->Part[idx].MbrType = GPT_MBRTYPE_LDM; disk->Part[idx].FsKind = GPT_FS_LDMMETA; disk->Part[idx].IsGptEntry = 1;
        SetGptName(&disk->Part[idx], "LDM metadata partition");
        disk->Count++;
        /* areas between the fixed partitions */
        for (cur = first; cur <= last;) {
            gpt_u64 next = last + 1, after = 0; int ok = 1;
            for (i = 0; i < disk->Count; i++) {
                GPT_PARTITION *p = &disk->Part[i];
                int fixed = p->FsKind == GPT_FS_MSR || p->FsKind == GPT_FS_LDMMETA || p->MbrType == GPT_MBRTYPE_EFISYS;
                if (!fixed) continue;
                if (p->StartLba <= cur && p->StartLba + p->SectorCount > cur) { after = p->StartLba + p->SectorCount; break; }
                if (p->StartLba > cur && p->StartLba < next) next = p->StartLba;
            }
            if (after) { cur = after; continue; }
            for (i = 0; i < disk->Count; i++) {
                GPT_PARTITION *p = &disk->Part[i];
                int fixed = p->FsKind == GPT_FS_MSR || p->FsKind == GPT_FS_LDMMETA || p->MbrType == GPT_MBRTYPE_EFISYS;
                if (!fixed && (p->StartLba < cur || p->StartLba + p->SectorCount > next)) ok = 0;
            }
            if (ok && next - cur > b - a) { a = cur; b = next; }
            cur = next;
        }
        if (b - a < 2048) { reason = MGPT_LDMR_NOROOM; goto out; }
        pubStart = a; pubSize = b - a;
    }

    /* the database: the group's (or a new one) plus this disk and one simple
       volume for each partition it holds */
    if (g) { db = LdmLoadWork(g); if (!db) { reason = MGPT_LDMR_WRITE; goto out; } }
    else {
        char name[32]; gpt_u8 gg[16];
        db = (LDM_DB *)ExAllocatePool(NonPagedPool, sizeof(LDM_DB));
        if (!db) { reason = MGPT_LDMR_WRITE; goto out; }
        MemZero(db, sizeof(*db));
        GroupName(name); RandGuid(gg);
        if (LdmNewGroup(db, name, gg) != GPT_OK) { reason = MGPT_LDMR_FULL; goto out; }
    }
    RandGuid(dguid);
    if (LdmAddDisk(db, dguid, &di) != GPT_OK) { reason = MGPT_LDMR_FULL; goto out; }
    for (k = 0; k < (int)db->NDisks; k++) pub512[k] = (g && k < (int)g->Db->NDisks) ? g->PubSize[k] * g->Bps[k] / 512 : 0;
    pub512[di] = pubSize;
    if (!ReleaseBasicVolumes(dn, disk, bps, letters)) { reason = MGPT_LDMR_INUSE; goto out; }
    s = (LDM_NEWVOL *)ExAllocatePool(NonPagedPool, sizeof(LDM_NEWVOL));
    if (!s) { reason = MGPT_LDMR_WRITE; goto out; }
    for (i = 0; i < disk->Count; i++) {
        GPT_PARTITION *p = &disk->Part[i];
        if (p->MbrType == GPT_MBRTYPE_LDM || p->FsKind == GPT_FS_MSR || p->MbrType == GPT_MBRTYPE_EFISYS) continue;
        MemZero(s, sizeof(*s));
        s->Type = LDM_VT_SIMPLE; s->NDisks = 1; s->Disk[0] = (gpt_u32)di; s->Size[0] = p->SectorCount;
        s->Fixed = 1; s->Start[0] = p->StartLba - pubStart; s->Retain = isGpt ? 0 : 1;
        s->PartType = p->MbrType; RandGuid(s->Guid);
        if (letters[i]) { s->Hint[0] = (char)letters[i]; s->Hint[1] = ':'; }
        if (LdmAddVolume(db, s, pub512, &vi) != GPT_OK) { reason = MGPT_LDMR_FULL; goto out; }
    }

    /* the new table */
    if (!isGpt) {
        gpt_u64 lastEnd = 63; ULONG n = disk->Count;
        for (i = 0; i < n; i++) {
            GPT_PARTITION *p = &disk->Part[i];
            p->MbrType = GPT_MBRTYPE_LDM; p->FsKind = GPT_FS_LDMDATA; p->Recognized = 0; p->BootIndicator = 0;
            if (p->StartLba + p->SectorCount > lastEnd) lastEnd = p->StartLba + p->SectorCount;
        }
        if (lastEnd < pubStart + pubSize && n < 4) {
            GPT_PARTITION *p = &disk->Part[n];
            MemZero(p, sizeof(*p));
            p->StartLba = lastEnd; p->SectorCount = pubStart + pubSize - lastEnd;
            p->MbrType = GPT_MBRTYPE_LDM; p->FsKind = GPT_FS_LDMDATA;
            disk->Count++;
        }
    } else {
        ULONG n = 0;
        for (i = 0; i < disk->Count; i++) {
            GPT_PARTITION *p = &disk->Part[i];
            if (p->FsKind == GPT_FS_MSR || p->FsKind == GPT_FS_LDMMETA || p->MbrType == GPT_MBRTYPE_EFISYS)
                { if (n != i) memcpy(&disk->Part[n], p, sizeof(*p)); n++; }
        }
        disk->Count = n;
        MemZero(&disk->Part[n], sizeof(GPT_PARTITION));
        disk->Part[n].StartLba = pubStart; disk->Part[n].SectorCount = pubSize;
        memcpy(disk->Part[n].TypeGuid, LdmGuidData, 16); RandGuid(disk->Part[n].UniqueGuid);
        disk->Part[n].MbrType = GPT_MBRTYPE_LDM; disk->Part[n].FsKind = GPT_FS_LDMDATA; disk->Part[n].IsGptEntry = 1;
        SetGptName(&disk->Part[n], "LDM data partition");
        disk->Count++;
    }

    h = (LDM_HDR *)ExAllocatePool(NonPagedPool, sizeof(LDM_HDR));
    if (!h) { reason = MGPT_LDMR_WRITE; goto out; }
    LdmMakeHeader(h, isGpt, 512, pubStart, pubSize, privStart, 2048, dguid, db);
    if (LdmCommit(g, db, -1, dn, h)) { reason = MGPT_LDMR_WRITE; goto out; }
    if (!NT_SUCCESS(OpenPartition0(dn, &f, &p0))) { reason = MGPT_LDMR_WRITE; goto out; }
    ctx.Part0 = p0; ctx.BytesPerSector = bps; ctx.Limit = 0;
    k = GptWriteDisk(EnumRead, EnumWrite, &ctx, disk);
    TellDiskDriver(p0);
    ObDereferenceObject(f);
    reason = (k == GPT_OK) ? MGPT_LDMR_OK : MGPT_LDMR_WRITE;
out:
    if (h) ExFreePool(h);
    if (s) ExFreePool(s);
    LdmDropWork(db);
    ExFreePool(disk);
    return reason;
}

static ULONG ToBasic(ULONG dn)
{
    GPT_DISK *disk; ULONG bps, i, reason = MGPT_LDMR_PARAM; LDMGRP *g; LDM_DB *db = NULL; int k, di = -1;
    PFILE_OBJECT f; PDEVICE_OBJECT p0; ENUMCTX ctx; LDM_HDR *h = NULL;
    disk = LoadDisk(dn, &bps);
    if (!disk) return MGPT_LDMR_PARAM;
    if (!IsDynamicTable(disk)) { reason = MGPT_LDMR_SAME; goto out; }
    k = LdmGroupOf(dn, &di);
    if (k >= 0) {
        g = g_grp[k];
        if (g->Native) { reason = MGPT_LDMR_NATIVE; goto out; }
        for (i = 0; i < g->Db->NParts; i++) if (g->Db->Part[i].DiskId == g->Db->Disk[di].Id) { reason = MGPT_LDMR_NOTEMPTY; goto out; }
        if (g->Db->NDisks > 1) {
            if (!AllPresent(g)) { reason = MGPT_LDMR_MISSING; goto out; }
            db = LdmLoadWork(g);
            if (!db) { reason = MGPT_LDMR_WRITE; goto out; }
            for (i = 0; i < db->NDisks; i++) if (MemSame(db->Disk[i].Guid, g->Db->Disk[di].Guid, 16)) break;
            if (i < db->NDisks) {
                if (LdmRemoveDisk(db, (int)i) != GPT_OK) { reason = MGPT_LDMR_NOTEMPTY; goto out; }
                if (LdmCommit(g, db, di, 0, NULL)) { reason = MGPT_LDMR_WRITE; goto out; }
            }
        }
        h = g->Hdr[di];
    }
    if (!NT_SUCCESS(OpenPartition0(dn, &f, &p0))) { reason = MGPT_LDMR_WRITE; goto out; }
    ctx.Part0 = p0; ctx.BytesPerSector = bps; ctx.Limit = 0;
    ctx.Limit = disk->DiskSectors;
    if (h) LdmWipeHeader(EnumWrite, &ctx, h);
    ctx.Limit = 0;
    if (!disk->IsGpt) {
        disk->Count = 0;
    } else {
        ULONG n = 0; int msr = -1; gpt_u64 ms = 0, mc = 0;
        for (i = 0; i < disk->Count; i++) {
            GPT_PARTITION *p = &disk->Part[i];
            if (p->FsKind == GPT_FS_LDMMETA) { ms = p->StartLba; mc = p->SectorCount; continue; }
            if (p->MbrType == GPT_MBRTYPE_LDM) continue;
            if (n != i) memcpy(&disk->Part[n], p, sizeof(*p));
            if (disk->Part[n].FsKind == GPT_FS_MSR) msr = (int)n;
            n++;
        }
        disk->Count = n;
        /* the reserved partition gets its megabyte back */
        if (msr >= 0 && mc && disk->Part[msr].StartLba + disk->Part[msr].SectorCount == ms) disk->Part[msr].SectorCount += mc;
    }
    k = GptWriteDisk(EnumRead, EnumWrite, &ctx, disk);
    TellDiskDriver(p0);
    ObDereferenceObject(f);
    reason = (k == GPT_OK) ? MGPT_LDMR_OK : MGPT_LDMR_WRITE;
out:
    LdmDropWork(db);
    ExFreePool(disk);
    return reason;
}

static NTSTATUS DoDynamic(PVOID sys, ULONG inlen, ULONG *info)
{
    MGPT_DYNAMIC *r = (MGPT_DYNAMIC *)sys;
    if (inlen < sizeof(MGPT_DYNAMIC)) return STATUS_INVALID_PARAMETER;
    *info = sizeof(MGPT_DYNAMIC);
    LdmLock();
    r->Reason = r->ToDynamic ? ToDynamic(r->DiskNumber) : ToBasic(r->DiskNumber);
    if (r->DiskNumber < MGPT_MAXDISK) ProbeDisk(r->DiskNumber, &g_known[r->DiskNumber]);
    if (r->Reason == MGPT_LDMR_OK || r->Reason == MGPT_LDMR_INUSE) SyncDiskInSystem(r->DiskNumber);
    LdmUnlock();
    return STATUS_SUCCESS;
}

static NTSTATUS ControlIoctl(PIRP Irp, ULONG Code, PVOID Sys, ULONG InLen, ULONG OutLen)
{
    NTSTATUS st; ULONG info = 0;
    switch (Code) {
    case IOCTL_MGPT_LIST:      st = DoList(Sys, OutLen, &info); break;
    case IOCTL_MGPT_GETLAYOUT: st = DoGetLayout(Sys, InLen, OutLen, &info); break;
    case IOCTL_MGPT_SETLAYOUT: st = DoSetLayout(Sys, InLen); break;
    case IOCTL_MGPT_CREATE:    st = DoCreate(Sys, InLen); break;
    case IOCTL_MGPT_DELETE:    st = DoDelete(Sys, InLen); break;
    case IOCTL_MGPT_INIT:      st = DoInit(Sys, InLen); break;
    case IOCTL_MGPT_SETTYPE:   st = DoSetType(Sys, InLen); break;
    case IOCTL_MGPT_SETACTIVE: st = DoSetActive(Sys, InLen); break;
    case IOCTL_MGPT_SETLETTER: st = DoSetLetter(Sys, InLen); break;
    case IOCTL_MGPT_CONVERT:   st = DoConvert(Sys, InLen, &info); break;
    case IOCTL_MGPT_LDMINFO:   st = DoLdmInfo(Sys, OutLen, &info); break;
    case IOCTL_MGPT_LDMCREATE: st = DoLdmCreate(Sys, InLen, &info); break;
    case IOCTL_MGPT_LDMDELETE: st = DoLdmDelete(Sys, InLen, &info); break;
    case IOCTL_MGPT_LDMLETTER: st = DoLdmLetter(Sys, InLen, &info); break;
    case IOCTL_MGPT_DYNAMIC:   st = DoDynamic(Sys, InLen, &info); break;
    case IOCTL_MGPT_RESCAN:
    case IOCTL_MGPT_GETGEN:
        if (OutLen < sizeof(ULONG)) { st = STATUS_BUFFER_TOO_SMALL; break; }
        if (Code == IOCTL_MGPT_RESCAN) RescanNow();
        *(ULONG *)Sys = g_Gen; info = sizeof(ULONG); st = STATUS_SUCCESS;
        break;
    default:                   st = STATUS_INVALID_DEVICE_REQUEST; break;
    }
    /* a letter change (or a failed write) is a change for the tools too */
    if (Code == IOCTL_MGPT_SETLETTER || Code == IOCTL_MGPT_SETTYPE || Code == IOCTL_MGPT_SETACTIVE ||
        Code == IOCTL_MGPT_LDMLETTER) g_Gen++;
    return Complete(Irp, st, info);
}


/* Publishes the volumes at start-up (once the class drivers have created
   their devices), then checks the disks every 2 s, or at once when a tool
   asks (IOCTL_MGPT_RESCAN). Never ends: there is no unload. */
static VOID NTAPI EnumThread(PVOID Context)
{
    LARGE_INTEGER delay; (void)Context;
    delay.QuadPart = -20000000;
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
    D("MBRGPTNT enum thread\n");
    g_PollRunning = 1;
    for (;;) {
        CheckDisks();
        delay.QuadPart = -20000000;                   /* 2 s */
        KeWaitForSingleObject(&g_ScanReq, Executive, KernelMode, FALSE, &delay);
    }
}

static NTSTATUS CreateControlDevice(void)
{
    UNICODE_STRING uname, ulink; PDEVICE_OBJECT dev; PCTRL_EXT ext; NTSTATUS st;
    if (!NT_SUCCESS(MakeUnicode("\\Device\\MbrGptNt", &uname))) return STATUS_INSUFFICIENT_RESOURCES;
    st = IoCreateDevice(g_Driver, sizeof(CTRL_EXT), &uname, 0, 0, FALSE, &dev);
    RtlFreeUnicodeString(&uname);
    if (!NT_SUCCESS(st)) return st;
    ext = (PCTRL_EXT)dev->DeviceExtension; ext->Kind = KIND_CTRL;
    dev->Flags |= DO_BUFFERED_IO; dev->Flags &= ~DO_DEVICE_INITIALIZING;
    g_Control = dev;
    if (NT_SUCCESS(MakeUnicode("\\DosDevices\\MbrGptNt", &ulink))) {
        UNICODE_STRING ut;
        if (NT_SUCCESS(MakeUnicode("\\Device\\MbrGptNt", &ut))) { IoCreateSymbolicLink(&ulink, &ut); RtlFreeUnicodeString(&ut); }
        RtlFreeUnicodeString(&ulink);
    }
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegPath)
{
    HANDLE hThread; NTSTATUS st; (void)RegPath;
    D("MBRGPTNT DriverEntry\n");
    g_Driver = Driver; g_PartList = NULL; g_Control = NULL;
    KeInitializeEvent(&g_Lock, SynchronizationEvent, TRUE);
    KeInitializeEvent(&g_SyncLock, SynchronizationEvent, TRUE);
    KeInitializeEvent(&g_ScanLock, SynchronizationEvent, TRUE);
    KeInitializeEvent(&g_ScanReq, SynchronizationEvent, FALSE);
    KeInitializeEvent(&g_LdmLock, SynchronizationEvent, TRUE);
    KeInitializeEvent(&g_LdmIoLock, SynchronizationEvent, TRUE);
    KeInitializeEvent(&g_LdmScanBufLock, SynchronizationEvent, TRUE);
    Driver->MajorFunction[IRP_MJ_CREATE]         = (PVOID)DispCreateClose;
    Driver->MajorFunction[IRP_MJ_CLOSE]          = (PVOID)DispCreateClose;
    Driver->MajorFunction[IRP_MJ_READ]           = (PVOID)DispReadWrite;
    Driver->MajorFunction[IRP_MJ_WRITE]          = (PVOID)DispReadWrite;
    Driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = (PVOID)DispDeviceControl;
    Driver->MajorFunction[IRP_MJ_FLUSH_BUFFERS]  = (PVOID)DispPassThrough;
    Driver->MajorFunction[IRP_MJ_SHUTDOWN]       = (PVOID)DispPassThrough;
    CreateControlDevice();
    st = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, EnumThread, NULL);
    if (NT_SUCCESS(st)) ZwClose(hThread);
    return STATUS_SUCCESS;
}
