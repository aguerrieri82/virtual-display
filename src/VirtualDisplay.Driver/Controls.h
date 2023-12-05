#pragma once


#define DISPLAY_INFO_REQ CTL_CODE(FILE_DEVICE_SCREEN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define SURFACE_OP_REQ   CTL_CODE(FILE_DEVICE_SCREEN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define UPDATE_DISPLAY_REQ CTL_CODE(FILE_DEVICE_SCREEN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define SET_MONITOR_MODES_REQ CTL_CODE(FILE_DEVICE_SCREEN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)


struct DisplayInfoRequest {
    int portIndex;
};

struct DisplayInfo {
    int width;
    int height;
    int64_t luid;
};


enum SurfaceOpeation {
    None,
    CopyToSurface,
    CopyToMemory
};

struct SurfaceOpRequest {

    int portIndex;
    SurfaceOpeation operation;
    HANDLE hNewFrame;
    HANDLE hSharedSurface;
};


struct UpdateDisplayReq {
    int portIndex;
    int width;
    int height;
    bool connected;
};

struct MonitorMode {
    DWORD width;
    DWORD height;
    DWORD vSync;
};


struct SetMonitorModeReq {
    size_t count;
    MonitorMode modes[1];
};