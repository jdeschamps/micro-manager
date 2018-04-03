//-----------------------------------------------------------------------------
// FILE:          iBeam-smart.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controls iBeam smart laser series from Toptica through serial port
// COPYRIGHT:     EMBL
// LICENSE:       LGPL
// AUTHOR:        Joran Deschamps
//-----------------------------------------------------------------------------

#include "iBeam-smart.h"
#include <iostream>
#include <fstream>


#ifdef WIN32
#include "winuser.h"
#endif

const char* g_DeviceiBeamSmartName = "iBeamSmart";
const int maxcount = 10;

//-----------------------------------------------------------------------------
// MMDevice API
//-----------------------------------------------------------------------------

MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_DeviceiBeamSmartName, MM::GenericDevice, "Toptica iBeam smart laser");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
    if (deviceName == 0)
	   return 0;

    if (strcmp(deviceName, g_DeviceiBeamSmartName) == 0)
    {
	   return new iBeamSmart;
    }

    return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

//-----------------------------------------------------------------------------
// iBeam smart device adapter
//-----------------------------------------------------------------------------

iBeamSmart::iBeamSmart():
   port_("Undefined"),
   serial_("Undefined"),
   initialized_(false),
   busy_(false),
   powerCh1_(0.00),
   powerCh2_(0.00),
   finea_(0),
   fineb_(10),
   laserOn_(false),
   fineOn_(false),
   ch1On_(false),
   ch2On_(false),
   extOn_(false),
   maxpower_(125)
{
     InitializeDefaultErrorMessages();
     SetErrorText(ERR_PORT_CHANGE_FORBIDDEN, "You can't change the port after device has been initialized.");

     // Description
     CreateProperty(MM::g_Keyword_Description, "iBeam smart Laser Controller", MM::String, true);

     // Port
     CPropertyAction* pAct = new CPropertyAction (this, &iBeamSmart::OnPort);
     CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	 
     std::ofstream log;
	 log.open ("Log_iBeam.txt", std::ios::app);
	 log << "---------------------------------- Initialization of device\n";
	 log.close();
}

iBeamSmart::~iBeamSmart()
{
     Shutdown();
}

void iBeamSmart::GetName(char* Name) const
{
     CDeviceUtils::CopyLimitedString(Name, g_DeviceiBeamSmartName);
}

