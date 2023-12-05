#include "Driver.h"
#include <string>

using namespace std;
using namespace Microsoft::IndirectDisp;
using namespace Microsoft::WRL;

#pragma region SampleMonitors

std::vector<MonitorMode> monitorModes = {
    { 1920, 1080, 60 },
    { 1600,  900, 60 },
    { 1024,  768, 75 },
};


static byte* lastFrame = nullptr;

#pragma endregion

#pragma region helpers

static inline void FillSignalInfo(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, DWORD Width, DWORD Height, DWORD VSync, bool bMonitorMode)
{
    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;

    // See https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_video_signal_info
    Mode.AdditionalSignalInfo.vSyncFreqDivider = bMonitorMode ? 0 : 1;
    Mode.AdditionalSignalInfo.videoStandard = 255;

    Mode.vSyncFreq.Numerator = VSync;
    Mode.vSyncFreq.Denominator = 1;
    Mode.hSyncFreq.Numerator = VSync * Height;
    Mode.hSyncFreq.Denominator = 1;

    Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;

    Mode.pixelRate = ((UINT64) VSync) * ((UINT64) Width) * ((UINT64) Height);
}

static IDDCX_MONITOR_MODE CreateIddCxMonitorMode(DWORD Width, DWORD Height, DWORD VSync, IDDCX_MONITOR_MODE_ORIGIN Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER)
{
    IDDCX_MONITOR_MODE Mode = {};

    Mode.Size = sizeof(Mode);
    Mode.Origin = Origin;
    FillSignalInfo(Mode.MonitorVideoSignalInfo, Width, Height, VSync, true);

    return Mode;
}

static IDDCX_TARGET_MODE CreateIddCxTargetMode(DWORD Width, DWORD Height, DWORD VSync)
{
    IDDCX_TARGET_MODE Mode = {};

    Mode.Size = sizeof(Mode);
    FillSignalInfo(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync, false);

    return Mode;
}

#pragma endregion

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_IDD_CX_DEVICE_IO_CONTROL IddSampleIoDeviceControl;

EVT_WDF_DRIVER_DEVICE_ADD IddSampleDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY IddSampleDeviceD0Entry;

EVT_IDD_CX_ADAPTER_INIT_FINISHED IddSampleAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES IddSampleAdapterCommitModes;

EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES IddSampleMonitorGetDefaultModes;
EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION IddSampleParseMonitorDescription;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES IddSampleMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN IddSampleMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN IddSampleMonitorUnassignSwapChain;

struct IndirectDeviceContextWrapper
{
    IndirectDeviceContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

struct IndirectMonitorContextWrapper
{
    IndirectMonitorContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);

WDF_DECLARE_CONTEXT_TYPE(IndirectMonitorContextWrapper);

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hInstance,
    _In_ UINT dwReason,
    _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

HANDLE hLogPipe = nullptr;

void Log(std::string text) {

    if (hLogPipe == NULL) {

        hLogPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\VirtualMonitor"),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            1024 * 16,
            1024 * 16,
            NMPWAIT_USE_DEFAULT_WAIT,
            NULL);

        Sleep(2000);
    }

    DWORD written;

    WriteFile(hLogPipe, text.c_str(), (DWORD)text.length(), &written, NULL);
}


_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT  pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    WDF_DRIVER_CONFIG Config;
    NTSTATUS Status;

    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);

    WDF_DRIVER_CONFIG_INIT(&Config,
        IddSampleDeviceAdd
    );

    Status = WdfDriverCreate(pDriverObject, pRegistryPath, &Attributes, &Config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit)
{
    NTSTATUS Status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    // Register for power callbacks - in this sample only power-on is needed
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
    PnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);

    IDD_CX_CLIENT_CONFIG IddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);


    IddConfig.EvtIddCxDeviceIoControl = IddSampleIoDeviceControl;
    IddConfig.EvtIddCxParseMonitorDescription = IddSampleParseMonitorDescription;
    IddConfig.EvtIddCxAdapterInitFinished = IddSampleAdapterInitFinished;
    IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes = IddSampleMonitorGetDefaultModes;
    IddConfig.EvtIddCxMonitorQueryTargetModes = IddSampleMonitorQueryModes;
    IddConfig.EvtIddCxAdapterCommitModes = IddSampleAdapterCommitModes;
    IddConfig.EvtIddCxMonitorAssignSwapChain = IddSampleMonitorAssignSwapChain;
    IddConfig.EvtIddCxMonitorUnassignSwapChain = IddSampleMonitorUnassignSwapChain;

    Status = IddCxDeviceInitConfig(pDeviceInit, &IddConfig);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }


    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);
    Attr.EvtCleanupCallback = [](WDFOBJECT Object)
    {
        // Automatically cleanup the context when the WDF object is about to be deleted
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if (pContext)
        {
            pContext->Cleanup();
        }
    };

    WDFDEVICE Device = nullptr;
    Status = WdfDeviceCreate(&pDeviceInit, &Attr, &Device);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }



    Status = IddCxDeviceInitialize(Device);

    // Create a new device context object and attach it to the WDF device object
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext = new IndirectDeviceContext(Device);

    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(PreviousState);



    // This function is called by WDF to start the device in the fully-on power state.

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext->InitAdapter();

    return STATUS_SUCCESS;
}

