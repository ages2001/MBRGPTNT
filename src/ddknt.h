/* ddknt.h - minimal NT DDK subset for the MBRGPTNT driver: NT 3.1 - 2000 /
   XP on x86, and XP / Server 2003 x64 (the _WIN64 layouts). */

#ifndef DDKNT_H
#define DDKNT_H

typedef unsigned char   UCHAR;
typedef unsigned short  USHORT;
typedef unsigned long   ULONG;
typedef unsigned int    UINT;
typedef char            CHAR;
typedef char            CCHAR;
typedef short           SHORT;
typedef long            LONG;
typedef unsigned char   BOOLEAN;
typedef void            VOID;
typedef void           *PVOID;
typedef char           *PCHAR;
typedef UCHAR          *PUCHAR;
typedef USHORT         *PUSHORT;
typedef ULONG          *PULONG;
typedef long            NTSTATUS;
typedef USHORT          WCHAR;
typedef WCHAR          *PWSTR;
typedef const WCHAR    *PCWSTR;
#if defined(_WIN64)
typedef unsigned long long ULONG_PTR;
typedef long long          LONG_PTR;
#define POINTER_ALIGNMENT  __attribute__((aligned(8)))
#else
typedef unsigned long      ULONG_PTR;
typedef long               LONG_PTR;
#define POINTER_ALIGNMENT
#endif
typedef ULONG_PTR       SIZE_T;

#define IN
#define OUT
#define OPTIONAL
#define NTAPI    __stdcall
#define TRUE  1
#define FALSE 0
#ifndef NULL
#define NULL ((void *)0)
#endif

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#define STATUS_NOT_SUPPORTED             ((NTSTATUS)0xC00000BBL)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#define STATUS_INVALID_DEVICE_REQUEST    ((NTSTATUS)0xC0000010L)
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)
#define STATUS_INSUFFICIENT_RESOURCES    ((NTSTATUS)0xC000009AL)
#define STATUS_DEVICE_NOT_READY          ((NTSTATUS)0xC00000A3L)
#define STATUS_ACCESS_DENIED             ((NTSTATUS)0xC0000022L)
#define STATUS_END_OF_FILE               ((NTSTATUS)0xC0000011L)
#define STATUS_PENDING                   ((NTSTATUS)0x00000103L)
#define NT_SUCCESS(s)                    (((NTSTATUS)(s)) >= 0)

#if !defined(_WIN64)
#pragma pack(push, 4)
#endif

#if defined(_MSC_VER)
typedef __int64 LONGLONG;
#else
typedef long long LONGLONG;
#endif

