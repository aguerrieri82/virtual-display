#include "pch.h"
#include "VirtualDisplayManager.h"

using namespace System::Runtime::InteropServices;


static VOID WINAPI OnDeviceCreated(
    _In_ HSWDEVICE hSwDevice,
    _In_ HRESULT hrCreateResult,
    _In_opt_ PVOID pContext,
    _In_opt_ PCWSTR pszDeviceInstanceId
)
{
    GCHandle handle = GCHandle::FromIntPtr(static_cast<IntPtr>(pContext));

    auto device = static_cast<VirtualDisplay::VirtualDisplayManager^>(handle.Target);

    device->_instanceName = msclr::interop::marshal_as<String^>(pszDeviceInstanceId);

    SetEvent(device->_hCreateEvent);
}


namespace VirtualDisplay {

    VirtualDisplayManager::~VirtualDisplayManager()
    {
        if (_hDevice != nullptr)
            CloseHandle(_hDevice);

        if (_hswDevice != nullptr)
            SwDeviceClose(_hswDevice);

        _hDevice = nullptr;
        _hswDevice = nullptr;
    }

    void VirtualDisplayManager::SetDisplayModes(array<DisplayMode^>^ modes)
    {
        auto size = sizeof(SetMonitorModeReq) + sizeof(MonitorMode) * modes->Length;

        SetMonitorModeReq* req = (SetMonitorModeReq*)new byte[size];

        req->count = modes->Length;

        for (int i = 0; i < req->count; i++) {
            req->modes[i].height = modes[i]->Height;
            req->modes[i].width = modes[i]->Width;
            req->modes[i].vSync = modes[i]->VSync;
        }

        BOOL ioRes = DeviceIoControl(_hDevice, SET_MONITOR_MODES_REQ, req, size, NULL, 0, NULL, NULL);

        delete[] req;

        if (!ioRes)
            throw gcnew System::ComponentModel::Win32Exception(GetLastError());
    }

    void VirtualDisplayManager::CreateDisplay(int portIndex, int width, int height)
    {
        UpdateDisplayReq req{};
        req.portIndex = portIndex;
        req.connected = true;
        req.width = width;
        req.height = height;

        BOOL ioRes = DeviceIoControl(_hDevice, UPDATE_DISPLAY_REQ, &req, sizeof(req), NULL, 0, NULL, NULL);

        if (!ioRes)
            throw gcnew System::ComponentModel::Win32Exception(GetLastError());

        ChangeDisplayMode(portIndex, width, height);
    }