#pragma region Direct3DDevice

Direct3DDevice::Direct3DDevice(LUID AdapterLuid) : AdapterLuid(AdapterLuid)
{

}

Direct3DDevice::Direct3DDevice()
{
    AdapterLuid = LUID{};
}

HRESULT Direct3DDevice::Init()
{
    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the specified render adapter
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if (FAILED(hr))
    {
        return hr;
    }

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext);
    if (FAILED(hr))
    {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        return hr;
    }

    return S_OK;
}

#pragma endregion

#pragma region SwapChainProcessor

SwapChainProcessor::SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, shared_ptr<Direct3DDevice> Device, HANDLE NewFrameEvent, IndirectMonitorContext* MonitorContext)
    : m_hSwapChain(hSwapChain), m_Device(Device), m_hAvailableBufferEvent(NewFrameEvent), m_Ctx(MonitorContext)
{
    m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));

    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    // Alert the swap-chain processing thread to terminate
    SetEvent(m_hTerminateEvent.Get());

    if (m_hThread.Get())
    {
        // Wait for the thread to terminate
        WaitForSingleObject(m_hThread.Get(), INFINITE);
    }
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID Argument)
{
    reinterpret_cast<SwapChainProcessor*>(Argument)->Run();
    return 0;
}

void SwapChainProcessor::Run()
{
    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    DWORD AvTask = 0;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristicsW(L"Distribution", &AvTask);

    RunCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    WdfObjectDelete((WDFOBJECT)m_hSwapChain);
    m_hSwapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
}

void SwapChainProcessor::RunCore()
{
    Log("Running\n");

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<ID3D10Texture2D> targetTexture;
    HANDLE hCurSharedSurface = NULL;

    HRESULT hr = m_Device->Device.As(&dxgiDevice);

    if (FAILED(hr))
        return;


    IDARG_IN_SWAPCHAINSETDEVICE SetDevice = {};
    SetDevice.pDevice = dxgiDevice.Get();

    hr = IddCxSwapChainSetDevice(m_hSwapChain, &SetDevice);
    if (FAILED(hr))
        return;

    // Acquire and release buffers in a loop
    for (;;)
    {
        ComPtr<IDXGIResource> AcquiredBuffer;

        // Ask for the next buffer from the producer
        IDARG_OUT_RELEASEANDACQUIREBUFFER Buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &Buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING)
        {
            // We must wait for a new buffer
            HANDLE WaitHandles [] =
            {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles), WaitHandles, FALSE, 16);
            if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT)
            {
                // We have a new buffer, so try the AcquireBuffer again
                continue;
            }
            else if (WaitResult == WAIT_OBJECT_0 + 1)
            {
                // We need to terminate
                break;
            }
            else
            {
                // The wait was cancelled or something unexpected happened
                hr = HRESULT_FROM_WIN32(WaitResult);
                break;
            }
        }
        else if (SUCCEEDED(hr))
        {
            // We have new frame to process, the surface has a reference on it that the driver has to release
            AcquiredBuffer.Attach(Buffer.MetaData.pSurface);


            ComPtr<IDXGISurface> surface;

            hr = Buffer.MetaData.pSurface->QueryInterface<IDXGISurface>(&surface);

            if (SUCCEEDED(hr)) {

                DXGI_SURFACE_DESC desc;
                hr = surface->GetDesc(&desc);

                m_Ctx->Info.width = desc.Width;
                m_Ctx->Info.height = desc.Height;
                m_Ctx->Info.luid = *(int64_t*)&m_Device->AdapterLuid;
            }

            if (m_Ctx->hNewFrame != nullptr) {
                SetEvent(m_Ctx->hNewFrame);
            }

            if (m_Ctx->hSharedSurface != nullptr) {

                Log("hSharedSurface Submitted\n");

                if (targetTexture.Get() == nullptr || m_Ctx->hSharedSurface != hCurSharedSurface) {
                    
                    hCurSharedSurface = m_Ctx->hSharedSurface;

                    hr = m_Device->Device->OpenSharedResource(hCurSharedSurface, __uuidof(ID3D11Texture2D), &targetTexture);

                    Log("surfaced opened: "); Log(std::to_string(hr)); Log("\n");
                }
            }

            if (targetTexture.Get() != NULL) {
                ComPtr<ID3D11DeviceContext> ctx;
                ComPtr<ID3D11Resource> src;
                ComPtr<ID3D11Resource> dst;

                hr = Buffer.MetaData.pSurface->QueryInterface<ID3D11Resource>(&src);

                hr = targetTexture->QueryInterface<ID3D11Resource>(&dst);

                m_Device->Device->GetImmediateContext(&ctx);

                ctx->CopyResource(dst.Get(), src.Get());
                ctx->Flush();
            }


            AcquiredBuffer.Reset();
            
            hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
            if (FAILED(hr))
            {
                break;
            }

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            //
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        }
        else
        {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            break;
        }
    }
}

