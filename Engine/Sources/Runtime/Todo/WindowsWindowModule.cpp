#pragma once
#include "Demo18.h"

#if DEMO18_WINDOWS
#include "WindowModule.h"
#include "Math.h"
#include "Debug.h"
#include "Utils.h"
#include "Sync.h"

#include <windows.h>

namespace Demo18
{
	class WindowsWindowModule;
	static WindowsWindowModule* ThisModule;
	class WindowsWindowModule : public WindowModule
	{
	public :
		WindowsWindowModule()
			: mTitle(L"Demo18")
			, mResizeHandled(false)
			, mWindowHandle(0)
			, mKeepAspect(false)
		{
			ThisModule = this;
		}
		virtual void InitializeModule()
		{
			//READ parametrs
			int clientWidth = 1024;
			int clientHeight = 768;
			mClientAspect = float(1.0 * clientWidth / clientHeight);
			mClientSize = V2I(clientWidth, clientHeight);
		}
		virtual void Start()
		{
			Signal signal;
			mWindowThread = std::thread([&]()
			{
				_Start(signal);
			});
			signal.Wait();
		}
		void _Start(Signal& signal)
		{
			auto windowClassName = L"Demo18WindowClass";
			auto instance = (HINSTANCE)GetModuleHandle(nullptr);

			WNDCLASS wc;
			wc.style = CS_HREDRAW | CS_VREDRAW;
			wc.lpfnWndProc = _StaticWindowProc;
			wc.cbClsExtra = wc.cbWndExtra = 0;
			wc.hInstance = instance;
			wc.hIcon = LoadIcon(0, IDI_APPLICATION);
			wc.hCursor = LoadCursor(0, IDC_ARROW);
			wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
			wc.lpszMenuName = 0;
			wc.lpszClassName = windowClassName;
			Check(RegisterClass(&wc));

			mWindowHandle = CreateWindow(
				windowClassName,
				mTitle.c_str(),
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				mClientSize.x,
				mClientSize.y,
				0,
				0,
				instance,
				this);
			Check(mWindowHandle, "creating window failed");

			DWORD dwStyle = (DWORD)GetWindowLongPtr(mWindowHandle, GWL_STYLE);
			HMENU menu = GetMenu(mWindowHandle);
			RECT rc = { 0, 0, mClientSize.x, mClientSize.y };
			AdjustWindowRect(&rc, dwStyle, menu ? TRUE : FALSE);
			mWindowSize.x = rc.right - rc.left;
			mWindowSize.y = rc.bottom - rc.top;
			SetWindowPos(mWindowHandle, NULL, 0, 0, mWindowSize.x, mWindowSize.y, SWP_NOZORDER | SWP_NOMOVE);

			GetClientRect(mWindowHandle, &rc);
			Check(rc.right - rc.left == mClientSize.x);
			Check(rc.bottom - rc.top == mClientSize.y);

			ShowWindow(mWindowHandle, 1);
			UpdateWindow(mWindowHandle);

			signal.Notify();
			_RunMessageLoop();
		}
		virtual void SetTitle(const std::wstring& title)
		{
			mTitle = title;
			if (mWindowHandle)
				SetWindowText(mWindowHandle, mTitle.c_str());
		}
		virtual bool CheckWindowResized(bool clearState)
		{
			auto r = mResizeHandled;
			if (clearState)
				mResizeHandled = true;
			return r;
		}
		virtual V2I GetClientSize() { return mClientSize; }
		virtual V2I GetWindowSize() { return mWindowSize; }
		virtual void SetKeepAspect(bool keep) { mKeepAspect = keep; }
		virtual void CaptureState(KeyMap& keyMap, MouseState& mouseState)
		{
			{
				std::lock_guard<std::mutex> guard(mInputLock);
				keyMap = mKeyStates;
			}
			SHORT r;
			const SHORT MSBMask = SHORT(-1) ^ (USHORT(-1) >> 1);
			r = GetAsyncKeyState(VK_LBUTTON);
			mouseState.LButtonDown = bool(GetAsyncKeyState(VK_LBUTTON) & MSBMask);
			mouseState.RButtonDown = bool(GetAsyncKeyState(VK_RBUTTON) & MSBMask);
			POINT pos;
			auto succ = GetCursorPos(&pos);
			V2I clientPos;
			if (mouseState.LButtonDown)
			{
				clientPos = clientPos;
			}
			if (SUCCEEDED(succ))
			{
				Check(SUCCEEDED(ScreenToClient(mWindowHandle, &pos)));
				clientPos.x = int32_t(pos.x);
				clientPos.y = int32_t(pos.y);
				mLastClientPos = clientPos;
			}
			else
			{
				clientPos = mLastClientPos;
			}
			mouseState.Pos.x = clientPos.x;
			mouseState.Pos.y = mClientSize.y - clientPos.y;
			mouseState.Coord = V2F(float(mouseState.Pos.x) / mClientSize.x, float(mouseState.Pos.y) / mClientSize.y);
		}

