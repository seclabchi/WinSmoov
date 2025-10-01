#include "pch.h"

#include "WindowsAudioInterface.h"
#include <Functiondiscoverykeys_devpkey.h>
#include <devicetopology.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

CAPTURE_THREAD_DATA WindowsAudioInterface::capture_thread_data = { NULL, NULL, NULL, NULL, NULL, NULL };
bool WindowsAudioInterface::running = FALSE;
fmsmoov::ProcessorCore* WindowsAudioInterface::core = NULL;
fmsmoov::CoreConfig* WindowsAudioInterface::core_config = NULL;

static DWORD capture_thread_func(void* param) {
	DWORD wait_result = 0;
	HRESULT result = S_OK;

	CAPTURE_THREAD_DATA* thread_data = (CAPTURE_THREAD_DATA*)param;
	HANDLE capture_event = thread_data->capture_event_handle;
	IAudioCaptureClient* cli_capture = thread_data->capture_client;
	HANDLE render_event = thread_data->render_event_handle;
	IAudioRenderClient* cli_render = thread_data->render_client;
	WAVEFORMATEXTENSIBLE* audio_format = thread_data->audio_format;
	fmsmoov::ProcessorCore* core = thread_data->core;
	void* audio_interface = thread_data->audio_interface;
	PROCESSOR_THREAD_EXITED_CB exit_cb = thread_data->exit_cb;

	HANDLE event_handles[2] = { capture_event, render_event };

	BYTE* pCaptureData = NULL;
	UINT32 numFramesToRead = 0;
	DWORD flags = 0;

	BYTE* pRenderData = NULL;

	UINT32 packet_len = 0;
	UINT32 next_packet_len = 0;

	CFile output_file;

	TRACE("capture_thread_func started\n");

	try {
		output_file.Open(_T("C:\\tmp\\capture.pcm"), CFile::modeCreate | CFile::modeWrite | CFile::typeBinary);
	}
	catch (CFileException* ex) {
		TCHAR szError[1024];
		ex->GetErrorMessage(szError, sizeof(szError));
		TRACE("Opening file failed with exception reason %s", szError);
		ex->Delete();
	}

	while (WindowsAudioInterface::running) {

		try {
			wait_result = WaitForMultipleObjects(2, event_handles, FALSE, INFINITE);

			if (wait_result == WAIT_OBJECT_0) {
				result = cli_capture->GetNextPacketSize(&next_packet_len);

				if ((next_packet_len > 0) && (packet_len != next_packet_len)) {
					packet_len = next_packet_len;
					core->set_packet_frame_length(packet_len);
				}

				if(next_packet_len > 0) {
					result = cli_capture->GetBuffer(&pCaptureData, &numFramesToRead, &flags, NULL, NULL);

					if (result != S_OK) {
						TRACE("ERROR cli_capture->GetBuffer 0x%08X\n", result);
						exit_cb(audio_interface, result);
						return (DWORD)result;
					}

					//output_file.Write(pData, numFramesToRead * 2 * sizeof(float));

					if (cli_render) {
						if (numFramesToRead > 0) {

							result = cli_render->GetBuffer(numFramesToRead, &pRenderData);

							if (result != S_OK) {
								TRACE("ERROR cli_render->GetBuffer 0x%08X\n", result);
								exit_cb(audio_interface, result);
								return (DWORD)result;
							}

							//memcpy(pRenderData, pCaptureData, numFramesToRead * 2 * sizeof(float));
							core->process((float*)pRenderData, (float*)pCaptureData);

							result = cli_render->ReleaseBuffer(numFramesToRead, 0);

							if (result != S_OK) {
								TRACE("ERROR cli_render->ReleaseBuffer 0x%08X\n", result);						
								exit_cb(audio_interface, result);
								return (DWORD)result;
							}
						}
					}

					result = cli_capture->ReleaseBuffer(numFramesToRead);
					if (result != S_OK) {
						TRACE("ERROR cli_capture->ReleaseBuffer 0x%08X\n", result);
						exit_cb(audio_interface, result);
						return (DWORD)result;
					}

					//result = cli_capture->GetNextPacketSize(&next_packet_len);
					//if (result != S_OK) {
					//	TRACE("ERROR cli_capture->GetNextPacketSize 0x%08X\n", result);
					//	return (DWORD)result;
					//}

				}
			}
		}
		catch (CException* ex) {
			TRACE("CAUGHT EXCEPTION IN PROCESSING THREAD\n");
		}
	}
	exit_cb(audio_interface, 0);
	return (DWORD)0;
}