#pragma endregion

#pragma region IndirectDeviceContext

IndirectDeviceContext::IndirectDeviceContext(_In_ WDFDEVICE WdfDevice) :
    m_WdfDevice(WdfDevice)
{
    m_Adapter = {};
}

IndirectDeviceContext::~IndirectDeviceContext()
{
}

void IndirectDeviceContext::InitAdapter()
{
    IDDCX_ADAPTER_CAPS AdapterCaps = {};
    AdapterCaps.Size = sizeof(AdapterCaps);

    // Declare basic feature support for the adapter (required)
    AdapterCaps.MaxMonitorsSupported = 5;
    AdapterCaps.EndPointDiagnostics.Size = sizeof(AdapterCaps.EndPointDiagnostics);
    AdapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    AdapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
    AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"Virtual Display Device";
    AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Eusoft";
    AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"Virtual Display Model";

    // Declare your hardware and firmware versions (required)
    IDDCX_ENDPOINT_VERSION Version = {};
    Version.Size = sizeof(Version);
    Version.MajorVer = 1;
    AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version;
    AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version;

    // Initialize a WDF context that can store a pointer to the device context object
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT AdapterInit = {};
    AdapterInit.WdfDevice = m_WdfDevice;
    AdapterInit.pCaps = &AdapterCaps;
    AdapterInit.ObjectAttributes = &Attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    IDARG_OUT_ADAPTER_INIT AdapterInitOut;
    NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

    if (NT_SUCCESS(Status))
    {

        // Store a reference to the WDF adapter handle
        m_Adapter = AdapterInitOut.AdapterObject;

        // Store the device context object into the WDF object context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterInitOut.AdapterObject);
        pContext->pContext = this;
    }
}

void IndirectDeviceContext::DisconnectMonitor(UINT ConnectorIndex) {
    if (ConnectorIndex > monitors.size() - 1)
        return;

    IddCxMonitorDeparture(monitors[ConnectorIndex]->Monitor);
}

void IndirectDeviceContext::ConnectMonitor(UINT ConnectorIndex)
{
    NTSTATUS Status;

    Log("ConnectMonitor: "); Log(std::to_string(ConnectorIndex)); Log("\n");

    if (ConnectorIndex >= monitors.size())
    {
        monitors.resize(ConnectorIndex + 1);

        Log("CreateMonitor: "); Log(std::to_string(monitors.size())); Log("\n");

        WDF_OBJECT_ATTRIBUTES Attr;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectMonitorContextWrapper);

        // In the sample driver, we report a monitor right away but a real driver would do this when a monitor connection event occurs
        IDDCX_MONITOR_INFO MonitorInfo = {};
        MonitorInfo.Size = sizeof(MonitorInfo);
        MonitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
        MonitorInfo.ConnectorIndex = ConnectorIndex;
        MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
        MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
        MonitorInfo.MonitorDescription.DataSize = 0;
        MonitorInfo.MonitorDescription.pData = nullptr;

        // Create a container ID
        CoCreateGuid(&MonitorInfo.MonitorContainerId);

        IDARG_IN_MONITORCREATE MonitorCreate = {};
        MonitorCreate.ObjectAttributes = &Attr;
        MonitorCreate.pMonitorInfo = &MonitorInfo;

        // Create a monitor object with the specified monitor descriptor
        IDARG_OUT_MONITORCREATE MonitorCreateOut;
        Status = IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);

        if (NT_SUCCESS(Status))
        {
            // Create a new monitor context object and attach it to the Idd monitor object
            auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorCreateOut.MonitorObject);
            pMonitorContextWrapper->pContext = new IndirectMonitorContext(MonitorCreateOut.MonitorObject);

            monitors[ConnectorIndex] = pMonitorContextWrapper->pContext;
        }
    }

    // Tell the OS that the monitor has been plugged in
    IDARG_OUT_MONITORARRIVAL ArrivalOut;
    Status = IddCxMonitorArrival(monitors[ConnectorIndex]->Monitor, &ArrivalOut);
}