typedef union _LARGE_INTEGER {
    struct { ULONG LowPart; LONG HighPart; } u;
    struct { ULONG LowPart; LONG HighPart; };
#if defined(_MSC_VER)
    __int64 QuadPart;
#else
    long long QuadPart;
#endif
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _LIST_ENTRY { struct _LIST_ENTRY *Flink, *Blink; } LIST_ENTRY, *PLIST_ENTRY;

typedef struct _STRING { USHORT Length; USHORT MaximumLength; PCHAR Buffer; }
    STRING, ANSI_STRING, *PANSI_STRING, *PSTRING;
typedef struct _UNICODE_STRING { USHORT Length; USHORT MaximumLength; PWSTR Buffer; }
    UNICODE_STRING, *PUNICODE_STRING;

typedef struct _IO_STATUS_BLOCK { union { NTSTATUS Status; PVOID Pointer; }; ULONG_PTR Information; } IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
#if defined(_WIN64)
typedef struct _KEVENT { ULONG_PTR Opaque[3]; } KEVENT, *PKEVENT;
#else
typedef struct _KEVENT { UCHAR Opaque[0x10]; } KEVENT, *PKEVENT;
#endif
typedef enum _EVENT_TYPE { NotificationEvent = 0, SynchronizationEvent = 1 } EVENT_TYPE;
typedef enum _KWAIT_REASON { Executive = 0 } KWAIT_REASON;
typedef CCHAR KPROCESSOR_MODE;
#define KernelMode 0

typedef struct _DRIVER_OBJECT  DRIVER_OBJECT,  *PDRIVER_OBJECT;
typedef struct _DEVICE_OBJECT  DEVICE_OBJECT,  *PDEVICE_OBJECT;
typedef struct _IRP            IRP,            *PIRP;
typedef struct _FILE_OBJECT    FILE_OBJECT,    *PFILE_OBJECT;
typedef struct _MDL            MDL,            *PMDL;

#if defined(_WIN64)
struct _DEVICE_OBJECT {                     /* XP / 2003 x64 */
    SHORT           Type;                   /* 0x00 */
    USHORT          Size;
    LONG            ReferenceCount;         /* 0x04 */
    PDRIVER_OBJECT  DriverObject;           /* 0x08 */
    PDEVICE_OBJECT  NextDevice;             /* 0x10 */
    PDEVICE_OBJECT  AttachedDevice;         /* 0x18 */
    PIRP            CurrentIrp;             /* 0x20 */
    PVOID           Timer;                  /* 0x28 */
    ULONG           Flags;                  /* 0x30 */
    ULONG           Characteristics;        /* 0x34 */
    PVOID           Vpb;                    /* 0x38 */
    PVOID           DeviceExtension;        /* 0x40 */
    ULONG           DeviceType;             /* 0x48 */
    CCHAR           StackSize;              /* 0x4C */
    ULONG_PTR       Queue[9];               /* 0x50 */
    ULONG           AlignmentRequirement;   /* 0x98 */
};
#else
struct _DEVICE_OBJECT {
    SHORT           Type;                   /* 0x00 */
    USHORT          Size;
    LONG            ReferenceCount;         /* 0x04 */
    PDRIVER_OBJECT  DriverObject;           /* 0x08 */
    PDEVICE_OBJECT  NextDevice;             /* 0x0C */
    PDEVICE_OBJECT  AttachedDevice;         /* 0x10 */
    PIRP            CurrentIrp;             /* 0x14 */
    PVOID           Timer;                  /* 0x18 */
    ULONG           Flags;                  /* 0x1C */
    ULONG           Characteristics;        /* 0x20 */
    PVOID           Vpb;                    /* 0x24 */
    PVOID           DeviceExtension;        /* 0x28 */
    ULONG           DeviceType;             /* 0x2C */
    CCHAR           StackSize;              /* 0x30 */
    UCHAR           Pad0[3];
    UCHAR           Queue[0x28];            /* 0x34 */
    ULONG           AlignmentRequirement;   /* 0x5C */
    UCHAR           DeviceQueue[0x14];      /* 0x60 */
    UCHAR           Dpc[0x20];              /* 0x74 */
    ULONG           ActiveThreadCount;      /* 0x94 */
    PVOID           SecurityDescriptor;     /* 0x98 */
    KEVENT          DeviceLock;             /* 0x9C */
    USHORT          SectorSize;             /* 0xAC */
    USHORT          Spare1;
    PVOID           DeviceObjectExtension;  /* 0xB0 */
    PVOID           Reserved;               /* 0xB4 */
};
#endif

#define DO_BUFFERED_IO       0x00000004
#define DO_EXCLUSIVE         0x00000008
#define DO_DIRECT_IO         0x00000010
#define DO_DEVICE_INITIALIZING 0x00000080

#define FILE_DEVICE_DISK            0x00000007
#define FILE_DEVICE_MASS_STORAGE    0x0000002d

struct _DRIVER_OBJECT {
    SHORT Type; SHORT Size;
    PDEVICE_OBJECT DeviceObject;
    ULONG Flags; PVOID DriverStart; ULONG DriverSize;
    PVOID DriverSection; PVOID DriverExtension;
    UNICODE_STRING DriverName; PVOID HardwareDatabase; PVOID FastIoDispatch;
    PVOID DriverInit; PVOID DriverStartIo; PVOID DriverUnload;
    PVOID MajorFunction[28];
};

#define IRP_MJ_CREATE                 0x00
#define IRP_MJ_CLOSE                  0x02
#define IRP_MJ_READ                   0x03
#define IRP_MJ_WRITE                  0x04
#define IRP_MJ_DEVICE_CONTROL         0x0e
#define IRP_MJ_SHUTDOWN               0x10
#define IRP_MJ_FLUSH_BUFFERS          0x09

typedef struct _IO_STACK_LOCATION {
    UCHAR  MajorFunction;
    UCHAR  MinorFunction;
    UCHAR  Flags;
    UCHAR  Control;
    union {
        struct { ULONG Length; ULONG POINTER_ALIGNMENT Key; LARGE_INTEGER ByteOffset; } ReadWrite;
        struct { ULONG OutputBufferLength; ULONG POINTER_ALIGNMENT InputBufferLength;
                 ULONG POINTER_ALIGNMENT IoControlCode; PVOID Type3InputBuffer; } DeviceIoControl;
        struct { PVOID Argument1, Argument2, Argument3, Argument4; } Others;
    } Parameters;
    PDEVICE_OBJECT DeviceObject;
    PFILE_OBJECT   FileObject;
    PVOID          CompletionRoutine;
    PVOID          Context;
} IO_STACK_LOCATION, *PIO_STACK_LOCATION;

struct _IRP {
    SHORT  Type; USHORT Size;
    PMDL   MdlAddress; ULONG Flags;
    union { PIRP MasterIrp; PVOID SystemBuffer; } AssociatedIrp;
    LIST_ENTRY ThreadListEntry;
    IO_STATUS_BLOCK IoStatus;
    KPROCESSOR_MODE RequestorMode;
    UCHAR  PendingReturned; CHAR StackCount; CHAR CurrentLocation;
    UCHAR  Cancel; UCHAR CancelIrql; CCHAR ApcEnvironment; UCHAR AllocationFlags;
    PIO_STATUS_BLOCK UserIosb; PKEVENT UserEvent;
    union { PVOID o[2]; LARGE_INTEGER AllocationSize; } Overlay;
    PVOID  CancelRoutine; PVOID UserBuffer;
    union {
        struct {
            PVOID  DeviceQueueEntry[4];         /* 0x40 (x64: 0x78) */
            PVOID  Thread;                      /* 0x50 */
            PVOID  AuxiliaryBuffer;             /* 0x54 */
            LIST_ENTRY ListEntry;               /* 0x58 */
            PIO_STACK_LOCATION CurrentStackLocation; /* 0x60 */
            PVOID  OriginalFileObject;          /* 0x64 */
        } Overlay;
        PVOID Pad[12];
    } Tail;
};

#define IoGetCurrentIrpStackLocation(Irp) ((PIO_STACK_LOCATION)((Irp)->Tail.Overlay.CurrentStackLocation))
#define IoGetNextIrpStackLocation(Irp)    ((PIO_STACK_LOCATION)((Irp)->Tail.Overlay.CurrentStackLocation) - 1)
#define IO_NO_INCREMENT 0

#define CTL_CODE(DevType,Func,Meth,Acc) (((DevType)<<16)|((Acc)<<14)|((Func)<<2)|(Meth))
#define METHOD_BUFFERED   0
#define FILE_ANY_ACCESS   0
#define FILE_READ_ACCESS  1
#define FILE_WRITE_ACCESS 2
#define IOCTL_DISK_BASE   FILE_DEVICE_DISK

#define IOCTL_DISK_GET_DRIVE_GEOMETRY CTL_CODE(IOCTL_DISK_BASE,0x0000,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_DISK_GET_PARTITION_INFO CTL_CODE(IOCTL_DISK_BASE,0x0001,METHOD_BUFFERED,FILE_READ_ACCESS)
#define IOCTL_DISK_SET_PARTITION_INFO CTL_CODE(IOCTL_DISK_BASE,0x0002,METHOD_BUFFERED,FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_DISK_GET_DRIVE_LAYOUT   CTL_CODE(IOCTL_DISK_BASE,0x0003,METHOD_BUFFERED,FILE_READ_ACCESS)
#define IOCTL_DISK_SET_DRIVE_LAYOUT   CTL_CODE(IOCTL_DISK_BASE,0x0004,METHOD_BUFFERED,FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_DISK_IS_WRITABLE        CTL_CODE(IOCTL_DISK_BASE,0x0009,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_DISK_VERIFY             CTL_CODE(IOCTL_DISK_BASE,0x0005,METHOD_BUFFERED,FILE_ANY_ACCESS)

typedef enum _MEDIA_TYPE { Unknown = 0, RemovableMedia = 11, FixedMedia = 12 } MEDIA_TYPE;
#define SL_OVERRIDE_VERIFY_VOLUME 0x02
typedef struct _DISK_GEOMETRY {
    LARGE_INTEGER Cylinders; MEDIA_TYPE MediaType;
    ULONG TracksPerCylinder; ULONG SectorsPerTrack; ULONG BytesPerSector;
} DISK_GEOMETRY, *PDISK_GEOMETRY;

typedef struct _PARTITION_INFORMATION {
    LARGE_INTEGER StartingOffset; LARGE_INTEGER PartitionLength;
    ULONG HiddenSectors; ULONG PartitionNumber;
    UCHAR PartitionType; BOOLEAN BootIndicator;
    BOOLEAN RecognizedPartition; BOOLEAN RewritePartition;
} PARTITION_INFORMATION, *PPARTITION_INFORMATION;

#if !defined(_WIN64)
#pragma pack(pop)
#endif

struct _MDL {
    struct _MDL *Next;
    SHORT  Size;
    SHORT  MdlFlags;
    PVOID  Process;
    PVOID  MappedSystemVa;
    PVOID  StartVa;
    ULONG  ByteCount;
    ULONG  ByteOffset;
};
#define MDL_MAPPED_TO_SYSTEM_VA     0x0001
#define MDL_SOURCE_IS_NONPAGED_POOL 0x0004
PVOID NTAPI MmMapLockedPages(PMDL, CCHAR);
#define MmGetSystemAddress(m) (((m)->MdlFlags & (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL)) ? \
                               (m)->MappedSystemVa : MmMapLockedPages((m), 0))
