#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Platform/WindowsWindowModule.h"

#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Misc/Debug.h"
#include "Runtime/Public/Misc/Utils.h"
#include "Runtime/Public/Concurrency/Thread.h"
#include "Runtime/Public/Concurrency/Sync.h"
#include "Runtime/Public/Platform/InputState.h"

#include "Windows.h"
#include "windowsx.h"

namespace Yes
{
	static KeyCode Map2KeyCode(WPARAM wParam)
	{
		switch (wParam)
		{
		case 0x41:
			return KeyCode::Left;
		case 0x44:
			return KeyCode::Right;
		case 0x45:
			return KeyCode::Up;
		case 0x51:
			return KeyCode::Down;
		case 0x57:
			return KeyCode::Forward;
		case 0x53:
			return KeyCode::Backward;
		default:
			return KeyCode::Other;
		}
	}

	class WindowsWindowModuleImp : public WindowsWindowModule
	{
	private:
		HWND mHwnd;
		Thread mThread;
		int mClientWidth;
		int mClientHeight;
		InputState mOutputInputStates;
		InputState mInternalInputStates;
		int mFrameIndex;
		Semaphore<> mInitState;
	public:
		virtual void InitializeModule(System*) override
		{
			Yes::TickModule* tickModule = GET_MODULE(TickModule);
			tickModule->AddTickable(this);
			mClientWidth = 800;
			mClientHeight = 600;
			ZeroFill(mOutputInputStates);
			ZeroFill(mInternalInputStates);
			mThread.Run(WindowThreadFunction, this, L"WindowsPlatformUIThread");
			mInitState.Decrease();
		}
		void* GetWindowHandle() override
		{
			return &mHwnd;
		}
		void GetWindowRect(int& width, int& height) override
		{
			width = mClientWidth;
			height = mClientHeight;
		}
		const InputState* GetInputState() override
		{
			return &mOutputInputStates;
		}
		void Tick() override
		{
			InputState& ostate = mOutputInputStates;
			int lastX = ostate.AbsoluteMousePosition[0];
			int lastY = ostate.AbsoluteMousePosition[1];
			ostate = mInternalInputStates;
			ostate.DeltaAbsoluteMousePosition[0] = ostate.AbsoluteMousePosition[0] - lastX;
			ostate.DeltaAbsoluteMousePosition[1] = ostate.AbsoluteMousePosition[1] - lastY;
			ostate.DeltaNormalizedMousePosition[0] = ostate.DeltaAbsoluteMousePosition[0] / (float)mClientWidth;
			ostate.DeltaNormalizedMousePosition[1] = ostate.DeltaAbsoluteMousePosition[1] / (float)mClientHeight;
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
			self->mInitState.Increase();
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
				return true;
			}
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				if (self == nullptr)
				{
					return 0;
				}
				KeyCode code = Map2KeyCode(wParam);
				if (code != KeyCode::Other)
				{
					self->mInternalInputStates.KeyStates[(int)code] = message == WM_KEYDOWN;
				}
				return 0;
			}
			case WM_LBUTTONUP:
			case WM_LBUTTONDOWN:
			{
				if (self == nullptr)
				{
					return 0;
				}
				self->mInternalInputStates.KeyStates[(int)KeyCode::MouseLeft] = WM_LBUTTONDOWN == message;
				return 0;
			}

			case WM_RBUTTONUP:
			case WM_RBUTTONDOWN:
			{
				if (self == nullptr)
				{
					return 0;
				}
				self->mInternalInputStates.KeyStates[(int)KeyCode::MouseRight] = WM_RBUTTONDOWN == message;
				return 0;
			}

			case WM_MOUSEMOVE:
			{
				if (self == nullptr)
				{
					return 0;
				}
				auto xPos = GET_X_LPARAM(lParam);
				auto yPos = GET_Y_LPARAM(lParam);
				InputState& istate = self->mInternalInputStates;
				istate.AbsoluteMousePosition[0] = xPos;
				istate.AbsoluteMousePosition[1] = yPos;
				istate.NormalizedMousePosition[0] = ((float)xPos) / self->mClientWidth;
				istate.NormalizedMousePosition[1] = 1.0f - ((float)yPos) / self->mClientHeight;
				return 0;
			}
			case WM_DESTROY:
			{
				PostQuitMessage(0);
				return true;
			}
			}
			// Handle any messages the switch statement didn't.
			return ::DefWindowProc(hWnd, message, wParam, lParam);
		}

		DEFINE_MODULE_IN_CLASS(WindowsWindowModule, WindowsWindowModuleImp);
	};
	DEFINE_MODULE_REGISTRY(WindowsWindowModule, WindowsWindowModuleImp, 250);
}