int iBeamSmart::Initialize()
{
     std::ofstream log;
	 log.open ("Log_iBeam.txt", std::ios::app);
	 log << "---------------------------------- Initialization starts\n";
	 maxpower_ = getMaxPower();
	 
	 log << "---------------------------------- Got max Power\n";

	 std::vector<std::string> commandsOnOff;
     commandsOnOff.push_back("Off");
     commandsOnOff.push_back("On");

	 // Serial number
	 serial_ = getSerial(); 
	 log << "---------------------------------- Got serial\n";
     CPropertyAction* pAct = new CPropertyAction (this, &iBeamSmart::OnSerial);
     int nRet = CreateProperty("Serial ID", serial_.c_str(), MM::String, true, pAct);
	 if (DEVICE_OK != nRet)
          return nRet;
	  
	 // Maximum power			
	 std::ostringstream os;
	 os << maxpower_;
     pAct = new CPropertyAction (this, &iBeamSmart::OnSerial);
     nRet = CreateProperty("Maximum power (mW)", os.str().c_str(), MM::String, true, pAct);
	 if (DEVICE_OK != nRet)
          return nRet;
	 
	 // Laser On/Off
	 laserOn_ = getLaserStatus();
	 log << "---------------------------------- Got laser status\n";
	 pAct = new CPropertyAction (this, &iBeamSmart::OnLaserOnOFF);
	 if(laserOn_){
		 nRet = CreateProperty("Laser Operation", "On", MM::String, false, pAct);
		 SetAllowedValues("Laser Operation", commandsOnOff);
		 if (DEVICE_OK != nRet)
			  return nRet;
	 } else {
		 nRet = CreateProperty("Laser Operation", "Off", MM::String, false, pAct);
		 SetAllowedValues("Laser Operation", commandsOnOff);
		 if (DEVICE_OK != nRet)
			  return nRet;
	 }

	 // Power channel 1
	 powerCh1_ = getPower(1);
	 log << "---------------------------------- Got ch1 Power\n";
	 pAct = new CPropertyAction (this, &iBeamSmart::OnPowerCh1);
	 nRet = CreateProperty("Ch1 Power (mW)", to_string(powerCh1_).c_str(), MM::Float, false, pAct);
	 SetPropertyLimits("Ch1 Power (mW)", 0, maxpower_);
     if (DEVICE_OK != nRet)
          return nRet;

	 // Enable channel 1
	 ch1On_ = getChannelStatus(1);
	 pAct = new CPropertyAction (this, &iBeamSmart::OnEnableCh1);
	 if(ch1On_){
		nRet = CreateProperty("Ch1 enable", "On", MM::String, false, pAct);
		SetAllowedValues("Ch1 enable", commandsOnOff);
		if (DEVICE_OK != nRet)
		    return nRet;
	 } else{
		nRet = CreateProperty("Ch1 enable", "Off", MM::String, false, pAct);
		SetAllowedValues("Ch1 enable", commandsOnOff);
		if (DEVICE_OK != nRet)
		    return nRet;
	 }

	 // Power channel 2
	 powerCh2_ = getPower(2);
	 pAct = new CPropertyAction (this, &iBeamSmart::OnPowerCh2);
     nRet = CreateProperty("Ch2 Power (mW)", to_string(powerCh2_).c_str(), MM::Float, false, pAct);
	 SetPropertyLimits("Ch2 Power (mW)", 0, maxpower_);
     if (DEVICE_OK != nRet)
          return nRet;
	 
	 // Enable channel 2
	 ch2On_ = getChannelStatus(2);
	 pAct = new CPropertyAction (this, &iBeamSmart::OnEnableCh2);
	 if(ch2On_){
		nRet = CreateProperty("Ch2 enable", "On", MM::String, false, pAct);
		SetAllowedValues("Ch2 enable", commandsOnOff);
		if (DEVICE_OK != nRet)
		    return nRet;
	 } else{
		nRet = CreateProperty("Ch2 enable", "Off", MM::String, false, pAct);
		SetAllowedValues("Ch2 enable", commandsOnOff);
		if (DEVICE_OK != nRet)
		    return nRet;
	 }

	 // Ext
	 extOn_ = getExtStatus();
	 if(extOn_){
		nRet = CreateProperty("Enable ext trigger", "On", MM::String, false, pAct);
		SetAllowedValues("Enable ext trigger", commandsOnOff);
		if (DEVICE_OK != nRet)
		    return nRet;
	 } else{
		nRet = CreateProperty("Enable ext trigger", "Off", MM::String, false, pAct);
		SetAllowedValues("Enable ext trigger", commandsOnOff);
		if (DEVICE_OK != nRet)
		    return nRet;
	 }

	 // Fine
	 fineOn_ = getFineStatus();
	 if(fineOn_){
		 pAct = new CPropertyAction (this, &iBeamSmart::OnEnableFine);
		 nRet = CreateProperty("Enable Fine", "On", MM::String, false, pAct);
		 SetAllowedValues("Enable Fine", commandsOnOff);
		 if (DEVICE_OK != nRet)
			  return nRet;
	 } else{
		 pAct = new CPropertyAction (this, &iBeamSmart::OnEnableFine);
		 nRet = CreateProperty("Enable Fine", "Off", MM::String, false, pAct);
		 SetAllowedValues("Enable Fine", commandsOnOff);
		 if (DEVICE_OK != nRet)
			  return nRet;
	 }

	 // Fine a percentage
	 finea_ = getFineValue('a');
	 pAct = new CPropertyAction (this, &iBeamSmart::OnFineA);
     nRet = CreateProperty("Fine A (%)", to_string(finea_).c_str(), MM::Float, false, pAct);
	 SetPropertyLimits("Fine A (%)", 0, 100);
     if (DEVICE_OK != nRet)
          return nRet;
	 
	 // Fine b percentage
	 fineb_ = getFineValue('b');
	 pAct = new CPropertyAction (this, &iBeamSmart::OnFineB);
     nRet = CreateProperty("Fine B (%)", to_string(fineb_).c_str(), MM::Float, false, pAct);
	 SetPropertyLimits("Fine B (%)", 0, 100);
     if (DEVICE_OK != nRet)
          return nRet;

	 log.close();
     initialized_ = true;
     return DEVICE_OK;
}

