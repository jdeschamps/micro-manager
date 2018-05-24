//-----------------------------------------------------------------------------
// FILE:          iBeam-smart.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controls iBeam smart laser series from Toptica through serial port
// COPYRIGHT:     EMBL
// LICENSE:       LGPL
// AUTHOR:        Joran Deschamps, 2018
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
	clip_("Undefined"),
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
	SetErrorText(LASER_WARNING, "The laser has emitted a warning error, please refer to the CoreLog for the warning code.");
	SetErrorText(LASER_ERROR, "The laser has emitted an error, please refer to the CoreLog for the error code.");
	SetErrorText(LASER_FATAL_ERROR, "The laser has emitted a fatal error, please refer to the CoreLog for the error code.");
	SetErrorText(ADAPTER_POWER_OUTSIDE_RANGE, "The specified power is outside the range (0<=power<= max power).");
	SetErrorText(ADAPTER_PERC_OUTSIDE_RANGE, "The specified percentage is outside the range (0<=percentage<=100).");
	SetErrorText(ADAPTER_ERROR_DATA_NOT_FOUND, "Some data could not be extracted, consult the CoreLog.");
	SetErrorText(ADAPTER_CANNOT_CHANGE_CH2_EXT_ON, "Channel2 cannot be (de)activated when external trigger is ON.");

	// Description
	CreateProperty(MM::g_Keyword_Description, "iBeam smart Laser Controller", MM::String, true, 0, true);

	// Port
	CPropertyAction* pAct = new CPropertyAction (this, &iBeamSmart::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
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
	// Make sure prompting ("CMD>") is off (so we get [OK] or error for every answer) 
	// and "talk" is set to usual.
	// Otherwise we will get infinite loops (because we are looking for "[OK]")
	// and some of the data will not be found (e.g. enable EXT from CMD>sh data)
	int nRet = setPromptOff();
	if (DEVICE_OK != nRet)
		return nRet;
	
	nRet = setTalkUsual();
	if (DEVICE_OK != nRet)
		return nRet;

	std::vector<std::string> commandsOnOff;
	commandsOnOff.push_back("Off");
	commandsOnOff.push_back("On");

	//////////////////////////////////////////////
	// Read only properties

	// Serial number
	nRet = getSerial(&serial_); 
	if (DEVICE_OK != nRet)
		return nRet;

	nRet = CreateProperty("Serial ID", serial_.c_str(), MM::String, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Maximum power
	nRet = getMaxPower(&maxpower_);
	if (DEVICE_OK != nRet)
		return nRet;

	nRet = CreateProperty("Maximum power (mW)", to_string(maxpower_).c_str(), MM::String, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Firmware version
	std::string version;
	nRet = getFirmwareVersion(&version);
	if (DEVICE_OK != nRet)
		return nRet;
	
	nRet = CreateProperty("Firmware version", version.c_str(), MM::String, true);
	if (DEVICE_OK != nRet)
		return nRet;

	// Clipping status
	nRet = getClipStatus(&clip_);
	if (DEVICE_OK != nRet)
		return nRet;

	CPropertyAction*  pAct = new CPropertyAction (this, &iBeamSmart::OnClip);
	nRet = CreateProperty("Clipping status", clip_.c_str(), MM::String, true, pAct);
	if (DEVICE_OK != nRet)
		return nRet;


	//////////////////////////////////////////////
	// Properties
	// Laser On/Off
	nRet = getLaserStatus(&laserOn_);

	pAct = new CPropertyAction (this, &iBeamSmart::OnLaserOnOff);
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
	nRet = getPower(1,&powerCh1_);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &iBeamSmart::OnPowerCh1);
	nRet = CreateProperty("Ch1 power (mW)", to_string(powerCh1_).c_str(), MM::Float, false, pAct);
	SetPropertyLimits("Ch1 power (mW)", 0, maxpower_);
	if (DEVICE_OK != nRet)
		return nRet;

	// Enable channel 1
	nRet = getChannelStatus(1,&ch1On_);
	if (DEVICE_OK != nRet)
		return nRet;

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
	nRet = getPower(2, &powerCh2_);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &iBeamSmart::OnPowerCh2);
	nRet = CreateProperty("Ch2 power (mW)", to_string(powerCh2_).c_str(), MM::Float, false, pAct);
	SetPropertyLimits("Ch2 power (mW)", 0, maxpower_);
	if (DEVICE_OK != nRet)
		return nRet;

	// Enable channel 2
	nRet = getChannelStatus(2, &ch2On_);
	if (DEVICE_OK != nRet)
		return nRet;

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
	nRet = getExtStatus(&extOn_);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &iBeamSmart::OnEnableExt);
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
	nRet = getFineStatus(&fineOn_);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &iBeamSmart::OnEnableFine);
	if(fineOn_){
		nRet = CreateProperty("Enable Fine", "On", MM::String, false, pAct);
		SetAllowedValues("Enable Fine", commandsOnOff);
		if (DEVICE_OK != nRet)
			return nRet;
	} else{
		nRet = CreateProperty("Enable Fine", "Off", MM::String, false, pAct);
		SetAllowedValues("Enable Fine", commandsOnOff);
		if (DEVICE_OK != nRet)
			return nRet;
	}

	// Fine a percentage
	nRet = getFinePercentage('a',&finea_);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &iBeamSmart::OnFineA);
	nRet = CreateProperty("Fine A (%)", to_string(finea_).c_str(), MM::Float, false, pAct);
	SetPropertyLimits("Fine A (%)", 0, 100);
	if (DEVICE_OK != nRet)
		return nRet;

	// Fine b percentage
	nRet = getFinePercentage('b', &fineb_);
	if (DEVICE_OK != nRet)
		return nRet;

	pAct = new CPropertyAction (this, &iBeamSmart::OnFineB);
	nRet = CreateProperty("Fine B (%)", to_string(fineb_).c_str(), MM::Float, false, pAct);
	SetPropertyLimits("Fine B (%)", 0, 100);
	if (DEVICE_OK != nRet)
		return nRet;

	initialized_ = true;
	return DEVICE_OK;
}