void WindowsAudioInterface::processor_exited_callback(void* audio_interface, DWORD exit_code) {
	WindowsAudioInterface* wai = reinterpret_cast<WindowsAudioInterface*>(audio_interface);

	if (wai) {
		TRACE("Got processor thread exit callback with code 0x%08X\n", exit_code);
		//wai->stop();
	}
	else {
		TRACE("Failed to reinterpret_cast to WindowsAudioInterface in processor thread callback.\n");
	}
}


WindowsAudioInterface::WindowsAudioInterface() {

	running = false;
	sel_dev_input = NULL;
	sel_dev_output = NULL;
	audio_client_input = NULL;
	audio_client_output = NULL;
	audio_capture_client = NULL;
	capture_event_handle = NULL;
	audio_render_client = NULL;
	render_event_handle = NULL;
	capture_thread_handle = NULL;

	HRESULT result = CoInitialize(nullptr);

	if (S_OK == result) {
		TRACE("CoInitialize OK.\n");

	}
	else {
		TRACE("CoInitialize error %d.\n", result);
	}

	pEnumerator = nullptr;
	defaultRenderDevice = nullptr;
	defaultCaptureDevice = nullptr;
	pCollection = nullptr;
	pPropStore = nullptr;
	pwszID = nullptr;
	this->loadPrefs();
	this->enumerateDevices();
}

WindowsAudioInterface::~WindowsAudioInterface() {
	if (core) {
		core->stop();
		delete core;
	}

	if (core_config) {
		core_config->release();
	}

	CoUninitialize();
}

void WindowsAudioInterface::loadPrefs() {
	std::wstring cfg_file_path(_T("C:\\tmp\\winsmoov.cfg"));
	core_config = fmsmoov::CoreConfig::get_instance();
	bool result = core_config->load(cfg_file_path);

	if (!result) {
		TRACE("Failed to load core config from path %s", cfg_file_path);
	}
}

void WindowsAudioInterface::enumerateDevices() {

	IMMDevice* dev = nullptr;
	uint32_t count = 0;
	
	HRESULT hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator),
		NULL,
		CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		(void**)&pEnumerator);

	if (S_OK == hr) {
		TRACE("Got a handle to the MM device enumerator\n");

	} 
	else {
		TRACE("There was an error %d getting the audio device enumerator.\n", hr);
	}

	EXIT_ON_ERROR(hr)
	
	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultRenderDevice);
	EXIT_ON_ERROR(hr)
	hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &defaultCaptureDevice);

	input_device_map.clear();
	output_device_map.clear();
	hr = pEnumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &pCollection);

	EXIT_ON_ERROR(hr)

	hr = pCollection->GetCount(&count);
	EXIT_ON_ERROR(hr)

	TRACE("%d audio endpoints found.\n", count);

	
	for (uint32_t i = 0; i < count; i++) {
		hr = pCollection->Item(i, &dev);
		EXIT_ON_ERROR(hr)

		hr = dev->GetId(&pwszID);
		dev->OpenPropertyStore(STGM_READ, &pPropStore);

		PROPVARIANT propDevFriendlyName;
		PropVariantInit(&propDevFriendlyName);
		hr = pPropStore->GetValue(PKEY_Device_FriendlyName, &propDevFriendlyName);
		EXIT_ON_ERROR(hr)

		if (propDevFriendlyName.vt != VT_EMPTY) {
			TRACE("Found audio endpoint named %S (%S)\n", propDevFriendlyName.pwszVal, pwszID);
		}

		std::wstring devFriendlyName = propDevFriendlyName.pwszVal;

		IDeviceTopology* dev_topology = nullptr;
		uint32_t connector_count = 0;
		IConnector* dev_connector = nullptr;
		ConnectorType connector_type = ConnectorType::Unknown_Connector;
		DataFlow connector_flow = DataFlow::Out;

		hr = dev->Activate(
			__uuidof(IDeviceTopology),
			CLSCTX_ALL,
			NULL,
			(void**)&dev_topology);

		EXIT_ON_ERROR(hr)

		hr = dev_topology->GetConnectorCount(&connector_count);
		EXIT_ON_ERROR(hr)
		hr = dev_topology->GetConnector(0, &dev_connector);
		EXIT_ON_ERROR(hr)
		hr = dev_connector->GetType(&connector_type);
		EXIT_ON_ERROR(hr)
		hr = dev_connector->GetDataFlow(&connector_flow);
		EXIT_ON_ERROR(hr)

		if (connector_flow == eRender) {
			output_device_map[devFriendlyName] = dev;
		}
		else if (connector_flow == eCapture) {
			input_device_map[devFriendlyName] = dev;
		}
		else {
			TRACE("Unknown connector flow %d\n", connector_flow);
		}

		PropVariantClear(&propDevFriendlyName);
		SAFE_RELEASE(pPropStore)
		SAFE_RELEASE(dev);
		CoTaskMemFree(pwszID);
		pwszID = nullptr;
	}

	return;