int iBeamSmart::Shutdown()
{
   if (initialized_)
   {
		setLaserOnOff(false);
		initialized_ = false;	 
   }
   return DEVICE_OK;
}

bool iBeamSmart::Busy()
{
   return busy_;
}



//---------------------------------------------------------------------------
// Conveniance functions:
//---------------------------------------------------------------------------


///// Get
std::string iBeamSmart::getSerial(){
	std::ostringstream command;
    std::string answer;
    std::string serial;
	serial = "";

    command << "id";

	std::ofstream log;
	log.open ("Log_iBeam.txt", std::ios::app);
	log << "------------------ Get serial\n";


	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return "Error";

	int counter = 0;
	while(counter<150){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		
		if (ret != DEVICE_OK){ 
			log << "ret: " << ret << " at count "<< counter <<"\n";
			break;
		}

		std::size_t found = answer.find("iBEAM");
		if (found!=std::string::npos){	
			serial = answer.substr(found);
			PurgeComPort(port_.c_str());
			break;
		}

		counter++;
	}
	
	log << "Answer: "<< serial << "\n";
	log.close();

	return serial;
}

int iBeamSmart::getMaxPower(){	
	std::ostringstream command;
    std::string answer;

	int maxpow = 0;

    command << "sh data";

	std::ofstream log;
	log.open ("Log_iBeam.txt", std::ios::app);
	log << "------------------ Get Max Power\n";


	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	bool foundline = false;
	int count = 0;
	while(count<100){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);

		if (ret != DEVICE_OK){ 
			log << "ret: " << ret << " at count "<< count <<"\n";
			break;
		}

		if(!foundline){
			std::size_t found = answer.find("Pmax:");
			if (found!=std::string::npos){	
				std::size_t found2 = answer.find(" mW");
				std::string s = answer.substr(found+5,found2-found-5);
				std::stringstream streamval(s);
				streamval >> maxpow;

				foundline = true;
			}
		} else {
			std::size_t found = answer.find("CH2 setp:");
			if(found!=std::string::npos){
				PurgeComPort(port_.c_str());
				break;
			}
		}

		count++;
	}
	log << "Pow: " << maxpow <<"\n";
	log.close();
	return maxpow;
}

double iBeamSmart::getPower(int channel){
	std::ostringstream command;
    std::string answer;

    command << "sh level pow";

	std::ofstream log;
	log.open ("Log_iBeam.txt", std::ios::app);
	log << "------------------ Get Power channel "<< channel <<" \n";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;
	
	double pow = 0;
	bool foundline = false;
	int count = 0;		
	std::ostringstream tag;
	tag << "CH" << channel <<", PWR:";
	while(count<5){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		
		if (ret != DEVICE_OK){ 
			log << "ret: " << ret << " at count "<< count <<"\n";
			break;
		}

		std::size_t found = answer.find(tag.str());
		if (found!=std::string::npos){	
			std::size_t found2 = answer.find(" mW");

			std::string s = answer.substr(found+9,found2-found-9);
			std::stringstream streamval(s);
			streamval >> pow;

			foundline = true;
		}

		if(foundline && answer.find("CH2, PWR:")!=std::string::npos){
			PurgeComPort(port_.c_str());
			break;
		}

		count++;
	}
	
	log << "Pow: "<< pow <<" \n";
	log.close();
	return pow;
}

