#include "Yes.h"
#include "Platform/WindowsWindowModule.h"

#include "Core/System.h"
#include "Misc/Debug.h"
#include "Misc/Thread.h"
#include "Misc/Sync.h"

#include "Windows.h"

namespace Yes
{
	class WindowsWindowModuleImp : public WindowsWindowModule
	{
	private:
		HWND mHwnd;
		Thread mThread;
		int mClientWidth;
		int mClientHeight;
		Semaphore<> mInitState;
	public:
		virtual void InitializeModule(System* system) override
		{
			mClientWidth = 800;
			mClientHeight = 600;
			mThread = Thread(WindowThreadFunction, this);
			mInitState.Wait();
		}
		virtual void* GetWindowHandle() override
		{
			return &mHwnd;
		}
		virtual void GetWindowRect(int& width, int& height) override
		{
			width = mClientWidth;
			height = mClientHeight;
		}
	private:
		static void WindowThreadFunction(void* s)
		{
			WindowsWindowModuleImp* self = (WindowsWindowModuleImp*)s;
			// Initialize the window class.
			WNDCLASSEX windowClass = { 0 };
			windowClass.cbSize = sizeof(WNDCLASSEX);
			windowClass.style = CS_HREDRAW | CS_VREDRAW;
			windowClass.lpfnWndProc = WindowProc;
			HINSTANCE hInstance = windowClass.hInstance = GetModuleHandle(nullptr);
			windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
			windowClass.lpszClassName = L"YesOnWindows";
			RegisterClassEx(&windowClass);

			RECT windowRect = { 0, 0, static_cast<LONG>(self->mClientWidth), static_cast<LONG>(self->mClientHeight) };
			AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

			// Create the window and store a handle to it.
			self->mHwnd = ::CreateWindow(
				windowClass.lpszClassName,
				L"Yes",
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				windowRect.right - windowRect.left,
				windowRect.bottom - windowRect.top,
				nullptr,        // We have no parent window.
				nullptr,        // We aren't using menus.
				hInstance,
				self);
			::ShowWindow(self->mHwnd, SW_SHOW);
			self->mInitState.Notify();
			MSG msg = {};
			while (msg.message != WM_QUIT)
			{
				// Process any messages in the queue.
				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}
		static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			WindowsWindowModuleImp* self = reinterpret_cast<WindowsWindowModuleImp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
			switch (message)
			{
			case WM_CREATE:
			{
				// Save the DXSample* passed in to CreateWindow.
				LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
				SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
				self = reinterpret_cast<WindowsWindowModuleImp*>(pCreateStruct->lpCreateParams);
				return 0;
			}
			case WM_KEYDOWN:
			{
				if (self)
				{
					return 0;
				}
			}
			case WM_KEYUP:
			if (self)
			{
				return 0;
			}
			case WM_DESTROY:
			{
				PostQuitMessage(0);
				return 0;
			}
			}

			// Handle any messages the switch statement didn't.
			return ::DefWindowProc(hWnd, message, wParam, lParam);
		}

		DEFINE_MODULE_IN_CLASS(WindowsWindowModule, WindowsWindowModuleImp);
	};
	DEFINE_MODULE_REGISTRY(WindowsWindowModule, WindowsWindowModuleImp, 250);
}