Exit:

	CoTaskMemFree(pwszID); pEnumerator = nullptr;
	SAFE_RELEASE(defaultRenderDevice)
	SAFE_RELEASE(defaultCaptureDevice)
	SAFE_RELEASE(pCollection)
	SAFE_RELEASE(pPropStore)

}

void WindowsAudioInterface::getInputDevices(std::vector<std::wstring>& devList) {
	devList.clear();

	for (std::map<std::wstring, IMMDevice*>::iterator it = input_device_map.begin(); it != input_device_map.end(); it++) {
		devList.push_back(it->first);
	}
}

void WindowsAudioInterface::getOutputDevices(std::vector<std::wstring>& devList) {
	devList.clear();

	for (std::map<std::wstring, IMMDevice*>::iterator it = output_device_map.begin(); it != output_device_map.end(); it++) {
		devList.push_back(it->first);
	}
}

void WindowsAudioInterface::set_device(DEVICE_TYPE type, std::wstring& dev_name) {
	IMMDevice* sel_dev = NULL;
	std::wstring sel_dev_name = std::wstring();
	std::map<std::wstring, IMMDevice*> dev_map;

	this->stop();

	if (type == DEVICE_TYPE_INPUT) {
		dev_map = input_device_map;
	}
	else if (type == DEVICE_TYPE_OUTPUT) {
		dev_map = output_device_map;
	}
	else {
		TRACE("Invalid device type %d\n", type);
		return;
	}

	std::wstring enum_devname = _T("");
	bool found = false;

	for (std::map<std::wstring, IMMDevice*>::iterator it = dev_map.begin(); it != dev_map.end(); it++) {
		enum_devname = it->first;
		if (!dev_name.compare(enum_devname)) {
			found = true;
			sel_dev = it->second;
			sel_dev_name = it->first;
			break;
		}
	}

	if (found) {
		if (type == DEVICE_TYPE_INPUT) {
			TRACE("FOUND SELECTED INPUT DEVICE %ls IN SYSTEM DEVICES\n", dev_name.c_str());
			sel_dev_input = sel_dev;
			sel_dev_name_input = sel_dev_name;
		}
		else if (type == DEVICE_TYPE_OUTPUT) {
			TRACE("FOUND SELECTED OUTPUT DEVICE %ls IN SYSTEM DEVICES\n", dev_name.c_str());
			sel_dev_output = sel_dev;
			sel_dev_name_output = sel_dev_name;
		}

		this->start();
	}
	else {
		TRACE("DID NOT FIND SELECTED DEVICE %ls IN SYSTEM DEVICES\n", dev_name.c_str());
	}

}

void  WindowsAudioInterface::setInputDevice(std::wstring& dev_name) {
	set_device(DEVICE_TYPE_INPUT, dev_name);
}

void  WindowsAudioInterface::setOutputDevice(std::wstring& dev_name) {
	set_device(DEVICE_TYPE_OUTPUT, dev_name);
}

