#ifndef __WINDOWS_AUDIO_INTERFACE_H__
#define __WINDOWS_AUDIO_INTERFACE_H__


#include <initguid.h>
#include <dshow.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <map>
#include <string>
#include <vector>

#include "ProcessorCore.hpp"

typedef void(*PROCESSOR_THREAD_EXITED_CB)(void*, DWORD);

typedef struct {
	HANDLE capture_event_handle;
	IAudioCaptureClient* capture_client;
	HANDLE render_event_handle;
	IAudioRenderClient* render_client;
	WAVEFORMATEXTENSIBLE* audio_format;
	fmsmoov::ProcessorCore* core;
	void* audio_interface;
	PROCESSOR_THREAD_EXITED_CB exit_cb;
} CAPTURE_THREAD_DATA;

typedef enum {
	DEVICE_TYPE_INPUT = 0,
	DEVICE_TYPE_OUTPUT = 1
} DEVICE_TYPE;

class WindowsAudioInterface
{
public:
	WindowsAudioInterface();
	virtual ~WindowsAudioInterface();
	void getInputDevices(std::vector<std::wstring>& devList);
	void getOutputDevices(std::vector<std::wstring>& devList);
	void setInputDevice(std::wstring& dev_name);
	void setOutputDevice(std::wstring& dev_name);
	void start();
	void stop();
	static bool running;
	static void processor_exited_callback(void* audio_interface, DWORD exit_code);

protected:
	virtual void enumerateDevices();
	virtual void loadPrefs();
	void set_device(DEVICE_TYPE type, std::wstring& dev_name);

private:
	IMMDeviceEnumerator* pEnumerator;
	IMMDeviceCollection* pCollection;
	IMMDevice* defaultRenderDevice;
	IMMDevice* defaultCaptureDevice;
	IPropertyStore* pPropStore;
	LPWSTR pwszID;
	std::map<std::wstring, IMMDevice*> input_device_map;
	std::map<std::wstring, IMMDevice*> output_device_map;

	IMMDevice* sel_dev_input;
	IMMDevice* sel_dev_output;
	std::wstring sel_dev_name_input;
	std::wstring sel_dev_name_output;
	IAudioClient* audio_client_input;
	IAudioClient* audio_client_output;
	IAudioCaptureClient* audio_capture_client;
	HANDLE capture_event_handle;
	IAudioRenderClient* audio_render_client;
	HANDLE render_event_handle;
	HANDLE capture_thread_handle;
	static CAPTURE_THREAD_DATA capture_thread_data;
	static fmsmoov::ProcessorCore* core;
	static fmsmoov::CoreConfig* core_config;
};

#endif // __WINDOWS_AUDIO_INTERFACE_H__