bool iBeamSmart::getChannelStatus(int channel){
	std::ostringstream command;
    std::string answer;

    command << "sta ch "<< channel;

	std::ofstream log;
	log.open ("Log_iBeam.txt", std::ios::app);
	log << "------------------ Get channel status \n";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return false;
	
	int count = 0;
	while(count<5){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			log << "ret: " << ret << " at count "<< count <<"\n";
			break;
		}

		log << "Answer: "<< answer <<" \n";
		if (answer.find("ON") != std::string::npos){	
			log << "Answer is On \n";
			log.close();
			PurgeComPort(port_.c_str());
			return true;
		} else if (answer.find("OFF") != std::string::npos){	
			log << "Answer is Off \n";
			log.close();
			PurgeComPort(port_.c_str());
			return false;
		}

		count++;
	}
	
	log << "Answer is somethign else... \n";
	log.close();
	return false;
}

bool iBeamSmart::getFineStatus(){
	std::ostringstream command;
    std::string answer;

    command << "sta fine";

	std::ofstream log;
	log.open ("Log_iBeam.txt", std::ios::app);
	log << "------------------ Get fine status \n";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return false;
	
	int count = 0;
	while(count<5){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			log << "ret: " << ret << " at count "<< count <<"\n";
			break;
		}

		log << "Answer: "<< answer <<" \n";
		if (answer.find("ON") != std::string::npos){	
			log << "Answer is On \n";
			log.close();
			PurgeComPort(port_.c_str());
			return true;
		} else if (answer.find("OFF") != std::string::npos){	
			log << "Answer is Off \n";
			log.close();
			PurgeComPort(port_.c_str());
			return false;
		}

		count++;
	}
	
	log << "Answer is somethign else... \n";
	log.close();
	return false;
}

double iBeamSmart::getFineValue(char fine){
	std::ostringstream command;
    std::string answer;

	double perc = 0;

    command << "sh data";

	std::ofstream log;
	log.open ("Log_iBeam.txt", std::ios::app);
	log << "------------------ Get fine status \n";
	log << "Fine: "<< fine <<" \n";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	bool foundline = false;
	std::ostringstream tag;
	tag << "fine " << fine;
	int count = 0;
	while(count<100){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);

		if (ret != DEVICE_OK){ 
			log << "ret: " << ret << " at count "<< count <<"\n";
			break;
		}

		if(!foundline){
			std::size_t found = answer.find(tag.str());
			if (found!=std::string::npos){	
				std::size_t found1 = answer.find("-> ");
				std::size_t found2 = answer.find(" %");
				std::string s = answer.substr(found1+3,found2-found1-3);
				std::stringstream streamval(s);
				streamval >> perc;

				foundline = true;
			}
		} else {
			std::size_t found = answer.find("CH2 setp:");
			if(found!=std::string::npos){
				PurgeComPort(port_.c_str());
				break;
			}
		}

		count++;
	}

	log << "Perc: " << perc <<"\n";
	log.close();

	return perc;
}

bool iBeamSmart::getExtStatus(){ // might be buggy
	std::ostringstream command;
    std::string answer;

    command << "sh data";

	std::ofstream log;
	log.open ("Log_iBeam.txt", std::ios::app);
	log << "------------------ Get ext status \n";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return false;

	bool foundline = false;
	bool en = false;
	int count = 0;
	while(count<100){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);

		if (ret != DEVICE_OK){ 
			log << "ret: " << ret << " at count "<< count <<"\n";
			break;
		}

		if(!foundline){
			std::size_t found = answer.find("enable EXT:");
			if (found!=std::string::npos){	
				if(answer.find("disabled")!=std::string::npos){
					en = false;
				} else if(answer.find("enabled")!=std::string::npos){
					en = true;
				}
				foundline = true;
			}
		} else {
			std::size_t found = answer.find("CH2 setp:");
			if(found!=std::string::npos){
				PurgeComPort(port_.c_str());
				break;
			}
		}

		count++;
	}

	log << "Enabled: " << en <<"\n";
	log.close();

	return en;
}

bool iBeamSmart::getLaserStatus(){
	std::ostringstream command;
    std::string answer;

    command << "sta la";

	std::ofstream log;
	log.open ("Log_iBeam.txt", std::ios::app);
	log << "------------------ Get laser status \n";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return false;
	
	int count = 0;
	while(count<5){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			log << "ret: " << ret << " at count "<< count <<"\n";
			break;
		}

		log << "Answer: "<< answer <<" \n";
		if (answer.find("ON") != std::string::npos){	
			log << "Answer is On \n";
			log.close();
			PurgeComPort(port_.c_str());
			return true;
		} else if (answer.find("OFF") != std::string::npos){	
			log << "Answer is Off \n";
			log.close();
			PurgeComPort(port_.c_str());
			return false;
		}

		count++;
	}
	
	log << "Answer is somethign else... \n";
	log.close();
	return false;
}