#if defined(_WIN64)
/* not exported on x64: read the shared user data page (KSYSTEM_TIME at 0x14) */
static __inline VOID KeQuerySystemTime(PLARGE_INTEGER t)
{
    volatile ULONG *st = (volatile ULONG *)(ULONG_PTR)0xFFFFF78000000014ULL;
    ULONG lo, hi;
    do { hi = st[1]; lo = st[0]; } while (hi != st[2]);
    t->QuadPart = ((LONGLONG)hi << 32) | lo;
}
#else
VOID NTAPI KeQuerySystemTime(PLARGE_INTEGER);
#endif
PIRP  NTAPI IoAllocateIrp(CCHAR StackSize, BOOLEAN ChargeQuota);
VOID  NTAPI IoFreeIrp(PIRP);
PMDL  NTAPI IoAllocateMdl(PVOID Va, ULONG Length, BOOLEAN Secondary, BOOLEAN ChargeQuota, PIRP Irp);
VOID  NTAPI IoFreeMdl(PMDL);
VOID  NTAPI MmBuildMdlForNonPagedPool(PMDL);
#if defined(_WIN64)
static __inline PVOID KeGetCurrentThread(void)
{ PVOID t; __asm__ __volatile__("movq %%gs:0x188, %0" : "=r"(t)); return t; }
#else
PVOID NTAPI KeGetCurrentThread(void);
#endif
#define SL_INVOKE_ON_CANCEL  0x20
#define SL_INVOKE_ON_SUCCESS 0x40
#define SL_INVOKE_ON_ERROR   0x80
#define STATUS_MORE_PROCESSING_REQUIRED ((NTSTATUS)0xC0000016L)
#define KEY_READ 0x20019
typedef struct _KEY_VALUE_PARTIAL_INFORMATION { ULONG TitleIndex, Type, DataLength; UCHAR Data[1]; } KEY_VALUE_PARTIAL_INFORMATION;
#define KeyValuePartialInformation 2

