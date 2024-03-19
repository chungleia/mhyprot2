#include "pch.h"
#include "mhyprot.hpp"
#include "raw_driver.hpp"

#define SERVICE_NAME xorstr_("mhyprot2")
#define SERVICE_PATH xorstr_("5eef18bf-ce3d-4443-9528-09919afa348e")
#define SERVICE_DIR xorstr_("Temp")
#define SERVICE_FILE xorstr_("mhyprot2.sys")
#define DEVICE_NAME xorstr_(R"(\\.\mhyprot2)")
#define MODULE_NAME xorstr_("mhyprot")
#define SYSFILE_NAME xorstr_("mhyprot.sys")

#define MHYPROT_IOCTL_INITIALIZE 0x80034000
#define MHYPROT_IOCTL_READ_KERNEL_MEMORY 0x83064000
#define MHYPROT_IOCTL_READ_WRITE_USER_MEMORY 0x81074000
#define MHYPROT_IOCTL_ENUM_PROCESS_MODULES 0x82054000
#define MHYPROT_IOCTL_GET_SYSTEM_UPTIME 0x80134000
#define MHYPROT_IOCTL_ENUM_PROCESS_THREADS 0x83024000
#define MHYPROT_IOCTL_TERMINATE_PROCESS 0x81034000
#define MHYPROT_OFFSET_SEEDMAP 0xA0E8
#define MHYPROT_ACTION_READ 0x0
#define MHYPROT_ACTION_WRITE 0x1
#define MHYPROT_ENUM_PROCESS_MODULE_SIZE 0x3A0
#define MHYPROT_ENUM_PROCESS_THREADS_SIZE 0xA8
#define MHYPROT_ENUM_PROCESS_THREADS_CODE 0x88

// Function prototypes with consistent naming conventions
static bool DriverIsExist();
static bool DriverIsStart();
static void DriverStart();
static void DriverStop();
static void DriverDelete();
static void DriverCreate();

struct Mhyprot {
    Mhyprot();
    ~Mhyprot();
    static void CreateInstance();
};

typedef struct _MHYPROT_INITIALIZE {
    DWORD _m_001;
    DWORD _m_002;
    DWORD64 _m_003;
} MHYPROT_INITIALIZE, * PMHYPROT_INITIALIZE;

typedef struct _MHYPROT_KERNEL_READ_REQUEST {
    DWORD64 address;
    ULONG size;
} MHYPROT_KERNEL_READ_REQUEST, * PMHYPROT_KERNEL_READ_REQUEST;

typedef struct _MHYPROT_USER_READ_WRITE_REQUEST {
    DWORD64 response;
    DWORD action_code;
    DWORD reserved_01;
    DWORD process_id;
    DWORD reserved_02;
    DWORD64 buffer;
    DWORD64 address;
    ULONG size;
    ULONG reverved_03;
} MHYPROT_USER_READ_WRITE_REQUEST, * PMHYPROT_USER_READ_WRITE_REQUEST;

typedef struct _MHYPROT_ENUM_PROCESS_THREADS_REQUEST {
    DWORD validation_code;
    DWORD process_id;
    DWORD owner_process_id;
} MHYPROT_ENUM_PROCESS_THREADS_REQUEST, * PMHYPROT_ENUM_PROCESS_THREADS_REQUEST;

typedef struct _MHYPROT_THREAD_INFORMATION {
    DWORD64 kernel_address;
    DWORD64 start_address;
    bool unknown;
} MHYPROT_THREAD_INFORMATION, * PMHYPROT_THREAD_INFORMATION;

typedef struct _MHYPROT_TERMINATE_PROCESS_REQUEST {
    DWORD64 response;
    DWORD process_id;
} MHYPROT_TERMINATE_PROCESS_REQUEST, * PMHYPROT_TERMINATE_PROCESS_REQUEST;

