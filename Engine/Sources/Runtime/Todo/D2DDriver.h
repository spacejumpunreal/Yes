#pragma once
#include "Sync.h"
#include "Time.h"
#include "Math.h"
#include "WindowModule.h"

#include <unordered_map>
#include <string>
#include <thread>
#include <atomic>

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>

namespace SoftwareRenderer
{
	class Direct2DDriver : public WindowModule
	{
	public:
		virtual void InitializeModule() override;
		virtual bool IsKeyPressed(KeyCode k) override { return mKeyStates[(WPARAM)k]; }
		virtual void SetInfoText(const std::wstring& text) override { mInfoText = text; }
		virtual BackBuffer* AcquireBuffer() override;
		virtual void CommitBuffer(BackBuffer* buffer) override;
	public:
		Direct2DDriver(
			const wchar_t* title = L"Direct2DDriver",
			int clientWidth = 800, int clientHeight = 600,
			int backBufferWidth = 80, int backBufferHeight = 60,
			int nBuffers = 4);
		~Direct2DDriver();
		// message loop
		void RunMessageLoop();
		// window proc
		LRESULT _WindowProc(HWND hwnd, UINT uMSG, WPARAM wParam, LPARAM lParam);
	private:
		void _InitializeWindow();
		void _InitializeD2DDevice(bool resize);
		void _RunD2D();
		void _FixRect(RECT& nrc, WPARAM wParam) const;
		void _KeyStateChanged(WPARAM wParam, bool oldState, bool newState);
		void _RenderBuffer(BackBuffer* buffer);
		//  parameters
		std::wstring mTitle;
		int mClientWidth, mClientHeight;
		double mClientRatio;
		int mBoarderWidth, mBoarderHeight;
		int mBufferWidth, mBufferHeight;
		HWND mWindowHandle;
		// statistics
		std::wstring mInfoText;
		FPSCalculator<30> mFPS;
		// KeyState
		std::unordered_map<WPARAM, bool> mKeyStates;
		// DX resource
		std::atomic<int> mD2DReady;
		ID2D1Factory* mFactory = nullptr;
		IDWriteFactory* mWriteFactory = nullptr;
		ID2D1HwndRenderTarget* mRenderTarget = nullptr;
		ID2D1SolidColorBrush* mBrush = nullptr;
		IDWriteTextFormat* mTextFormat = nullptr;
		ID2D1Bitmap* mBitmap = nullptr;
		// BackBuffer
		RingBuffer<BackBuffer*> mFreeBuffers;
		RingBuffer<BackBuffer*> mCommitedBuffers;
		// worker thread
		std::thread mDeviceThread;
		std::thread mUIThread;
	};
}