    bool VirtualDisplayManager::ChangeDisplayMode(int portIndex, int width, int height)
    {
        DISPLAY_DEVICE dev = {};
        dev.cb = sizeof(DISPLAY_DEVICE);
        int i = 0;
        int pi = 0;
        bool found = false;
        while (true) {

            if (!EnumDisplayDevices(NULL, i, &dev, 0))
                break;
            if (wcscmp(L"Virtual Display", dev.DeviceString) == 0) {
                if (pi == portIndex)
                {
                    found = true;
                    break;
                }
                pi++;
            }
            i++;
        }
        if (found) {

            DEVMODE dm = {};
            dm.dmSize = sizeof(dm);

            if (EnumDisplaySettings(dev.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {

                dm.dmPelsWidth = width;
                dm.dmPelsHeight = height;
                LONG result = ChangeDisplaySettingsEx(dev.DeviceName , &dm, NULL, 0, NULL);
                return result == DISP_CHANGE_SUCCESSFUL;
            }
        }
        return false;
    }

    void VirtualDisplayManager::DestroyDisplay(int portIndex)
    {
        UpdateDisplayReq req{};
        req.portIndex = portIndex;
        req.connected = false;

        BOOL ioRes = DeviceIoControl(_hDevice, UPDATE_DISPLAY_REQ, &req, sizeof(req), NULL, 0, NULL, NULL);

        if (!ioRes)
            throw gcnew System::ComponentModel::Win32Exception(GetLastError());
    }

    void VirtualDisplayManager::SetTargetSurface(int displayIndex, IntPtr handle)
    {
        SurfaceOpRequest req{};

        req.operation = SurfaceOpeation::CopyToSurface;
        req.hSharedSurface = handle.ToPointer();

        BOOL ioRes = DeviceIoControl(_hDevice, SURFACE_OP_REQ, &req, sizeof(req), NULL, 0, NULL, NULL);

        if (!ioRes)
            throw gcnew System::ComponentModel::Win32Exception(GetLastError());

    }

    DisplayInfo^ VirtualDisplayManager::GetDisplayInfo(int displayIndex)
    {
        DisplayInfoRequest req {};
        DisplayInfoNative resp {};
        req.monitorId = displayIndex;

        BOOL ioRes = DeviceIoControl(_hDevice, DISPLAY_INFO_REQ, &req, sizeof(req), &resp, sizeof(resp), NULL, NULL);

        if (!ioRes)
            throw gcnew System::ComponentModel::Win32Exception(GetLastError());
        
        DisplayInfo^ result = gcnew DisplayInfo();

        result->Width = resp.width;
        result->Height = resp.height;
        result->Luid = resp.luid;

        return result;
    }

    void VirtualDisplayManager::Create()
    {
        HSWDEVICE hSwDevice;
        SW_DEVICE_CREATE_INFO createInfo = { 0 };

        PCWSTR description = L"Virtual Display";
        PCWSTR instanceId = L"VirtualDisplay";
        PCWSTR hardwareIds = L"VirtualDisplay\0\0";
        PCWSTR compatibleIds = L"VirtualDisplay\0\0";

        createInfo.cbSize = sizeof(createInfo);
        createInfo.pszzCompatibleIds = compatibleIds;
        createInfo.pszInstanceId = instanceId;
        createInfo.pszzHardwareIds = hardwareIds;
        createInfo.pszDeviceDescription = description;

        createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
            SWDeviceCapabilitiesSilentInstall |
            SWDeviceCapabilitiesDriverRequired;

        auto handle = GCHandle::Alloc(this);

        

        _hCreateEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        HRESULT hr = SwDeviceCreate(L"VirtualDisplay",
            L"HTREE\\ROOT\\0",
            &createInfo,
            0,
            nullptr,
            OnDeviceCreated,
            GCHandle::ToIntPtr(handle).ToPointer(),
            &hSwDevice);

        if (FAILED(hr))
            throw gcnew System::ComponentModel::Win32Exception(hr);

        _hswDevice = hSwDevice;

        DWORD waitResult = WaitForSingleObject(_hCreateEvent, 10 * 1000);

        CloseHandle(_hCreateEvent);

        _hCreateEvent = NULL;

        if (waitResult != WAIT_OBJECT_0)
            throw gcnew System::TimeoutException();

        DEVINST hInst = NULL;
        DEVINST dnDevInst;
        DEVPROPTYPE propertyType;

        pin_ptr<const wchar_t> instanceName = PtrToStringChars(_instanceName);


        CM_Locate_DevNodeW(&dnDevInst, (wchar_t*)instanceName, CM_LOCATE_DEVNODE_NORMAL);

        wchar_t podName[256];
        size_t size = sizeof(podName);

        CM_Get_DevNode_Property(dnDevInst, &DEVPKEY_Device_PDOName, &propertyType, (PBYTE)podName, (PULONG)&size, 0);

        std::wstring devicePath = L"\\\\?\\global\\globalroot";

        devicePath += podName;

        int attempt = 0;

        while (attempt < 10) {

            _hDevice = CreateFile(devicePath.c_str(),
                0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (_hDevice != INVALID_HANDLE_VALUE)
                break;

            attempt++;

            Sleep(100);
        }

        if (_hDevice == INVALID_HANDLE_VALUE)
            throw gcnew System::ComponentModel::Win32Exception(GetLastError());
    }

}