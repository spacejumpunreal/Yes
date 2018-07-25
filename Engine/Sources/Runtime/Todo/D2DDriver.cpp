#include "D2DDriver.h"
#include "Math.h"
#include "Utils.h"
#include "StringUtils.h"

#include <thread>

#include <d2d1helper.h>

namespace SoftwareRenderer
{
	static Direct2DDriver* mInstance = nullptr;

	static LRESULT CALLBACK Direct2DDriverWindowProc(HWND hwnd, UINT uMSG, WPARAM wParam, LPARAM lParam)
	{
		auto lptr = (Direct2DDriver*)GetWindowLongPtr(hwnd, 0);
		if (lptr)
		{
			Check(lptr == mInstance, "should match");
			return lptr->_WindowProc(hwnd, uMSG, wParam, lParam);
		}
		else
		{
			return DefWindowProc(hwnd, uMSG, wParam, lParam);
		}
	}
	Direct2DDriver::Direct2DDriver(const wchar_t* title, int clientWidth, int clientHeight, int backBufferWidth, int backBufferHeight, int nBuffer)
		// parameters
		: mTitle(title)
		, mClientWidth(clientWidth)
		, mClientHeight(clientHeight)
		, mClientRatio(clientWidth * 1.0 / clientHeight)
		, mBufferWidth(backBufferWidth)
		, mBufferHeight(backBufferHeight)
		, mFreeBuffers(nBuffer)
		, mCommitedBuffers(nBuffer)
		// DX resource
		, mD2DReady(1)
	{
		// module related
		Check(mInstance == nullptr, "initialize once");
		mInstance = this;
		// BackBuffers
		for (int i = 0; i < nBuffer; i++)
		{
			mFreeBuffers.Put(new BackBuffer(mBufferWidth, mBufferHeight));
		}
	}
	void Direct2DDriver::InitializeModule()
	{
		mUIThread = std::thread([this]
		{
			_InitializeWindow();
		});
	}
	Direct2DDriver::~Direct2DDriver()
	{ 
	}
	void Direct2DDriver::RunMessageLoop()
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
	}
	void Direct2DDriver::_RenderBuffer(BackBuffer* buffer)
	{
		HRESULT hr;
		int bw = buffer->Width;
		int bh = buffer->Height;
		mBitmap->CopyFromMemory(nullptr, buffer->Data, buffer->GetStride());
		mRenderTarget->BeginDraw();
		mRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
		mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
		mRenderTarget->DrawBitmap(
			mBitmap,
			D2D1::RectF(0, 0, float(mClientWidth), float(mClientHeight)),
			1.0f,
			D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			nullptr);
		if (!mInfoText.empty())
		{
			mRenderTarget->DrawText(
				mInfoText.data(), (UINT32)mInfoText.size(),
				mTextFormat, D2D1::RectF(0, 0, (float)mClientWidth, (float)mClientHeight), mBrush);
		}
		hr = mRenderTarget->EndDraw();
		Check(hr == S_OK, "draw text should succeed");
		mFPS.StartFrame();
		auto fps = mFPS.GetFPS();
		auto s = FormatString("Frames:%d\nFPS:%4.1f", mFPS.GetFrameCount(), fps);
		SetInfoText(std::wstring(s.begin(), s.end()));
	}
	void Direct2DDriver::_InitializeWindow()
	{
		// window creation
		WNDCLASS wc = {};
		auto className = L"Direct2DDriver";
		wc.lpszClassName = className;
		wc.hInstance = GetModuleHandle(nullptr);
		wc.lpfnWndProc = Direct2DDriverWindowProc;
		wc.cbWndExtra = sizeof(this);
		RegisterClass(&wc);
		mWindowHandle = CreateWindowEx(
			0,
			className,
			mTitle.data(),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, mClientWidth, mClientHeight,
			nullptr, nullptr, nullptr, nullptr);
		if (mWindowHandle == nullptr)
		{
			GetLastError();
			Check(false, "creating window failed");
		}
		SetWindowLongPtr(mWindowHandle, 0, (LONG_PTR)this);
		//adjust window size
		DWORD dwStyle = (DWORD)GetWindowLongPtr(mWindowHandle, GWL_STYLE);
		DWORD dwExStyle = (DWORD)GetWindowLongPtr(mWindowHandle, GWL_EXSTYLE);
		HMENU menu = GetMenu(mWindowHandle);
		///*
		RECT rc = { 0, 0, mClientWidth, mClientHeight };
		AdjustWindowRectEx(&rc, dwStyle, menu ? TRUE : FALSE, dwExStyle);
		auto windowWidth = rc.right - rc.left;
		auto windowHeight = rc.bottom - rc.top;
		SetWindowPos(mWindowHandle, NULL, 0, 0, windowWidth, windowHeight, SWP_NOZORDER | SWP_NOMOVE);
		RECT cr;
		GetClientRect(mWindowHandle, &cr);
		Check(mClientWidth == cr.right - cr.left, "client width");
		Check(mClientHeight == cr.bottom - cr.top, "client height");
		mBoarderWidth = (windowWidth - mClientWidth) / 2;
		mBoarderHeight = (windowHeight - mClientHeight) / 2;
		//show window
		ShowWindow(mWindowHandle, SW_SHOW);
		mDeviceThread = std::thread([this]() {
			_RunD2D();
		});
		RunMessageLoop();
	}
	void Direct2DDriver::_InitializeD2DDevice(bool resize)
	{
		// DX initialization
		HRESULT hr;
		if (!mFactory)
		{
			hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &mFactory);
			Check(hr == S_OK, "creating factory failed");
		}
		if (!mWriteFactory)
		{
			hr = DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown**>(&mWriteFactory));
			Check(hr == S_OK, "create write factory failed");
		}
		if (resize)
		{
			SafeRelease(mRenderTarget);
			SafeRelease(mBrush);
			SafeRelease(mTextFormat);
			SafeRelease(mBitmap);
		}
		D2D1_SIZE_U sz = D2D1::SizeU(mClientWidth, mClientHeight);
		if (!mRenderTarget)
		{
			hr = mFactory->CreateHwndRenderTarget(
				D2D1::RenderTargetProperties(),
				D2D1::HwndRenderTargetProperties(mWindowHandle, sz),
				&mRenderTarget);
			Check(hr == S_OK, "creating render target failed");
		}
		// dx text
		if (!mBrush)
		{
			hr = mRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::Blue),
				&mBrush);
			Check(hr == S_OK, "creating brush failed");
		}
		if (!mTextFormat)
		{
			hr = mWriteFactory->CreateTextFormat(
				L"Consolas",
				nullptr,
				DWRITE_FONT_WEIGHT_REGULAR,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				float(mClientHeight / 20),
				L"zh-cn",
				&mTextFormat);
			Check(hr == S_OK, "create TextFormat failed");
		}
		// dx bitmap
		if (!mBitmap)
		{
			hr = mRenderTarget->CreateBitmap(
				D2D1::SizeU(mBufferWidth, mBufferHeight),
				nullptr,
				0, // ignored since source == nullptr
				D2D1::BitmapProperties(
					D2D1::PixelFormat(
						DXGI_FORMAT_R8G8B8A8_UNORM,
						D2D1_ALPHA_MODE_IGNORE)),
				&mBitmap);
			Check(hr == S_OK, "create Bitmap failed");
		}
	}
	void Direct2DDriver::_RunD2D()
	{
		while (true)
		{
			if (mD2DReady > 0)
			{
				mD2DReady = 0;
				_InitializeD2DDevice(true);
			}
			auto cur = mCommitedBuffers.Get();
			_RenderBuffer(cur);
			mFreeBuffers.Put(cur);
		}
	}
	void Direct2DDriver::_KeyStateChanged(WPARAM wParam, bool oldState, bool newState)
	{
		if (oldState == newState)
			return;
		printf("KeyState:%x:%x\n", (unsigned int)wParam, newState ? 1 : 0);
	}
	void Direct2DDriver::_FixRect(RECT& nrc, WPARAM wParam) const
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
		int clientWidth = nw - 2 * mBoarderWidth;
		int clientHeight = nh - 2 * mBoarderHeight;
		if (moveHori != 0)
		{
			fixedH = int(clientWidth / mClientRatio) + 2 * mBoarderHeight;
		}
		else
		{
			fixedW = int(clientHeight * mClientRatio) + 2 * mBoarderWidth;
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
	LRESULT Direct2DDriver::_WindowProc(HWND hwnd, UINT uMSG, WPARAM wParam, LPARAM lParam)
	{
		bool handled = false;
		switch (uMSG)
		{
		case WM_KEYDOWN:
			//printf("WM_KEYDOWN:%x:%x\n", (unsigned int)wParam, (unsigned int)lParam);
			_KeyStateChanged(wParam, mKeyStates[wParam], true);
			mKeyStates[wParam] = true;
			break;
		case WM_KEYUP:
			//printf("WM_KEYUP:%x:%x\n", (unsigned int)wParam, (unsigned int)lParam);
			_KeyStateChanged(wParam, mKeyStates[wParam], false);
			mKeyStates[wParam] = false;
			break;
		case WM_SIZING:
			_FixRect(*(RECT*)lParam, wParam);
			break;
		case WM_EXITSIZEMOVE:
		{
			RECT rc;
			GetClientRect(mWindowHandle, &rc);
			mClientWidth = rc.right - rc.left;
			mClientHeight = rc.bottom - rc.top;
			mD2DReady++;
			break;
		}
			
		}
		if (handled)
			return TRUE;
		else
			return DefWindowProc(hwnd, uMSG, wParam, lParam);
	}
	BackBuffer* Direct2DDriver::AcquireBuffer()
	{
		return mFreeBuffers.Get();
	}
	void Direct2DDriver::CommitBuffer(BackBuffer* buffer)
	{
		mCommitedBuffers.Put(buffer);
	}
}