typedef struct _MHYPROT_ENUM_PROCESS_MODULES_REQUEST {
    uint32_t process_id;
    uint32_t max_count;
} MHYPROT_ENUM_PROCESS_MODULES_REQUEST, * PMHYPROT_ENUM_PROCESS_MODULES_REQUEST;

namespace mhyprot {
    static HANDLE GDrvHandle;
    static DWORD64 GSeedMap[312];
    static DWORD GProcessId;

    static void RequestIOCTL(DWORD Code, void* Buffer, DWORD Size);
}

Mhyprot::Mhyprot() {
    if (DriverIsExist() && !DriverIsStart()) DriverStart();
    
    auto device = CreateFileA(DEVICE_NAME, GENERIC_ALL, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, 0);
    
    if (device == INVALID_HANDLE_VALUE || device == NULL) THROW_WINAPI("CreateFile", "Fail");
    
    mhyprot::GDrvHandle = device;
}

Mhyprot::~Mhyprot() {
    if (DriverIsExist() && DriverIsStart()) DriverStop();
    
    if (mhyprot::GDrvHandle) {
        NtClose(mhyprot::GDrvHandle);
        mhyprot::GDrvHandle = NULL;
   }
}

void Mhyprot::CreateInstance() {
   static shared_ptr<Mhyprot> instance;

   if (instance) return;

   if (!(instance = make_shared<Mhyprot>())) throw bad_alloc();
}

bool mhyprot::Initialize() {
   try {
       Mhyprot::CreateInstance();
       MHYPROT_INITIALIZE initializer;
       initializer._m_002 = 0x0BAEBAEEC;
       initializer._m_003 = 0x0EBBAAEF4FFF89042;
       RequestIOCTL(MHYPROT_IOCTL_INITIALIZE, &initializer, sizeof(initializer));
       auto mhyprotAddress = FindSysmoduleAddressByName(SYSFILE_NAME);
       if (!mhyprotAddress) THROW_USER("FindSysmoduleAddressByName", "Not found");
       DWORD64 seedMapAddress = ReadKernelMemory<DWORD64>(mhyprotAddress + MHYPROT_OFFSET_SEEDMAP);
       if (!seedMapAddress) THROW_USER("ReadKernelMemory", "SeedMapAddress");
       if (!ReadKernelMemory(seedMapAddress, &GSeedMap, sizeof(GSeedMap))) THROW_USER("ReadKernelMemory", "SeedMap");
   } catch (const exception& e) {
       #ifdef _DEBUG
           OutputDebugStringA(e.what());
       #else
           UNREFERENCED_PARAMETER(e);
       #endif
       return false;
   }
   return true;
}

bool mhyprot::Install() {
   try {
       if (DriverIsExist()) {
           return true; /*if (DriverIsStart()) DriverStop(); DriverDelete();*/
       }
       DriverCreate();
   } catch (const exception& e) {
       #ifdef _DEBUG
           OutputDebugStringA(e.what());
       #else
           UNREFERENCED_PARAMETER(e);
       #endif
       return false;
   }
   return true;
}

void mhyprot::Uninstall() {
   try {
       if (!DriverIsExist()) return;

       if (DriverIsStart()) DriverStop();

       if (GDrvHandle) {
           NtClose(GDrvHandle);
           GDrvHandle = NULL; 
       }

       DriverDelete();
   } catch (const exception& e) {
       #ifdef _DEBUG 
           OutputDebugStringA(e.what());
       #else 
           UNREFERENCED_PARAMETER(e); 
       #endif 
   }
}

void mhyprot::RequestIOCTL(DWORD Code, void* Buffer, DWORD Size) {
   void* outBuffer = calloc(1, Size);
   DWORD outBufferSize = 0;

   if (!outBuffer) throw bad_alloc();

   if (!DeviceIoControl(GDrvHandle, Code, Buffer, Size, outBuffer, Size, &outBufferSize, NULL)) {
      free(outBuffer);
      THROW_WINAPI("
