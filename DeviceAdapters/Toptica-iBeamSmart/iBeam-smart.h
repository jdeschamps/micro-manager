//-----------------------------------------------------------------------------
// FILE:          iBeam-smart.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controls iBeam smart laser series from Toptica through serial port
// COPYRIGHT:     EMBL
// LICENSE:       LGPL
// AUTHOR:        Joran Deschamps, 2018
//-----------------------------------------------------------------------------


#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ModuleInterface.h"

#include <string>
#include <map>
#include <iomanip>
#include <iostream>

//-----------------------------------------------------------------------------
// Error code
//-----------------------------------------------------------------------------

#define ERR_PORT_CHANGE_FORBIDDEN    101
#define LASER_WARNING    102
#define LASER_ERROR    103
#define LASER_FATAL_ERROR    104
#define ADAPTER_POWER_OUTSIDE_RANGE    105
#define ADAPTER_PERC_OUTSIDE_RANGE    106
#define ADAPTER_ERROR_DATA_NOT_FOUND    107

//-----------------------------------------------------------------------------

class iBeamSmart: public CGenericBase<iBeamSmart>
{
public:
    iBeamSmart();
    ~iBeamSmart();

    // MMDevice API
    int Initialize();
    int Shutdown();

    void GetName(char* pszName) const;
    bool Busy();

	// getters
	int getMaxPower(int* maxpower);
	int getPower(int channel, double* power);
	int getChannelStatus(int channel, bool* status);
	int getFineStatus(bool* status);
	int getFinePercentage(char fine, double* value);
	int getExtStatus(bool* status);
	int getLaserStatus(bool* status);
	int getSerial(std::string* serial);
	int getFirmwareVersion(std::string* version);

	// setters
	int setLaserOnOff(bool b);
	int enableChannel(int channel, bool enable);
	int setPower(int channel, double pow);
	int setFineA(double perc);
	int setFineB(double perc);
	int enableExt(bool b);
	int enableFine(bool b);
	int setPromptOff();
	int setTalkUsual();

    // action properties
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnLaserOnOff(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPowerCh1(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPowerCh2(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnEnableExt(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnEnableCh1(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnEnableCh2(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnEnableFine(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFineA(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFineB(MM::PropertyBase* pProp, MM::ActionType eAct);
	
	// convenience function 
	bool isError(std::string answer);
	bool isOk(std::string answer);
	int getError(std::string error);
	int publishError(std::string error);
	std::string to_string(double x);

private:
	std::string port_;
	std::string serial_;    
	bool initialized_;
	bool busy_;
	bool laserOn_;
	bool fineOn_;
	bool ch1On_;
	bool ch2On_;
	bool extOn_;
	int maxpower_;
	double powerCh1_;
	double powerCh2_;
	double finea_;
	double fineb_;
};
