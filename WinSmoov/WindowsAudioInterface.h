#pragma once

#include "Mmdeviceapi.h"

class WindowsAudioInterface
{
public:
	WindowsAudioInterface();
	virtual ~WindowsAudioInterface();
protected:
	virtual void enumerateDevices();
private:
	IMMDeviceEnumerator* pEnumerator;
	IMMDeviceCollection* pCollection;
	IMMDevice* defaultDevice;
	IPropertyStore* pPropStore;
	LPWSTR pwszID;

};

