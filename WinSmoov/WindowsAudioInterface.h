#pragma once

#include <initguid.h>
#include <dshow.h>
#include "Mmdeviceapi.h"
#include <map>
#include <string>
#include <vector>

class WindowsAudioInterface
{
public:
	WindowsAudioInterface();
	virtual ~WindowsAudioInterface();
	void getInputDevices(std::vector<std::wstring>& devList);
	void getOutputDevices(std::vector<std::wstring>& devList);

protected:
	virtual void enumerateDevices();
private:
	IMMDeviceEnumerator* pEnumerator;
	IMMDeviceCollection* pCollection;
	IMMDevice* defaultRenderDevice;
	IMMDevice* defaultCaptureDevice;
	IPropertyStore* pPropStore;
	LPWSTR pwszID;
	std::map<std::wstring, IMMDevice*> input_device_map;
	std::map<std::wstring, IMMDevice*> output_device_map;
};