IndirectMonitorContext::IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor) :
    Monitor(Monitor)
{
}

IndirectMonitorContext::~IndirectMonitorContext()
{
    m_ProcessingThread.reset();
}

void IndirectMonitorContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent)
{
    m_ProcessingThread.reset();

    auto Device = make_shared<Direct3DDevice>(RenderAdapter);
    if (FAILED(Device->Init()))
    {
        // It's important to delete the swap-chain if D3D initialization fails, so that the OS knows to generate a new
        // swap-chain and try again.
        WdfObjectDelete(SwapChain);
    }
    else
    {
        // Create a new swap-chain processing thread
        m_ProcessingThread.reset(new SwapChainProcessor(SwapChain, Device, NewFrameEvent, this));
    }
}

void IndirectMonitorContext::UnassignSwapChain()
{
    // Stop processing the last swap-chain
    m_ProcessingThread.reset();
}

#pragma endregion

#pragma region DDI Callbacks



_Use_decl_annotations_
void IddSampleIoDeviceControl(WDFDEVICE Device,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode) {

    Log("IddSampleIoDeviceControl: "); Log(std::to_string(IoControlCode)); Log("\n");

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);


    if (IoControlCode == DISPLAY_INFO_REQ) {

        DisplayInfoRequest* req = NULL;
        DisplayInfo* resp = NULL;
        size_t size = sizeof(DisplayInfoRequest);

        NTSTATUS st = WdfRequestRetrieveInputBuffer(Request, size, (PVOID*)&req, NULL);
        if (st == STATUS_SUCCESS) {
            st = WdfRequestRetrieveOutputBuffer(Request, sizeof(DisplayInfo), (PVOID*)&resp, NULL);
            if (st == STATUS_SUCCESS)
                *resp = pContext->pContext->monitors[req->portIndex]->Info;
        }

        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(DisplayInfo));

    }
    else if (IoControlCode == SURFACE_OP_REQ) {

        SurfaceOpRequest* req = NULL;
        size_t size = sizeof(SurfaceOpRequest);

        NTSTATUS st = WdfRequestRetrieveInputBuffer(Request, size, (PVOID*)&req, NULL);

        if (st == STATUS_SUCCESS) {

            pContext->pContext->monitors[req->portIndex]->Operation = req->operation;
            pContext->pContext->monitors[req->portIndex]->hSharedSurface = req->hSharedSurface;
            pContext->pContext->monitors[req->portIndex]->hNewFrame = req->hNewFrame;

            WdfRequestComplete(Request, STATUS_SUCCESS);
        }
    }
    else if (IoControlCode == UPDATE_DISPLAY_REQ) {
        UpdateDisplayReq* req = NULL;
        size_t size = sizeof(UpdateDisplayReq);

        NTSTATUS st = WdfRequestRetrieveInputBuffer(Request, size, (PVOID*)&req, NULL);

        if (st == STATUS_SUCCESS) {

            if (req->connected)
                pContext->pContext->ConnectMonitor(req->portIndex);
            else
                pContext->pContext->DisconnectMonitor(req->portIndex);

            WdfRequestComplete(Request, STATUS_SUCCESS);
        }
    }
    else if (IoControlCode == SET_MONITOR_MODES_REQ){

        SetMonitorModeReq* req = NULL;
        size_t size = sizeof(SetMonitorModeReq);

        NTSTATUS st = WdfRequestRetrieveInputBuffer(Request, size, (PVOID*)&req, &size);
        if (st == STATUS_SUCCESS) {
            monitorModes.clear();
            for (int i = 0; i < req->count; i++)
                monitorModes.push_back(req->modes[i]);

            WdfRequestComplete(Request, STATUS_SUCCESS);
        }
    }
    else
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);


}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterInitFinished(IDDCX_ADAPTER AdapterObject, const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{

    Log("IddSampleAdapterInitFinished\n");
    
    UNREFERENCED_PARAMETER(pInArgs);
    UNREFERENCED_PARAMETER(AdapterObject);
    /*
    auto* pDeviceContextWrapper = WdfObjectGet_IndirectDeviceContextWrapper(AdapterObject);

    if (NT_SUCCESS(pInArgs->AdapterInitStatus))
        pDeviceContextWrapper->pContext->ConnectMonitor(0);
    */
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterCommitModes(IDDCX_ADAPTER AdapterObject, const IDARG_IN_COMMITMODES* pInArgs)
{
    Log("IddSampleAdapterCommitModes\n");

    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // For the sample, do nothing when modes are picked - the swap-chain is taken care of by IddCx

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    // ==============================

    return STATUS_SUCCESS;
}


_Use_decl_annotations_
NTSTATUS IddSampleParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs, IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs)
{ 
    Log("IddSampleParseMonitorDescription\n");

    UNREFERENCED_PARAMETER(pInArgs);
    UNREFERENCED_PARAMETER(pOutArgs);

    return STATUS_INVALID_PARAMETER;
}



_Use_decl_annotations_
NTSTATUS IddSampleMonitorGetDefaultModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs, IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
    Log("IddSampleMonitorGetDefaultModes\n");

    UNREFERENCED_PARAMETER(MonitorObject);

    if (pInArgs->DefaultMonitorModeBufferInputCount == 0)
    {
        pOutArgs->DefaultMonitorModeBufferOutputCount = (UINT)monitorModes.size();
    }
    else
    {
        for (DWORD ModeIndex = 0; ModeIndex < monitorModes.size(); ModeIndex++)
        {
            pInArgs->pDefaultMonitorModes[ModeIndex] = CreateIddCxMonitorMode(
                monitorModes[ModeIndex].width,
                monitorModes[ModeIndex].height,
                monitorModes[ModeIndex].vSync,
                IDDCX_MONITOR_MODE_ORIGIN_DRIVER
            );
        }

        pOutArgs->DefaultMonitorModeBufferOutputCount = (UINT)monitorModes.size();
        pOutArgs->PreferredMonitorModeIdx = 0;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorQueryModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_QUERYTARGETMODES* pInArgs, IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
    Log("IddSampleMonitorQueryModes\n");

    UNREFERENCED_PARAMETER(MonitorObject);

    vector<IDDCX_TARGET_MODE> TargetModes;

    // Create a set of modes supported for frame processing and scan-out. These are typically not based on the
    // monitor's descriptor and instead are based on the static processing capability of the device. The OS will
    // report the available set of modes for a given output as the intersection of monitor modes with target modes.

    TargetModes.push_back(CreateIddCxTargetMode(3840, 2160, 60));
    TargetModes.push_back(CreateIddCxTargetMode(2560, 1440, 144));
    TargetModes.push_back(CreateIddCxTargetMode(2560, 1440, 90));
    TargetModes.push_back(CreateIddCxTargetMode(2560, 1440, 60));
    TargetModes.push_back(CreateIddCxTargetMode(1920, 1080, 144));
    TargetModes.push_back(CreateIddCxTargetMode(1920, 1080, 90));
    TargetModes.push_back(CreateIddCxTargetMode(1920, 1080, 60));
    TargetModes.push_back(CreateIddCxTargetMode(1600,  900, 60));
    TargetModes.push_back(CreateIddCxTargetMode(1024,  768, 75));
    TargetModes.push_back(CreateIddCxTargetMode(1024,  768, 60));

    pOutArgs->TargetModeBufferOutputCount = (UINT) TargetModes.size();

    if (pInArgs->TargetModeBufferInputCount >= TargetModes.size())
    {
        copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject, const IDARG_IN_SETSWAPCHAIN* pInArgs)
{

    Log("IddSampleMonitorAssignSwapChain\n");

    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
    pMonitorContextWrapper->pContext->AssignSwapChain(pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject)
{
    Log("IddSampleMonitorUnassignSwapChain\n");
    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
    pMonitorContextWrapper->pContext->UnassignSwapChain();
    return STATUS_SUCCESS;
}

#pragma endregion
