#pragma once

#include <cstring>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include "TritonVision.h"

// Server Triton (IP:port gRPC) dan nama model yang dipakai
static const char* const TRITON_URL   = "192.168.1.156:8001";
static const char* const TRITON_MODEL = "model1";

namespace CppCLRWinFormsProject {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// Summary for Form1
	/// </summary>
	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();

			pictureBox1->SizeMode = PictureBoxSizeMode::Zoom;   // gambar di-scale, rasio dijaga

			cap = new cv::VideoCapture();
			liveFrame = new cv::Mat();
			liveMode = true;
			tritonConnected = false;

			timer = gcnew System::Windows::Forms::Timer();      // refresh kamera
			timer->Interval = 33;                                // ~30 fps
			timer->Tick += gcnew System::EventHandler(this, &Form1::timer_Tick);

			if (OpenCamera())
				timer->Start();
			else
				MessageBox::Show("Kamera tidak bisa dibuka saat startup!", "Camera Error",
					MessageBoxButtons::OK, MessageBoxIcon::Warning);
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~Form1()
		{
			if (timer)
				timer->Stop();
			if (components)
			{
				delete components;
			}
			this->!Form1();
		}

		// Finalizer: lepas resource native (kamera & frame)
		!Form1()
		{
			if (cap)
			{
				cap->release();
				delete cap;
				cap = nullptr;
			}
			delete liveFrame;
			liveFrame = nullptr;
		}
	private: System::Windows::Forms::PictureBox^ pictureBox1;
	protected:
	private: System::Windows::Forms::Button^ btnCaptureCamera;
	private: System::Windows::Forms::Button^ btnReloadCamera;

	private:
		cv::VideoCapture* cap;                     // handle kamera (native, jadi pointer)
		cv::Mat* liveFrame;                        // frame live terakhir
		bool liveMode;                             // true = tampilkan live, false = freeze hasil capture
		bool tritonConnected;                      // sudah Connect() ke Triton?
		System::Windows::Forms::Timer^ timer;      // timer refresh kamera

		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->pictureBox1 = (gcnew System::Windows::Forms::PictureBox());
			this->btnCaptureCamera = (gcnew System::Windows::Forms::Button());
			this->btnReloadCamera = (gcnew System::Windows::Forms::Button());
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox1))->BeginInit();
			this->SuspendLayout();
			// 
			// pictureBox1
			// 
			this->pictureBox1->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom)
				| System::Windows::Forms::AnchorStyles::Left)
				| System::Windows::Forms::AnchorStyles::Right));
			this->pictureBox1->Location = System::Drawing::Point(272, 12);
			this->pictureBox1->Name = L"pictureBox1";
			this->pictureBox1->Size = System::Drawing::Size(1280, 720);
			this->pictureBox1->TabIndex = 0;
			this->pictureBox1->TabStop = false;
			// 
			// btnCaptureCamera
			// 
			this->btnCaptureCamera->Location = System::Drawing::Point(12, 12);
			this->btnCaptureCamera->Name = L"btnCaptureCamera";
			this->btnCaptureCamera->Size = System::Drawing::Size(254, 75);
			this->btnCaptureCamera->TabIndex = 1;
			this->btnCaptureCamera->Text = L"CAPTURE CAMERA";
			this->btnCaptureCamera->UseVisualStyleBackColor = true;
			this->btnCaptureCamera->Click += gcnew System::EventHandler(this, &Form1::btnCaptureCamera_Click);
			// 
			// btnReloadCamera
			// 
			this->btnReloadCamera->Location = System::Drawing::Point(12, 93);
			this->btnReloadCamera->Name = L"btnReloadCamera";
			this->btnReloadCamera->Size = System::Drawing::Size(254, 75);
			this->btnReloadCamera->TabIndex = 2;
			this->btnReloadCamera->Text = L"RELOAD CAMERA";
			this->btnReloadCamera->UseVisualStyleBackColor = true;
			this->btnReloadCamera->Click += gcnew System::EventHandler(this, &Form1::btnReloadCamera_Click);
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(1564, 781);
			this->Controls->Add(this->btnReloadCamera);
			this->Controls->Add(this->btnCaptureCamera);
			this->Controls->Add(this->pictureBox1);
			this->Name = L"Form1";
			this->Text = L"Form1";
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox1))->EndInit();
			this->ResumeLayout(false);

		}