int iBeamSmart::Shutdown()
{
	if (initialized_)
	{
		setLaserOnOff(false); // The ibeamSmart software doesn't turn off the laser when stoping, I prefer to do it
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

bool iBeamSmart::isOk(std::string answer){
	if(answer.empty()){
		return false;
	}

	// checks that the laser is ready to receive a new command
	if(answer.find("[OK]")  != std::string::npos){
		return true;
	}
	return false;
}

bool iBeamSmart::isError(std::string answer){
	// check if starts with %SYS but is not an information (as opposed to errors, warnings and fatal errors)
	if(answer.substr(0,4).compare("%SYS") == 0 && answer.find("I") == std::string::npos){ 
		return true;
	}
	return false;
}

int iBeamSmart::getError(std::string error){
	std::string s = error.substr(5,1);
	if(s.compare("W") == 0){ // warning
		return LASER_WARNING;
	} else if(s.compare("E") == 0) { // error
		return LASER_ERROR;
	} else if(s.compare("F") == 0) { // fatal error
		return LASER_FATAL_ERROR;
	}
	return DEVICE_OK;
}

int iBeamSmart::publishError(std::string error){
	std::stringstream log;
	log << "iBeamSmart error: " << error;
	LogMessage(log.str(), false);

	// Make sure that in case of an error, a [OK] prompt 
	// is not interferring with the next command (to be tested)
	PurgeComPort(port_.c_str());

	return getError(error);
}

std::string iBeamSmart::to_string(double x) {
	std::ostringstream x_convert;
	x_convert << x;
	return x_convert.str();
}


//---------------------------------------------------------------------------
// Getters:
//---------------------------------------------------------------------------

int iBeamSmart::getSerial(std::string* serial){
	std::ostringstream command;
	command << "id";

	std::string answer;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	// the loop should end when the laser is ready to receive a new command
	// i.e. when it answers "[OK]"
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK)
			return ret;

		// if the line contains iBEAM, then extract the serial
		std::size_t found = answer.find("iBEAM");
		if (found!=std::string::npos){	
			*serial = answer.substr(found);
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::getClipStatus(std::string* status){
	std::ostringstream command;
	command << "sta clip";

	std::string answer;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// The answer is not just space or [OK].
		if(answer.find_first_not_of(' ') != std::string::npos && !isOk(answer)){ 
			*status = answer;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}
	return DEVICE_OK;
}

int iBeamSmart::getMaxPower(int* maxpower){	
	std::ostringstream command;
	std::string answer;
	command << "sh data";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	bool foundline = false;
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the Pmax line has not been found yet
		if(!foundline){
			std::size_t found = answer.find("Pmax:"); 
			if (found!=std::string::npos){ // if Pmax: is found in the answer
				std::size_t found2 = answer.find(" mW");
				std::string s = answer.substr(found+5,found2-found-5);
				std::stringstream streamval(s);

				int pow; // what if double?
				streamval >> pow;
				*maxpower = pow;

				foundline = true;
			}
		} 

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	if(!foundline){
		LogMessage("Could not extract Pmax from CMD>sh data",false);
		return ADAPTER_ERROR_DATA_NOT_FOUND;
	}

	return DEVICE_OK;
}

int iBeamSmart::getPower(int channel, double* power){
	std::ostringstream command;
	std::string answer;

	command << "sh level pow";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	// Tag we are loking for
	std::ostringstream tag;
	tag << "CH" << channel <<", PWR:";

	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// test if the tag is found in the answer
		std::size_t found = answer.find(tag.str());
		if(found!=std::string::npos){	
			std::size_t found2 = answer.find(" mW");

			std::string s = answer.substr(found+9,found2-found-9); // length of the tag = 9
			std::stringstream streamval(s);
			double pow;
			streamval >> pow;

			*power = pow;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::getChannelStatus(int channel, bool* status){
	std::ostringstream command;
	std::string answer;

	command << "sta ch "<< channel;

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		if (answer.find("ON") != std::string::npos){
			*status = true;
		} else if (answer.find("OFF") != std::string::npos){	
			*status = false;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::getFineStatus(bool* status){
	std::ostringstream command;
	std::string answer;

	command << "sta fine";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		if (answer.find("ON") != std::string::npos){	
			*status = true;
		} else if (answer.find("OFF") != std::string::npos){	
			*status = false;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::getFinePercentage(char fine, double* percentage){
	std::ostringstream command;
	std::string answer;

	command << "sh data";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	// Tag we are looking for
	std::ostringstream tag;
	tag << "fine " << fine;

	bool foundline = false;
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		if(!foundline){
			std::size_t found = answer.find(tag.str());
			if (found!=std::string::npos){	
				std::size_t found1 = answer.find("-> "); // length = 3
				std::size_t found2 = answer.find(" %");
				std::string s = answer.substr(found1+3,found2-found1-3);
				std::stringstream streamval(s);

				double perc;
				streamval >> perc;

				*percentage = perc;

				foundline = true;
			}
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}
		
	if(!foundline){
		LogMessage("Could not extract fine percentage from CMD>sh data",false);
		return ADAPTER_ERROR_DATA_NOT_FOUND;
	}

	return DEVICE_OK;
}

int iBeamSmart::getExtStatus(bool* status){
	std::ostringstream command;
	std::string answer;

	command << "sta ext"; // this command doesn't appear in the manual but is available from "help" comand

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		if (answer.find("ON") != std::string::npos){	
			*status = true;
		} else if (answer.find("OFF") != std::string::npos){	
			*status = false;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::getLaserStatus(bool* status){
	std::ostringstream command;
	std::string answer;

	command << "sta la";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		if (answer.find("ON") != std::string::npos){	
			*status = true;
		} else if (answer.find("OFF") != std::string::npos){	
			*status = false;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::getFirmwareVersion(std::string* version){
	std::ostringstream command;
	std::string answer;

	command << "ver";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK) 
		return ret;

	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		if(answer.find("iB") != std::string::npos){	
			*version = answer;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}


//---------------------------------------------------------------------------
// Setters:
//---------------------------------------------------------------------------

int iBeamSmart::setLaserOnOff(bool b){
	std::ostringstream command;
	std::string answer;

	if(b){
		command << "la on";
	} else {
		command << "la off";
	}

	// send command
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");		
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}
	
	return DEVICE_OK;
}

int iBeamSmart::setPromptOff(){
	std::ostringstream command;
	std::string answer;

	command << "prom off";

	// send command
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");		
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}
	
	return DEVICE_OK;
}

int iBeamSmart::setTalkUsual(){
	std::ostringstream command;
	std::string answer;

	command << "talk usual";

	// send command
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");		
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::enableChannel(int channel, bool b){
	std::ostringstream command;
	std::string answer;

	if(b){
		command << "en " << channel;
	} else {
		command << "di " << channel;
	}

	// send command
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");		
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::setPower(int channel, double pow){
	std::ostringstream command;
	std::string answer;

	if(pow<0 || pow>maxpower_){
		return ADAPTER_POWER_OUTSIDE_RANGE;
	}

	command << "ch "<< channel <<" pow " << pow;

	// send command
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");		
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::setFineA(double perc){
	std::ostringstream command;
	std::string answer;

	if(perc<0 || perc>100){
		return ADAPTER_PERC_OUTSIDE_RANGE;
	}

	command << "fine a " << perc;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::setFineB(double perc){
	std::ostringstream command;
	std::string answer;

	if(perc<0 || perc>100){
		return ADAPTER_PERC_OUTSIDE_RANGE;
	}

	command << "fine b " << perc;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
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
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
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
	if (ret != DEVICE_OK){ 
		return ret;
	}

	// get answer until [OK]
	while(!isOk(answer)){
		ret = GetSerialAnswer(port_.c_str(), "\r", answer);
		if (ret != DEVICE_OK){ 
			return ret;
		}

		// if the laser has an error
		if(isError(answer)){
			return publishError(answer);
		}
	}

	return DEVICE_OK;
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

int iBeamSmart::OnClip(MM::PropertyBase* pProp , MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		int ret = getClipStatus(&clip_);
		if(ret != DEVICE_OK)
			return ret;
		pProp->Set(clip_.c_str());
	}

	return DEVICE_OK;
}


//---------------------------------------------------------------------------
// Action handlers
//---------------------------------------------------------------------------

int iBeamSmart::OnLaserOnOff(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		int ret = getLaserStatus(&laserOn_);
		if(ret != DEVICE_OK)
			return ret;

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

		int ret = setLaserOnOff(laserOn_);
		if(ret != DEVICE_OK)
			return ret;
	}
	return DEVICE_OK;
}

int iBeamSmart::OnPowerCh1(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		int ret = getPower(1, &powerCh1_);
		if(ret != DEVICE_OK)
			return ret;

		pProp->Set(powerCh1_);
	} else if (eAct == MM::AfterSet){
		double pow;
		pProp->Get(pow);

		powerCh1_ = pow;
		int ret = setPower(1, powerCh1_);
		if(ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;
}

int iBeamSmart::OnPowerCh2(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){
		int ret = getPower(2, &powerCh2_); 
		if(ret != DEVICE_OK)
			return ret;

		pProp->Set(powerCh2_);
	} else if (eAct == MM::AfterSet){
		double pow;
		pProp->Get(pow);

		powerCh2_ = pow;
		int ret = setPower(2, powerCh2_);
		if(ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;
}

int iBeamSmart::OnEnableExt(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		int ret = getExtStatus(&extOn_);
		if(ret != DEVICE_OK)
			return ret;

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

		int ret = enableExt(extOn_);
		if(ret != DEVICE_OK)
			return ret;

		// Now the power output is the one of ch2 previously set + bias power of ch1
		//
		//powerCh1_ = 0;
		//setPowerCh1(0);
	}

	return DEVICE_OK;
}

int iBeamSmart::OnEnableCh1(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		int ret = getChannelStatus(1, &ch1On_);
		if(ret != DEVICE_OK)
			return ret;

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

		int ret = enableChannel(1,ch1On_);
		if(ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;
}

int iBeamSmart::OnEnableCh2(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		int ret = getChannelStatus(2, &ch2On_);
		if(ret != DEVICE_OK)
			return ret;
		if(ch2On_){
			pProp->Set("On");
		} else {
			pProp->Set("Off");
		}
	} else if (eAct == MM::AfterSet){
		bool extenabled;
		int ret = getExtStatus(&extenabled);
		if(ret != DEVICE_OK)
			return ret;

		// if the external trigger is enabled, channel 2 activation is not possible
		if(!extenabled){
			std::string status;
			pProp->Get(status);

			if(status.compare("On") == 0){
				ch2On_ = true;
			} else {
				ch2On_ = false;
			}

			ret = enableChannel(2,ch2On_);
			if(ret != DEVICE_OK)
				return ret;
		} else {
			return ADAPTER_CANNOT_CHANGE_CH2_EXT_ON;
		}
	}

	return DEVICE_OK;
}

int iBeamSmart::OnEnableFine(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		int ret = getFineStatus(&fineOn_);
		if(ret != DEVICE_OK)
			return ret;

		if(fineOn_){
			pProp->Set("On");
		} else {
			pProp->Set("Off");
		}
	} else if (eAct == MM::AfterSet){
		std::string status;
		pProp->Get(status);

		if(status.compare("On") == 0){
			fineOn_ = true;
		} else {
			fineOn_ = false;
		}

		if(fineOn_){
			// Put ch2 power to 0, as the overall power must be transferred to ch 1
			int ret = setPower(2,0.);
			if(ret != DEVICE_OK)
				return ret;

			// Toptica recommends putting Fine A to 0 to avoid clipping before turning fine on
			// (this should be tried out)
			finea_ = 0;
			ret  = setFineA(finea_);
			if(ret != DEVICE_OK)
				return ret;

			ret = OnPropertyChanged("Fine A (%)", "0");
			if(ret != DEVICE_OK)
				return ret;
		}

		int ret = enableFine(fineOn_);
		if(ret != DEVICE_OK)
			return ret; ;
	}

	return DEVICE_OK;
}

int iBeamSmart::OnFineA(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){
		int ret = getFinePercentage('a', &finea_);
		if(ret != DEVICE_OK)
			return ret;

		pProp->Set(finea_);
	} else if (eAct == MM::AfterSet){
		double perc;
		pProp->Get(perc);

		finea_ = perc;
		int ret = setFineA(finea_);
		if(ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;
}

int iBeamSmart::OnFineB(MM::PropertyBase* pProp, MM::ActionType eAct){
	if (eAct == MM::BeforeGet){ 
		int ret = getFinePercentage('b', &finea_);
		if(ret != DEVICE_OK)
			return ret;

		pProp->Set(fineb_);
	} else if (eAct == MM::AfterSet){
		double perc;
		pProp->Get(perc);

		fineb_ = perc;
		int ret = setFineB(fineb_);
		if(ret != DEVICE_OK)
			return ret;
	}

	return DEVICE_OK;
}