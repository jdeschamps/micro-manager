//-----------------------------------------------------------------------------
// FILE:          iBeam-smart.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controls iBeam smart laser series from Toptica through serial port
// COPYRIGHT:     EMBL
// LICENSE:       LGPL
// AUTHOR:        Joran Deschamps
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
#define ERR_MAXPOWER_CHANGE_FORBIDDEN    102

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
	int getMaxPower();
	double getPower(int channel);
	bool getChannelStatus(int channel);
	bool getFineStatus();
	double getFineValue(char fine);
	bool getExtStatus();
	bool getLaserStatus();

	std::string getSerial();
	int setLaserOnOff(bool b);
	int enableChannel(int channel, bool b);
	int setPower(int channel, double pow);
	int setFineA(double perc);
	int setFineB(double perc);
	int enableExt(bool b);
	int enableFine(bool b);

    // action properties
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSerial(MM::PropertyBase* pProp, MM::ActionType eAct);
    int OnLaserOnOFF(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPowerCh1(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPowerCh2(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnEnableExt(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnEnableCh1(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnEnableCh2(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnEnableFine(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFineA(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFineB(MM::PropertyBase* pProp, MM::ActionType eAct);

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