#pragma endregion
		// ------------------------------------------------------------ Camera ----
		private: bool OpenCamera()
		{
			if (!cap->open(0, cv::CAP_DSHOW))               // kamera index 0, DirectShow (cepat di Windows)
				return false;
			cap->set(cv::CAP_PROP_FRAME_WIDTH, 1280);
			cap->set(cv::CAP_PROP_FRAME_HEIGHT, 720);
			return true;
		}

		// Tampilkan cv::Mat (BGR) ke pictureBox1
		private: void ShowMat(const cv::Mat& bgr)
		{
			if (bgr.empty() || bgr.type() != CV_8UC3)
				return;

			// Format24bppRgb di memori urutannya B,G,R = sama dengan OpenCV
			Bitmap^ bmp = gcnew Bitmap(bgr.cols, bgr.rows, Imaging::PixelFormat::Format24bppRgb);
			Imaging::BitmapData^ data = bmp->LockBits(System::Drawing::Rectangle(0, 0, bgr.cols, bgr.rows),
				Imaging::ImageLockMode::WriteOnly, bmp->PixelFormat);
			unsigned char* dst = static_cast<unsigned char*>(data->Scan0.ToPointer());
			for (int y = 0; y < bgr.rows; ++y)              // copy per baris (stride Bitmap bisa beda)
				std::memcpy(dst + y * data->Stride, bgr.ptr(y), bgr.cols * 3);
			bmp->UnlockBits(data);

			Image^ old = pictureBox1->Image;
			pictureBox1->Image = bmp;
			if (old != nullptr)
				delete old;                                  // Dispose bitmap lama biar memori gak bocor
		}

		private: System::Void timer_Tick(System::Object^ sender, System::EventArgs^ e)
		{
			if (!liveMode)                                   // lagi freeze hasil capture
				return;

			cv::Mat frame;
			if (!cap->read(frame) || frame.empty())          // kamera hiccup -> skip
				return;

			*liveFrame = frame;                              // simpan buat Capture
			ShowMat(frame);
		}

		// ----------------------------------------------------------- Buttons ----
		private: System::Void btnCaptureCamera_Click(System::Object^ sender, System::EventArgs^ e)
		{
			if (liveFrame->empty()) {
				MessageBox::Show(this, "Belum ada frame dari kamera.", "Capture",
					MessageBoxButtons::OK, MessageBoxIcon::Warning);
				return;
			}

			this->Cursor = Cursors::WaitCursor;

			if (!tritonConnected) {                          // connect sekali di capture pertama
				tritonConnected = Connect(TRITON_URL, TRITON_MODEL);
				if (!tritonConnected) {
					this->Cursor = Cursors::Default;
					MessageBox::Show(this, "Gagal connect ke " + gcnew String(TRITON_URL) + ":\n" +
						gcnew String(LastError().c_str()), "Server", MessageBoxButtons::OK, MessageBoxIcon::Error);
					return;
				}
			}

			liveMode = false;                                // freeze tampilan
			timer->Stop();
			cv::Mat captured = liveFrame->clone();

			std::vector<Detection> dets = Detect(captured);  // kirim ke Triton
			std::string err = LastError();
			this->Cursor = Cursors::Default;

			if (dets.empty() && !err.empty()) {              // inferensi gagal
				tritonConnected = false;                     // capture berikutnya coba connect ulang
				ShowMat(captured);
				MessageBox::Show(this, gcnew String(err.c_str()), "Inference Error",
					MessageBoxButtons::OK, MessageBoxIcon::Error);
				return;
			}

			Draw(captured, dets);                            // gambar bbox + label
			ShowMat(captured);
			this->Text = String::Format("CAPTURED | {0} | {1} objek | infer {2:F0} ms",
				gcnew String(TRITON_MODEL), static_cast<int>(dets.size()), LastInferMs());
		}
		private: System::Void btnReloadCamera_Click(System::Object^ sender, System::EventArgs^ e)
		{
			timer->Stop();

			if (cap->isOpened()) {
				for (int i = 0; i < 5; ++i)                  // buang frame basi di buffer
					cap->grab();

				cv::Mat test;
				if (cap->read(test) && !test.empty()) {      // kamera masih jalan -> lanjut live
					liveMode = true;
					this->Text = "LIVE";
					timer->Start();
					return;
				}
				cap->release();                              // gagal baca -> anggap putus
			}

			if (OpenCamera()) {                              // buka ulang kamera
				liveMode = true;
				this->Text = "LIVE";
				timer->Start();
			}
			else {
				MessageBox::Show(this, "Gagal reconnect ke kamera!", "Camera Error",
					MessageBoxButtons::OK, MessageBoxIcon::Error);
			}
		}
	};
}