void WindowsAudioInterface::start() {

	HRESULT result = NULL;
	WAVEFORMATEX* dev_format_input = NULL;
	WAVEFORMATEXTENSIBLE* dev_format_input_ex = NULL;
	WAVEFORMATEX* dev_format_output = NULL;
	WAVEFORMATEXTENSIBLE* dev_format_output_ex = NULL;
	REFERENCE_TIME hnsDuration = 0;

	if (!running) {
		if (NULL == sel_dev_input) {
			TRACE("Can't start audio capture on a NULL device\n");
			goto failed;
		}

		result = sel_dev_input->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**) & audio_client_input);
			
		if (result != S_OK) {
			TRACE("Failed to activate input device with error 0x%08X\n", result);
			goto failed;
		}
				
		result = audio_client_input->GetMixFormat(&dev_format_input);

		if (result != S_OK) {
			TRACE("Failed to get audio input mix format with error 0x%08X\n", result);
			goto failed;
		}

		if (dev_format_input->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			dev_format_input_ex = (WAVEFORMATEXTENSIBLE*)dev_format_input;
			TRACE("Got extensible format information from audio input.\n");
		}

		result = audio_client_input->GetDevicePeriod(NULL, &hnsDuration);
		if (result != S_OK) {
			TRACE("Failed to get audio input device period with error 0x%08X\n", result);
			goto failed;
		}


		result = audio_client_input->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsDuration, hnsDuration, dev_format_input, NULL);

		if (result != S_OK) {
			TRACE("Failed to initialize audio input device with error 0x%08X\n", result);
			LPWSTR buffer = nullptr;
			FormatMessageW(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				result,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPWSTR)&buffer,
				0,
				NULL
			);
			TRACE("Error: %s\n", buffer);
			LocalFree(buffer);
			goto failed;
		}

		capture_event_handle = CreateEvent(NULL, FALSE, FALSE, _T("capture_event_handle"));

		if (capture_event_handle == NULL) {
			TRACE("Failed to create capture event handle\n");
			goto failed;
		}

		result = audio_client_input->SetEventHandle(capture_event_handle);

		if (result != S_OK) {
			TRACE("Failed to set audio input event handle with error 0x%08X\n", result);
			goto failed;
		}

		result = audio_client_input->GetService(_uuidof(IAudioCaptureClient), (void**) & audio_capture_client);

		if (result != S_OK) {
			TRACE("Failed to get audio input capture service with error 0x%08X\n", result);
			goto failed;
		}

		capture_thread_data.capture_event_handle = capture_event_handle;
		capture_thread_data.capture_client = audio_capture_client;

		if (NULL == sel_dev_output) {
			TRACE("Can't start audio render on a NULL device\n");
			goto failed;
		}

		result = sel_dev_output->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audio_client_output);

		if (result != S_OK) {
			TRACE("Failed to activate output device with error 0x%08X\n", result);
			goto failed;
		}

		result = audio_client_output->GetMixFormat(&dev_format_output);

		if (result != S_OK) {
			TRACE("Failed to get audio output mix format with error 0x%08X\n", result);
			goto failed;
		}

		if (dev_format_output->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			dev_format_output_ex = (WAVEFORMATEXTENSIBLE*)dev_format_output;
			TRACE("Got extensible format information from audio output.\n");
		}

		result = audio_client_output->GetDevicePeriod(NULL, &hnsDuration);
		if (result != S_OK) {
			TRACE("Failed to get audio output device period with error 0x%08X\n", result);
			goto failed;
		}

		//compare the formats.  If they're different, we cannot proceed.
		//TODO: set up format conversion?
		if (dev_format_input_ex && dev_format_output_ex) {
			if (dev_format_output_ex->dwChannelMask == dev_format_input_ex->dwChannelMask &&
				!memcmp(&(dev_format_output_ex->Format), &(dev_format_input_ex->Format), sizeof(WAVEFORMATEX)) &&
				!memcmp(&(dev_format_output_ex->Samples), &(dev_format_input_ex->Samples), sizeof(WORD)) &&
				dev_format_output_ex->SubFormat == dev_format_input_ex->SubFormat) {
				TRACE("Input and output audio formats are identical.  Proceed.\n");
			}
			else {
				TRACE("Input and output audio formats differ.  Abort.\n");
				goto failed;
			}
		}
		else {
			TRACE("Can't continue - no extensible format info for input or output.\n");
			goto failed;
		}


		result = audio_client_output->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsDuration, hnsDuration, dev_format_output, NULL);

		if (result != S_OK) {
			TRACE("Failed to initialize audio output device with error 0x%08X\n", result);
			LPWSTR buffer = nullptr;
			FormatMessageW(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				result,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPWSTR)&buffer,
				0,
				NULL
			);
			TRACE("Error: %s\n", buffer);
			LocalFree(buffer);
			goto failed;
		}

		render_event_handle = CreateEvent(NULL, FALSE, FALSE, _T("render_event_handle"));

		if (render_event_handle == NULL) {
			TRACE("Failed to create render event handle\n");
			goto failed;
		}

		result = audio_client_output->SetEventHandle(render_event_handle);

		if (result != S_OK) {
			TRACE("Failed to set audio output event handle with error 0x%08X\n", result);
			goto failed;
		}

		result = audio_client_output->GetService(_uuidof(IAudioRenderClient), (void**)&audio_render_client);

		if (result != S_OK) {
			TRACE("Failed to get audio output render service with error 0x%08X\n", result);
			goto failed;
		}

		capture_thread_data.render_event_handle = render_event_handle;
		capture_thread_data.render_client = audio_render_client;

		capture_thread_handle = CreateThread(NULL, 0, capture_thread_func, (LPVOID)&capture_thread_data, CREATE_SUSPENDED, NULL);

		if (!capture_thread_handle) {
			TRACE("Failed to create processing thread\n");
			goto failed;
		}

		if (core) {
			delete core;
			core = NULL;
		}

		core = new fmsmoov::ProcessorCore(dev_format_input_ex->Format.nSamplesPerSec, 
			dev_format_input_ex->Format.nChannels,
			0);

		if (core) {
			fmsmoov::AUDIO_DEVICE dev_in = { 0, sel_dev_name_input, std::wstring() };
			fmsmoov::AUDIO_DEVICE dev_out = { 0, sel_dev_name_output, std::wstring() };

			core->set_audio_devices(dev_in, dev_out);
		}

		capture_thread_data.core = core;
		capture_thread_data.audio_format = dev_format_input_ex;
		capture_thread_data.audio_interface = this;
		capture_thread_data.exit_cb = processor_exited_callback;
		
		running = true;
		ResumeThread(capture_thread_handle);

		result = audio_client_input->Start();

		if (result != S_OK) {
			TRACE("Failed to start audio input with error 0x%08X\n", result);
			goto failed;
		}

		result = audio_client_output->Start();

		if (result != S_OK) {
			TRACE("Failed to start audio output with error 0x%08X\n", result);
			goto failed;
		}
	}