ULONG NTAPI RtlRandom(PULONG);
#define STATUS_MEDIA_WRITE_PROTECTED ((NTSTATUS)0xC00000A2L)
#define IOCTL_DISK_UPDATE_PROPERTIES CTL_CODE(IOCTL_DISK_BASE,0x0050,METHOD_BUFFERED,FILE_ANY_ACCESS)

NTSTATUS NTAPI IoCreateDevice(PDRIVER_OBJECT, ULONG, PUNICODE_STRING, ULONG, ULONG, BOOLEAN, PDEVICE_OBJECT *);
VOID     NTAPI IoDeleteDevice(PDEVICE_OBJECT);
NTSTATUS NTAPI IoCreateSymbolicLink(PUNICODE_STRING, PUNICODE_STRING);
NTSTATUS NTAPI IoDeleteSymbolicLink(PUNICODE_STRING);
NTSTATUS NTAPI IoGetDeviceObjectPointer(PUNICODE_STRING, ULONG, PFILE_OBJECT *, PDEVICE_OBJECT *);
NTSTATUS NTAPI IoCallDriver(PDEVICE_OBJECT, PIRP);
VOID     NTAPI IoCompleteRequest(PIRP, CCHAR);
PIRP     NTAPI IoBuildSynchronousFsdRequest(ULONG, PDEVICE_OBJECT, PVOID, ULONG, PLARGE_INTEGER, PKEVENT, PIO_STATUS_BLOCK);
PIRP     NTAPI IoBuildDeviceIoControlRequest(ULONG, PDEVICE_OBJECT, PVOID, ULONG, PVOID, ULONG, BOOLEAN, PKEVENT, PIO_STATUS_BLOCK);
PVOID    NTAPI ExAllocatePool(ULONG, SIZE_T);
VOID     NTAPI ExFreePool(PVOID);
VOID     NTAPI ObDereferenceObject(PVOID);
VOID     NTAPI KeInitializeEvent(PKEVENT, EVENT_TYPE, BOOLEAN);
LONG     NTAPI KeSetEvent(PKEVENT, LONG, BOOLEAN);
NTSTATUS NTAPI KeWaitForSingleObject(PVOID, KWAIT_REASON, KPROCESSOR_MODE, BOOLEAN, PLARGE_INTEGER);
VOID     NTAPI RtlInitUnicodeString(PUNICODE_STRING, PCWSTR);
VOID     NTAPI RtlInitAnsiString(PANSI_STRING, const char *);
NTSTATUS NTAPI RtlAnsiStringToUnicodeString(PUNICODE_STRING, PANSI_STRING, BOOLEAN);
VOID     NTAPI RtlFreeUnicodeString(PUNICODE_STRING);
int      __cdecl sprintf(char *, const char *, ...);