		static LRESULT _StaticWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			LRESULT ret;
			auto handled = ThisModule->_WindowProc(hWnd, uMsg, wParam, lParam, ret);
			if (handled)
				return ret;
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
		void GetWindowHandle(void* out) 
		{
			auto rout = (HWND*)out;
			*rout = mWindowHandle;
		}
	protected:
		void _RunMessageLoop()
		{
			MSG msg = {};
			BOOL ret;
			while (ret = GetMessage(&msg, nullptr, 0, 0) != 0)
			{
				if (ret == -1)
				{
					Check(false, "error in Window Message Loop");
				}
				else
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			ExitProcess(0);
		}
		bool _WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& result)
		{
			switch (uMsg)
			{
			case WM_DESTROY:
				PostQuitMessage(0);
				result = 0;
				return true;
			case WM_KEYDOWN:
				//printf("WM_KEYDOWN:%x:%x\n", (unsigned int)wParam, (unsigned int)lParam);
				{
					std::lock_guard<std::mutex> guard(mInputLock);
					mKeyStates[(KeyCode)wParam] = true;
				}
				
				return true;
			case WM_KEYUP:
				//printf("WM_KEYUP:%x:%x\n", (unsigned int)wParam, (unsigned int)lParam);
				{
					std::lock_guard<std::mutex> guard(mInputLock);
					mKeyStates[(KeyCode)wParam] = false;
				}
				return true;
			case WM_SIZING:
				if (mKeepAspect)
					_FixRect(*(RECT*)lParam, wParam);
				break;
			case WM_EXITSIZEMOVE:
				RECT rc;
				GetWindowRect(mWindowHandle, &rc);
				mWindowSize.x = rc.right - rc.left;
				mWindowSize.y = rc.bottom - rc.top;
				GetClientRect(mWindowHandle, &rc);
				mClientSize.x = rc.right - rc.left;
				mClientSize.y = rc.bottom - rc.top;
				mResizeHandled = false;
			}
			return false;
		}
		void _FixRect(RECT& nrc, WPARAM wParam) const
		{
			printf(">>FIXRECT start with(%d,%d,%d,%d)\n", nrc.top, nrc.bottom, nrc.left, nrc.right);
			auto nw = nrc.right - nrc.left;
			auto nh = nrc.bottom - nrc.top;
			int moveVert = 0;
			int moveHori = 0;
			switch (wParam)
			{
			case WMSZ_BOTTOMLEFT:
				moveVert = 1;
				moveHori = -1;
				break;
			case WMSZ_BOTTOM:
				moveVert = 1;
				break;
			case WMSZ_BOTTOMRIGHT:
				moveVert = 1;
				moveHori = 1;
				break;
			case WMSZ_LEFT:
				moveHori = -1;
				break;
			case WMSZ_RIGHT:
				moveHori = 1;
				break;
			case WMSZ_TOPLEFT:
				moveVert = -1;
				moveHori = -1;
				break;
			case WMSZ_TOP:
				moveVert = -1;
				break;
			case WMSZ_TOPRIGHT:
				moveVert = -1;
				moveHori = 1;
				break;
			default:
				Check(false, "what the fuck");
			}
			int fixedH = nh;
			int fixedW = nw;
			auto boarderWidth = (mWindowSize.x - mClientSize.x) / 2;
			auto boarderHeight = (mWindowSize.y - mClientSize.y) / 2;
			int clientWidth = nw - 2 * boarderWidth;
			int clientHeight = nh - 2 * boarderHeight;
			if (moveHori != 0)
			{
				fixedH = int(clientWidth / mClientAspect) + 2 * boarderHeight;
			}
			else
			{
				fixedW = int(clientHeight * mClientAspect) + 2 * boarderWidth;
			}

			auto dh = fixedH - nh;
			auto dw = fixedW - nw;
			if (moveVert == 0)
			{
				nrc.bottom += dh / 2;
				nrc.top -= dh / 2;
			}
			else if (moveVert == -1)
			{
				nrc.top -= dh;
			}
			else
			{
				nrc.bottom += dh;
			}
			if (moveHori == 0)
			{
				nrc.left -= dw / 2;
				nrc.right += dw / 2;
			}
			else if (moveHori == -1)
			{
				nrc.left -= dw;
			}
			else
			{
				nrc.right += dw;
			}
			printf("<<FIXRECT end with(%d,%d,%d,%d)\n", nrc.top, nrc.bottom, nrc.left, nrc.right);
		}
	protected:
		std::wstring mTitle;
		bool mResizeHandled;
		HWND mWindowHandle;
		V2I mWindowSize;
		V2I mClientSize;
		float mClientAspect;
		KeyMap mKeyStates;
		V2I mLastClientPos;
		std::mutex mInputLock;
		bool mKeepAspect;
		std::thread mWindowThread;
	};

	WindowModule* CreateWindowModule()
	{
		return new WindowsWindowModule();
	}
}


#endif//DEMO18_WINDOWS