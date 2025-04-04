#include "pch.h"


#include "WindowsAudioInterface.h"
#include "Functiondiscoverykeys_devpkey.h"

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

WindowsAudioInterface::WindowsAudioInterface() {
	CoInitialize(nullptr);
	pEnumerator = nullptr;
	defaultRenderDevice = nullptr;
	defaultCaptureDevice = nullptr;
	pCollection = nullptr;
	pPropStore = nullptr;
	pwszID = nullptr;
	this->enumerateDevices();
}

WindowsAudioInterface::~WindowsAudioInterface() {
	CoUninitialize();
}


void WindowsAudioInterface::enumerateDevices() {
	
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

	device_map.clear();
	hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);

	EXIT_ON_ERROR(hr)

	uint32_t count = 0;
	hr = pCollection->GetCount(&count);
	EXIT_ON_ERROR(hr)

	TRACE("%d audio endpoints found.\n", count);

	IMMDevice* dev = nullptr;
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
		device_map[devFriendlyName] = dev;

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

	for (std::map<std::wstring, IMMDevice*>::iterator it = device_map.begin(); it != device_map.end(); it++) {
		devList.push_back(it->first);
	}
}

void WindowsAudioInterface::getOutputDevices(std::vector<std::wstring>& devList) {
	devList.clear();

	for (std::map<std::wstring, IMMDevice*>::iterator it = device_map.begin(); it != device_map.end(); it++) {
		devList.push_back(it->first);
	}
}