failed:
	return;
}

void WindowsAudioInterface::stop() {
	HRESULT result = S_OK;

	running = false;

	TRACE("Waiting for processor thread to complete...\n");

	DWORD dwResult = WaitForSingleObject(capture_thread_handle, INFINITE); // Wait indefinitely

	TRACE("dwResult = %d\n", dwResult);

	if (core) {
		core->stop();
		delete core;
		core = NULL;
	}

	if (capture_thread_handle) {
		DWORD dwResult = WaitForSingleObject(capture_thread_handle, INFINITE);
		TRACE("Processing thread has stopped.\n");
	}

	if (audio_client_output) {
		result = audio_client_output->Stop();
		audio_render_client->Release();
		audio_client_output->Release();
		CloseHandle(render_event_handle);
		audio_render_client = NULL;
		audio_client_output = NULL;
		render_event_handle = NULL;
	}

	if (audio_client_input) {
		result = audio_client_input->Stop();
		audio_capture_client->Release();
		audio_client_input->Release();
		CloseHandle(capture_event_handle);
		audio_capture_client = NULL;
		audio_client_input = NULL;
		capture_event_handle = NULL;
	}

	if (capture_thread_handle) {
		CloseHandle(capture_thread_handle);
		capture_thread_handle = NULL;
	}
		
	return;
}