/////// Set

int iBeamSmart::setLaserOnOff(bool b){
	std::ostringstream command;
    std::string answer;

	if(b){
		command << "la on";
	} else {
		command << "la off";
	}

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	return ret;
}

int iBeamSmart::enableChannel(int channel, bool b){
	std::ostringstream command;
    std::string answer;

	if(b){
		command << "en " << channel;
	} else {
		command << "di " << channel;
	}

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	return ret;
}

int iBeamSmart::setPower(int channel, double pow){
	std::ostringstream command;
    std::string answer;

	if(pow<0 || pow>maxpower_){
		return 0;
	}

	command << "ch "<< channel <<" pow " << pow;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	return ret;
}

int iBeamSmart::setFineA(double perc){
	std::ostringstream command;
    std::string answer;

	if(perc<0 || perc>100){
		return 0;
	}

	command << "fine a " << perc;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	return ret;
}

int iBeamSmart::setFineB(double perc){
	std::ostringstream command;
    std::string answer;

	if(perc<0 || perc>100){
		return 0;
	}

	command << "fine b " << perc;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	return ret;
}

int iBeamSmart::enableExt(bool b){
	std::ostringstream command;
    std::string answer;

	if(b){
		command << "en ext";
	} else {
		command << "di ext";
	}

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	return ret;
}

int iBeamSmart::enableFine(bool b){
	std::ostringstream command;
    std::string answer;

	if(b){
		command << "fine on";
	} else {
		command << "fine off";
	}

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	return ret;
}

//---------------------------------------------------------------------------
// Initial or read only properties
//---------------------------------------------------------------------------

int iBeamSmart::OnPort(MM::PropertyBase* pProp , MM::ActionType eAct)
{
     if (eAct == MM::BeforeGet)
     {
          pProp->Set(port_.c_str());
     }
     else if (eAct == MM::AfterSet)
     {
          if (initialized_)
          {
               // revert
               pProp->Set(port_.c_str());
               return ERR_PORT_CHANGE_FORBIDDEN;
          }

          pProp->Get(port_);
     }

     return DEVICE_OK;
}

int iBeamSmart::OnSerial(MM::PropertyBase* pProp, MM::ActionType eAct)
{
     if (eAct == MM::BeforeGet)
     {
          pProp->Set(serial_.c_str());
     }
     else if (eAct == MM::AfterSet)
     {
          pProp->Get(serial_);
     }

     return DEVICE_OK;
}


//---------------------------------------------------------------------------
// Action handlers
//---------------------------------------------------------------------------

int iBeamSmart::OnLaserOnOFF(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		laserOn_ = getLaserStatus();
		if(laserOn_){
			pProp->Set("On");
		} else {
			pProp->Set("Off");
		}
	} else if (eAct == MM::AfterSet){
		std::string status;
        pProp->Get(status);

		if(status.compare("On") == 0){
			laserOn_ = true;
		} else {
			laserOn_ = false;
		}

		setLaserOnOff(laserOn_);
   }

   return DEVICE_OK;
}

int iBeamSmart::OnPowerCh1(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		powerCh1_ = getPower(1);
		pProp->Set(powerCh1_);
	} else if (eAct == MM::AfterSet){
		double pow;
        pProp->Get(pow);

		powerCh1_ = pow;
		setPower(1, powerCh1_);
   }

   return DEVICE_OK;
}

int iBeamSmart::OnPowerCh2(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){
		powerCh2_ = getPower(2); 
		pProp->Set(powerCh2_);
	} else if (eAct == MM::AfterSet){
		double pow;
        pProp->Get(pow);

		powerCh2_ = pow;
		setPower(2, powerCh2_);
   }

   return DEVICE_OK;
}