#define NonPagedPool 0
#define PagedPool    1

typedef PVOID HANDLE;
typedef HANDLE *PHANDLE;
typedef VOID (NTAPI *PKSTART_ROUTINE)(PVOID);
NTSTATUS NTAPI PsCreateSystemThread(PHANDLE, ULONG, PVOID, HANDLE, PVOID, PKSTART_ROUTINE, PVOID);
typedef VOID (NTAPI *PWORKER_THREAD_ROUTINE)(PVOID);
typedef struct _WORK_QUEUE_ITEM {
    LIST_ENTRY List; PWORKER_THREAD_ROUTINE WorkerRoutine; PVOID Parameter;
} WORK_QUEUE_ITEM, *PWORK_QUEUE_ITEM;
#define DelayedWorkQueue 1
VOID     NTAPI ExQueueWorkItem(PWORK_QUEUE_ITEM, ULONG);
VOID     NTAPI PsTerminateSystemThread(NTSTATUS);
NTSTATUS NTAPI ZwClose(HANDLE);
NTSTATUS NTAPI KeDelayExecutionThread(KPROCESSOR_MODE, BOOLEAN, PLARGE_INTEGER);
#define THREAD_ALL_ACCESS 0x1F03FF

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
#define OBJ_CASE_INSENSITIVE 0x00000040
#define SYMBOLIC_LINK_QUERY  0x0001
#define InitObjAttr(p,name) do { (p)->Length=sizeof(OBJECT_ATTRIBUTES); (p)->RootDirectory=NULL; \
    (p)->ObjectName=(name); (p)->Attributes=OBJ_CASE_INSENSITIVE; (p)->SecurityDescriptor=NULL; \
    (p)->SecurityQualityOfService=NULL; } while(0)
PDEVICE_OBJECT NTAPI IoGetRelatedDeviceObject(PFILE_OBJECT);
NTSTATUS NTAPI ObReferenceObjectByHandle(HANDLE, ULONG DesiredAccess, PVOID ObjectType, KPROCESSOR_MODE, PVOID *Object, PVOID HandleInfo);
#ifndef FILE_READ_DATA
#define FILE_READ_DATA  0x0001
#define FILE_WRITE_DATA 0x0002
#endif
#define IRP_MJ_FILE_SYSTEM_CONTROL 0x0D
#define IRP_MN_USER_FS_REQUEST     0x00
#define FSCTL_LOCK_VOLUME          0x00090018
#define FSCTL_DISMOUNT_VOLUME      0x00090020
NTSTATUS NTAPI ObQueryNameString(PVOID Object, PVOID NameInfo, ULONG Length, PULONG ReturnLength);
NTSTATUS NTAPI ZwOpenSymbolicLinkObject(PHANDLE, ULONG DesiredAccess, POBJECT_ATTRIBUTES);
NTSTATUS NTAPI ZwQuerySymbolicLinkObject(HANDLE, PUNICODE_STRING, PULONG);
NTSTATUS NTAPI ZwOpenFile(PHANDLE, ULONG DesiredAccess, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ULONG ShareAccess, ULONG OpenOptions);
NTSTATUS NTAPI ZwOpenKey(PHANDLE, ULONG DesiredAccess, POBJECT_ATTRIBUTES);
NTSTATUS NTAPI ZwQueryValueKey(HANDLE, PUNICODE_STRING, ULONG Class, PVOID Info, ULONG Length, PULONG ResultLength);
NTSTATUS NTAPI ZwDeviceIoControlFile(HANDLE, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK, ULONG IoControlCode, PVOID InBuf, ULONG InLen, PVOID OutBuf, ULONG OutLen);
#define FILE_SHARE_READ  1
#define FILE_SHARE_WRITE 2
#define FILE_READ_ATTRIBUTES 0x0080
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020
#define SYNCHRONIZE 0x00100000

