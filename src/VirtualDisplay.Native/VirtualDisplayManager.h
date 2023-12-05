#pragma once


namespace VirtualDisplay {

	public ref struct DisplayInfo {
		property int Width;
		property int Height;
		property long Luid;
	};

	public ref struct DisplayMode {
		property int Width;
		property int Height;
		property int VSync;
	};

	public ref class VirtualDisplayManager : IDisposable
	{
	public:
		~VirtualDisplayManager();

		bool ChangeDisplayMode(int portIndex, int width, int height);


		void SetDisplayModes(array<DisplayMode^>^ modes);

		void CreateDisplay(int portIndex, int width, int height);

		void DestroyDisplay(int portIndex);

		void SetTargetSurface(int displayIndex, IntPtr handle);

		DisplayInfo^ GetDisplayInfo(int displayIndex);

		void Create();

	protected:
		HSWDEVICE _hswDevice = NULL;
		HANDLE _hDevice = NULL;
	internal:
		HANDLE _hCreateEvent = NULL;
		String^ _instanceName;
	};


}