int iBeamSmart::OnEnableExt(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		extOn_ = getExtStatus();
		if(extOn_){
			pProp->Set("On");
		} else {
			pProp->Set("Off");
		}
	} else if (eAct == MM::AfterSet){
		std::string status;
        pProp->Get(status);

		if(status.compare("On") == 0){
			extOn_ = true;
		} else {
			extOn_ = false;
		}

		enableExt(extOn_);

		// Now the power output is the one of ch2 previously set + bias power of ch1
		//powerCh1_ = 0;
		//setPowerCh1(0);
   }

   return DEVICE_OK;
}

int iBeamSmart::OnEnableCh1(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		ch1On_ = getChannelStatus(1);
		if(ch1On_){
			pProp->Set("On");
		} else {
			pProp->Set("Off");
		}
	} else if (eAct == MM::AfterSet){
		std::string status;
        pProp->Get(status);

		if(status.compare("On") == 0){
			ch1On_ = true;
		} else {
			ch1On_ = false;
		}

		enableChannel(1,ch1On_);
   }

   return DEVICE_OK;
}

int iBeamSmart::OnEnableCh2(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		ch2On_ = getChannelStatus(2);
		if(ch2On_){
			pProp->Set("On");
		} else {
			pProp->Set("Off");
		}
	} else if (eAct == MM::AfterSet){
		std::string status;
        pProp->Get(status);

		if(status.compare("On") == 0){
			ch2On_ = true;
		} else {
			ch2On_ = false;
		}

		enableChannel(2,ch2On_);
   }

   return DEVICE_OK;
}

int iBeamSmart::OnEnableFine(MM::PropertyBase* pProp, MM::ActionType eAct){ // here error
	if (eAct == MM::BeforeGet){ 
		std::ofstream log;
		log.open ("Log_iBeam.txt", std::ios::app);
		log << "------------------------------- change fine on/off (beforeget) \n";

		fineOn_ = getFineStatus();

		
		log << "Got fine status"<< fineOn_ <<"\n";
		if(fineOn_){
			pProp->Set("On");
			log << "Set fine prop on\n";
		} else {
			pProp->Set("Off");
			log << "Set fine prop off\n";
		}
		log << "Done\n";

		log.close();
	} else if (eAct == MM::AfterSet){
		std::ofstream log;
		log.open ("Log_iBeam.txt", std::ios::app);
		log << "------------------------------- change fine on/off  (beforeset) \n";

		
		log << "try to get status \n";

		std::string status;
        pProp->Get(status);

		
		log << "Status is "<<status<<" \n";

		if(status.compare("On") == 0){
			fineOn_ = true;
			
			log << "Status is "<<fineOn_<<" \n";
		} else {
			fineOn_ = false;
			log << "Status is "<<fineOn_<<" \n";
		}

		
		log << "Set enable fine "<< fineOn_ <<"\n";


		if(fineOn_){
			// Toptica recommends putting Fine A to 0 to avoid clipping before turning fine on
			finea_ = 0;

			log << "Set fine a = 0\n";

			setFineA(finea_);
		
			log << "Done set fine a = 0\n";

			std::ostringstream os;
			os << 0;
			OnPropertyChanged("Fine A (%)", os.str().c_str());

			log << "Changed property fine A\n";
		}

		enableFine(fineOn_); 
		
		log << "Enabled fine \n";
		
		log.close();
   }

   return DEVICE_OK;
}

int iBeamSmart::OnFineA(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		finea_ = getFineValue('a');
		pProp->Set(finea_);
	} else if (eAct == MM::AfterSet){
		double pow;
        pProp->Get(pow);

		finea_ = pow;
		setFineA(finea_);
   }

   return DEVICE_OK;
}

int iBeamSmart::OnFineB(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		fineb_ = getFineValue('b');
		pProp->Set(fineb_);
	} else if (eAct == MM::AfterSet){
		double pow;
        pProp->Get(pow);

		fineb_ = pow;
		setFineB(fineb_);
   }

   return DEVICE_OK;
}

std::string iBeamSmart::to_string(double x) {
  std::ostringstream x_convert;
  x_convert << x;
  return x_convert.str();
}