/* the layouts the kernels expect */
#define DDK_OFF(t,f) __builtin_offsetof(t,f)
#if defined(_WIN64)
_Static_assert(DDK_OFF(IRP, MdlAddress) == 0x08, "irp");
_Static_assert(DDK_OFF(IRP, AssociatedIrp) == 0x18, "irp");
_Static_assert(DDK_OFF(IRP, IoStatus) == 0x30, "irp");
_Static_assert(DDK_OFF(IRP, RequestorMode) == 0x40, "irp");
_Static_assert(DDK_OFF(IRP, Tail.Overlay.Thread) == 0x98, "irp");
_Static_assert(DDK_OFF(IRP, Tail.Overlay.CurrentStackLocation) == 0xB8, "irp");
_Static_assert(sizeof(IO_STACK_LOCATION) == 0x48, "isl");
_Static_assert(DDK_OFF(IO_STACK_LOCATION, Parameters.ReadWrite.ByteOffset) == 0x18, "isl");
_Static_assert(DDK_OFF(IO_STACK_LOCATION, Parameters.DeviceIoControl.IoControlCode) == 0x18, "isl");
_Static_assert(DDK_OFF(IO_STACK_LOCATION, Parameters.DeviceIoControl.InputBufferLength) == 0x10, "isl");
_Static_assert(DDK_OFF(IO_STACK_LOCATION, DeviceObject) == 0x28, "isl");
_Static_assert(DDK_OFF(IO_STACK_LOCATION, FileObject) == 0x30, "isl");
_Static_assert(DDK_OFF(DEVICE_OBJECT, Flags) == 0x30, "do");
_Static_assert(DDK_OFF(DEVICE_OBJECT, Vpb) == 0x38, "do");
_Static_assert(DDK_OFF(DEVICE_OBJECT, DeviceExtension) == 0x40, "do");
_Static_assert(DDK_OFF(DEVICE_OBJECT, StackSize) == 0x4C, "do");
_Static_assert(DDK_OFF(DEVICE_OBJECT, AlignmentRequirement) == 0x98, "do");
_Static_assert(DDK_OFF(DRIVER_OBJECT, MajorFunction) == 0x70, "drv");
_Static_assert(DDK_OFF(MDL, MappedSystemVa) == 0x18, "mdl");
_Static_assert(sizeof(KEVENT) == 0x18, "kev");
_Static_assert(sizeof(WORK_QUEUE_ITEM) == 0x20, "wqi");
_Static_assert(sizeof(OBJECT_ATTRIBUTES) == 0x30, "oa");
#else
_Static_assert(DDK_OFF(IRP, IoStatus) == 0x18, "irp");
_Static_assert(DDK_OFF(IRP, RequestorMode) == 0x20, "irp");
_Static_assert(DDK_OFF(IRP, Tail.Overlay.Thread) == 0x50, "irp");
_Static_assert(DDK_OFF(IRP, Tail.Overlay.CurrentStackLocation) == 0x60, "irp");
_Static_assert(sizeof(IO_STACK_LOCATION) == 0x24, "isl");
_Static_assert(DDK_OFF(IO_STACK_LOCATION, Parameters.ReadWrite.ByteOffset) == 0x0C, "isl");
_Static_assert(DDK_OFF(IO_STACK_LOCATION, DeviceObject) == 0x14, "isl");
_Static_assert(DDK_OFF(DEVICE_OBJECT, StackSize) == 0x30, "do");
_Static_assert(DDK_OFF(DEVICE_OBJECT, AlignmentRequirement) == 0x5C, "do");
_Static_assert(DDK_OFF(DRIVER_OBJECT, MajorFunction) == 0x38, "drv");
_Static_assert(DDK_OFF(MDL, MappedSystemVa) == 0x0C, "mdl");
#endif

#endif /* DDKNT_H */
