 //////////////////////////////////////////////////////////////////////////////
// FILE:          Mojo.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Adapter for Mojo board
// COPYRIGHT:     EMBL
// LICENSE:       LGPL
//
// AUTHOR:        Joran Deschamps, EMBL, January 2017
//
//


#include "MicroMojo.h"
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <cstdio>

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #define snprintf _snprintf 
#endif

const char* g_DeviceNameMojoHub = "Mojo-Hub";
const char* g_DeviceNameMojoLaserTrig = "Mojo-LaserTrig";
const char* g_DeviceNameMojoInput = "Mojo-Input";
const char* g_DeviceNameMojoPMW = "Mojo-PMW";
const char* g_DeviceNameMojoTTL = "Mojo-TTL";
const char* g_DeviceNameMojoServos = "Mojo-Servos";

const int g_version = 1;
const int g_maxlasers = 4;
const int g_maxanaloginput = 8;
const int g_maxttl = 4;
const int g_maxpwm = 2;
const int g_maxservos = 4;

const int g_offsetaddressLaserMode = 0;
const int g_offsetaddressLaserDuration = 4;
const int g_offsetaddressLaserSequence = 8;
const int g_offsetaddressTTL = 12;
const int g_offsetaddressServo = 16;
const int g_offsetaddressPMW = 20;
const int g_offsetaddressAnalogInput = 0;

// static lock
MMThreadLock MojoHub::lock_;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_DeviceNameMojoHub, MM::HubDevice, "Hub (required)");
   RegisterDevice(g_DeviceNameMojoLaserTrig, MM::GenericDevice, "Laser Trigger");
   RegisterDevice(g_DeviceNameMojoInput, MM::GenericDevice, "Analog Input");
   RegisterDevice(g_DeviceNameMojoPMW, MM::GenericDevice, "PMW Output");
   RegisterDevice(g_DeviceNameMojoTTL, MM::GenericDevice, "TTL Output");
   RegisterDevice(g_DeviceNameMojoServos, MM::GenericDevice, "Servos");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   if (strcmp(deviceName, g_DeviceNameMojoHub) == 0)
   {
      return new MojoHub;
   }
   else if (strcmp(deviceName, g_DeviceNameMojoLaserTrig) == 0)
   {
      return new MojoLaserTrig;
   }
   else if (strcmp(deviceName, g_DeviceNameMojoInput) == 0)
   {
      return new MojoInput;
   }
   else if (strcmp(deviceName, g_DeviceNameMojoPMW) == 0)
   {
      return new MojoPWM; 
   }
   else if (strcmp(deviceName, g_DeviceNameMojoTTL) == 0)
   {
      return new MojoTTL; 
   }
   else if (strcmp(deviceName, g_DeviceNameMojoServos) == 0)
   {
      return new MojoServo;
   }

   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// CArduinoHUb implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
MojoHub::MojoHub() :
   initialized_ (false),
   version_(g_version)
{
   portAvailable_ = false;

   InitializeDefaultErrorMessages();
   SetErrorText(ERR_PORT_OPEN_FAILED, "Failed opening Mojo USB device");
   SetErrorText(ERR_BOARD_NOT_FOUND, "Did not find an Mojo board with the correct firmware. Is the Mojo board connected to this serial port?");
   SetErrorText(ERR_NO_PORT_SET, "Hub Device not found. The Mojo Hub device is needed to create this device");
   SetErrorText(ERR_VERSION_MISMATCH, "The firmware version on the Mojo is not compatible with this adapter.  Please use firmware version ");

   CPropertyAction* pAct = new CPropertyAction(this, &MojoHub::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

MojoHub::~MojoHub()
{
   Shutdown();
}

void MojoHub::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameMojoHub);
}

bool MojoHub::Busy()
{
   return false;
}

MM::DeviceDetectionStatus MojoHub::DetectDevice(void)
{
   // To rewrite

   if (initialized_)
      return MM::CanCommunicate;

   // all conditions must be satisfied...
   MM::DeviceDetectionStatus result = MM::Misconfigured;
   char answerTO[MM::MaxStrLength];
   
   try
   {
      std::string portLowerCase = port_;
      for( std::string::iterator its = portLowerCase.begin(); its != portLowerCase.end(); ++its)
      {
         *its = (char)tolower(*its);
      }
      if( 0< portLowerCase.length() &&  0 != portLowerCase.compare("undefined")  && 0 != portLowerCase.compare("unknown") )
      {
         result = MM::CanNotCommunicate;
         // record the default answer time out
         GetCoreCallback()->GetDeviceProperty(port_.c_str(), "AnswerTimeout", answerTO);

         // device specific default communication parameters
         // for Mojo 
         GetCoreCallback()->SetDeviceProperty(port_.c_str(), MM::g_Keyword_Handshaking, "0");
         GetCoreCallback()->SetDeviceProperty(port_.c_str(), MM::g_Keyword_BaudRate, "57600" );
         GetCoreCallback()->SetDeviceProperty(port_.c_str(), MM::g_Keyword_StopBits, "1");
         GetCoreCallback()->SetDeviceProperty(port_.c_str(), "AnswerTimeout", "500.0");
         GetCoreCallback()->SetDeviceProperty(port_.c_str(), "DelayBetweenCharsMs", "0");
         MM::Device* pS = GetCoreCallback()->GetDevice(this, port_.c_str());
         pS->Initialize();

         CDeviceUtils::SleepMs(100);
         MMThreadGuard myLock(lock_);
         PurgeComPort(port_.c_str());
         long v = 0;
         int ret = GetControllerVersion(v);

         if( DEVICE_OK != ret )
         {
            LogMessageCode(ret,true);
         }
         else
         {
            // to succeed must reach here....
            result = MM::CanCommunicate;
         }
         pS->Shutdown();
         // always restore the AnswerTimeout to the default
         GetCoreCallback()->SetDeviceProperty(port_.c_str(), "AnswerTimeout", answerTO);

      }
   }
   catch(...)
   {
      LogMessage("Exception in DetectDevice!",false);
   }

   return result;
}


int MojoHub::Initialize()
{
   // Name
   int ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameMojoHub, MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   MMThreadGuard myLock(lock_);

   // Check that we have a controller:
   PurgeComPort(port_.c_str());

   ret = GetControllerVersion(version_);
   if( DEVICE_OK != ret)
      return ret;

   if (g_version != version_)
      return ERR_VERSION_MISMATCH;

   CPropertyAction* pAct = new CPropertyAction(this, &MojoHub::OnVersion);
   std::ostringstream sversion;
   sversion << version_;
   CreateProperty("MicroMojo Version", sversion.str().c_str(), MM::Integer, true, pAct);

   initialized_ = true;
   return DEVICE_OK;
}

int MojoHub::DetectInstalledDevices()
{
   if (MM::CanCommunicate == DetectDevice()) 
   {
      std::vector<std::string> peripherals; 
      peripherals.clear();
      peripherals.push_back(g_DeviceNameMojoLaserTrig);
      peripherals.push_back(g_DeviceNameMojoInput);
      peripherals.push_back(g_DeviceNameMojoPMW);
      peripherals.push_back(g_DeviceNameMojoTTL);
      peripherals.push_back(g_DeviceNameMojoServos);
      for (size_t i=0; i < peripherals.size(); i++) 
      {
         MM::Device* pDev = ::CreateDevice(peripherals[i].c_str());
         if (pDev) 
         {
            AddInstalledDevice(pDev);
         }
      }
   }

   return DEVICE_OK;
}


int MojoHub::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int MojoHub::GetControllerVersion(long& version)
{
   int ret = SendReadRequest(99);
   if (ret != DEVICE_OK)
      return ret;

   ret = ReadAnswer(version);
   if (ret != DEVICE_OK)
      return ret;

   return ret;
}

int MojoHub::SendWriteRequest(long address, long value)
{   
   unsigned char command[9];
   command[0] = (1 << 7);	
   command[1] = address;	
   command[2] = (address >> 8);	
   command[3] = (address >> 16);	
   command[4] = (address >> 24);	
   command[5] = value;	
   command[6] = (value >> 8);	
   command[7] = (value >> 16);	
   command[8] = (value >> 24);	

   int ret = WriteToComPortH((const unsigned char*) command, 9);
   
   return ret;
}
int MojoHub::SendReadRequest(long address){
   unsigned char command[5];
   command[0] = (0 << 7);	
   command[1] = address;	
   command[2] = (address >> 8);	
   command[3] = (address >> 16);	
   command[4] = (address >> 24);

   int ret = WriteToComPortH((const unsigned char*) command, 5);
   
   return ret;
}

int MojoHub::ReadAnswer(long& ans){
   unsigned char* answer = new unsigned char[4];

   MM::MMTime startTime = GetCurrentMMTime();  
   unsigned long bytesRead = 0;
   
   while ((bytesRead < 4) && ( (GetCurrentMMTime() - startTime).getMsec() < 500)) {
      unsigned long bR;
      int ret = ReadFromComPortH(answer + bytesRead, 4 - bytesRead, bR);
      if (ret != DEVICE_OK)
         return ret;
	  bytesRead += bR;
   }

   int tmp = answer[3];
   for(int i=1;i<4;i++){
	  tmp = tmp << 8;
	  tmp = tmp | answer[3-i];
   }

   ans = tmp;

   return DEVICE_OK;
}

int MojoHub::OnPort(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (pAct == MM::AfterSet)
   {
      pProp->Get(port_);
      portAvailable_ = true;
   }
   return DEVICE_OK;
}

int MojoHub::OnVersion(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet)
   {
      pProp->Set((long)version_);
   }
   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////
//////
MojoLaserTrig::MojoLaserTrig() :
   initialized_ (false),
   busy_(false)
{
   InitializeDefaultErrorMessages();

   // custom error messages??

   // Description
   int ret = CreateProperty(MM::g_Keyword_Description, "Mojo laser triggering system", MM::String, true);
   assert(DEVICE_OK == ret);

   // Name
   ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameMojoLaserTrig, MM::String, true);
   assert(DEVICE_OK == ret);

   // parent ID display
   CreateHubIDProperty();

   // Number of lasers
   CPropertyAction* pAct = new CPropertyAction(this, &MojoLaserTrig::OnNumberOfLasers);
   CreateProperty("Number of lasers", "4", MM::Integer, false, pAct, true);
   SetPropertyLimits("Number of lasers", 1, g_maxlasers);
}


MojoLaserTrig::~MojoLaserTrig()
{
   Shutdown();
}

void MojoLaserTrig::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameMojoLaserTrig);
}


int MojoLaserTrig::Initialize()
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel); 


   // Allocate memory for lasers
   mode_ = new long [GetNumberOfLasers()];
   duration_ = new long [GetNumberOfLasers()];
   sequence_ = new long [GetNumberOfLasers()];

   CPropertyActionEx *pExAct;
   int nRet;

   for(unsigned int i=0;i<GetNumberOfLasers();i++){	
	   mode_[i] = 0;
	   duration_[i] = 0;
	   sequence_[i] = 0;

	   std::stringstream mode;
	   std::stringstream dura;
	   std::stringstream seq;
	   mode << "Mode" << i;
	   dura << "Duration" << i;
	   seq << "Sequence" << i;
   
	   pExAct = new CPropertyActionEx (this, &MojoLaserTrig::OnDuration,i);
	   nRet = CreateProperty(dura.str().c_str(), "0", MM::Integer, false, pExAct);
	   if (nRet != DEVICE_OK)
		  return nRet;
	   SetPropertyLimits(dura.str().c_str(), 0, 65535);   

	   pExAct = new CPropertyActionEx (this, &MojoLaserTrig::OnMode,i);
	   nRet = CreateProperty(mode.str().c_str(), "0", MM::Integer, false, pExAct);
	   if (nRet != DEVICE_OK)
		  return nRet;
	   SetPropertyLimits(mode.str().c_str(), 0, 4);

	   pExAct = new CPropertyActionEx (this, &MojoLaserTrig::OnSequence,i);
	   nRet = CreateProperty(seq.str().c_str(), "65535", MM::Integer, false, pExAct);
	   if (nRet != DEVICE_OK)
		  return nRet;
	   SetPropertyLimits(seq.str().c_str(), 0, 65535);

   }

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

  //initializeValues();

   return DEVICE_OK;
}

int MojoLaserTrig::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int MojoLaserTrig::initializeValues()
{
   for(unsigned int i=0;i<GetNumberOfLasers();i++){	
		WriteToPort(3*i,4);
		WriteToPort(3*i+1,0);
		WriteToPort(3*i+2,65535);
   }
   return DEVICE_OK;
}

int MojoLaserTrig::WriteToPort(long address, long value)
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }

   MMThreadGuard myLock(hub->GetLock());

   hub->PurgeComPortH();

   int ret = hub->SendWriteRequest(address, value);
   if (ret != DEVICE_OK)
      return ret;

   return DEVICE_OK;
}


///////////////////////////////////////
/////////// Action handlers
int MojoLaserTrig::OnNumberOfLasers(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet){
      pProp->Set(numlasers_);
   } else if (pAct == MM::AfterSet){
      pProp->Get(numlasers_);
   }
   return DEVICE_OK;
}

int MojoLaserTrig::OnMode(MM::PropertyBase* pProp, MM::ActionType pAct, long laser)
{
   if (pAct == MM::BeforeGet)
   {
      // use cached state
      pProp->Set(mode_[laser]);
   }
   else if (pAct == MM::AfterSet)
   {
      long mode;
      pProp->Get(mode);

      int ret = WriteToPort(g_offsetaddressLaserMode+laser,mode); 
	  if (ret != DEVICE_OK)
		return ret;
	  mode_[laser] = mode;
	}
   
   return DEVICE_OK;
}

int MojoLaserTrig::OnDuration(MM::PropertyBase* pProp, MM::ActionType pAct, long laser)
{
   if (pAct == MM::BeforeGet)
   {
      // use cached state
      pProp->Set(duration_[laser]);
   }
   else if (pAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

      int ret = WriteToPort(g_offsetaddressLaserDuration+laser,pos); 
	  if (ret != DEVICE_OK)
		return ret;
	  duration_[laser] = pos;
	}
   
   return DEVICE_OK;
}

int MojoLaserTrig::OnSequence(MM::PropertyBase* pProp, MM::ActionType pAct, long laser)
{
   if (pAct == MM::BeforeGet)
   {
      // use cached state
      pProp->Set(sequence_[laser]);
   }
   else if (pAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

      int ret = WriteToPort(g_offsetaddressLaserSequence+laser,pos);
	  if (ret != DEVICE_OK)
		return ret; 
	  sequence_[laser] = pos;
	}
   
   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////
//////
MojoTTL::MojoTTL() :
   initialized_ (false),
   busy_(false)
{
   InitializeDefaultErrorMessages();

   // custom error messages??

   // Description
   int ret = CreateProperty(MM::g_Keyword_Description, "Mojo TTL", MM::String, true);
   assert(DEVICE_OK == ret);

   // Name
   ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameMojoTTL, MM::String, true);
   assert(DEVICE_OK == ret);

   // parent ID display
   CreateHubIDProperty();

   // Number of lasers
   CPropertyAction* pAct = new CPropertyAction(this, &MojoTTL::OnNumberOfChannels);
   CreateProperty("Number of channels", "4", MM::Integer, false, pAct, true);
   SetPropertyLimits("Number of channels", 1, g_maxttl);
}

MojoTTL::~MojoTTL()
{
   Shutdown();
}

void MojoTTL::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameMojoTTL);
}


int MojoTTL::Initialize()
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel);

   // set property list
   // -----------------

   // State
   // -----

   // Allocate memory for TTLs
   state_ = new long [GetNumberOfChannels()];

   CPropertyActionEx *pExAct;
   int nRet;

   for(unsigned int i=0;i<GetNumberOfChannels();i++){
	   state_[i]  = 0;

	   std::stringstream sstm;
	   sstm << "State" << i;

	   pExAct = new CPropertyActionEx (this, &MojoTTL::OnState,i);
	   nRet = CreateProperty(sstm.str().c_str(), "0", MM::Integer, false, pExAct);
	   if (nRet != DEVICE_OK)
		  return nRet;
	   AddAllowedValue(sstm.str().c_str(), "0");
	   AddAllowedValue(sstm.str().c_str(), "1");
   }

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   return DEVICE_OK;
}

int MojoTTL::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int MojoTTL::WriteToPort(long address, long state)
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }

   MMThreadGuard myLock(hub->GetLock());

   hub->PurgeComPortH();

   int val = 0;
   if(state == 1){
	   val = 1;
   } 
   int ret = hub->SendWriteRequest(address, val);
   if (ret != DEVICE_OK)
      return ret;

   return DEVICE_OK;
}


///////////////////////////////////////
/////////// Action handlers
int MojoTTL::OnNumberOfChannels(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet){
      pProp->Set(numChannels_);
   } else if (pAct == MM::AfterSet){
      pProp->Get(numChannels_);
   }
   return DEVICE_OK;
}

int MojoTTL::OnState(MM::PropertyBase* pProp, MM::ActionType pAct, long channel)
{
   if (pAct == MM::BeforeGet)
   {
      // use cached state
      pProp->Set(state_[channel]);
   }
   else if (pAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

      int ret = WriteToPort(g_offsetaddressTTL+channel, pos); 
	  if (ret != DEVICE_OK)
		return ret;
	  state_[channel] = pos;
	}
   
   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////
//////
MojoServo::MojoServo() :
   initialized_ (false),
   busy_(false)
{
   InitializeDefaultErrorMessages();

   // custom error messages??

   // Description
   int ret = CreateProperty(MM::g_Keyword_Description, "Mojo Servo controller", MM::String, true);
   assert(DEVICE_OK == ret);

   // Name
   ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameMojoServos, MM::String, true);
   assert(DEVICE_OK == ret);

   // parent ID display
   CreateHubIDProperty();

   // Number of lasers
   CPropertyAction* pAct = new CPropertyAction(this, &MojoServo::OnNumberOfServos);
   CreateProperty("Number of Servos", "4", MM::Integer, false, pAct, true);
   SetPropertyLimits("Number of Servos", 1, g_maxservos);
}

MojoServo::~MojoServo()
{
   Shutdown();
}

void MojoServo::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameMojoServos);
}


int MojoServo::Initialize()
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel); 

   // set property list
   // -----------------

   // State
   // -----

   // Allocate memory for servos
   position_ = new long [GetNumberOfServos()];

   CPropertyActionEx *pExAct;
   int nRet;

   for(unsigned int i=0;i<GetNumberOfServos();i++){	
	   position_[i] = 0;

	   std::stringstream sstm;
	   sstm << "Position" << i;

	   pExAct = new CPropertyActionEx (this, &MojoServo::OnPosition,i);
	   nRet = CreateProperty(sstm.str().c_str(), "0", MM::Integer, false, pExAct);
	   if (nRet != DEVICE_OK)
		  return nRet;
	   SetPropertyLimits(sstm.str().c_str(), 0, 65535);
   }

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   return DEVICE_OK;
}

int MojoServo::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int MojoServo::WriteToPort(long address, long value)
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }

   MMThreadGuard myLock(hub->GetLock());

   hub->PurgeComPortH();


   int ret = hub->SendWriteRequest(address, value);
   if (ret != DEVICE_OK)
      return ret;

   return DEVICE_OK;
}


///////////////////////////////////////
/////////// Action handlers
int MojoServo::OnNumberOfServos(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet){
      pProp->Set(numServos_);
   } else if (pAct == MM::AfterSet){
      pProp->Get(numServos_);
   }
   return DEVICE_OK;
}

int MojoServo::OnPosition(MM::PropertyBase* pProp, MM::ActionType pAct, long servo)
{
   if (pAct == MM::BeforeGet)
   {
      // use cached state
      pProp->Set(position_[servo]);
   }
   else if (pAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

      int ret = WriteToPort(g_offsetaddressServo+servo,pos); 
	  if (ret != DEVICE_OK)
		return ret;
	  position_[servo] = pos;
	}
   
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////
//////
MojoPWM::MojoPWM() :
   initialized_ (false),
   busy_(false)
{
   InitializeDefaultErrorMessages();

   // custom error messages??

   // Description
   int ret = CreateProperty(MM::g_Keyword_Description, "Mojo PMW controller", MM::String, true);
   assert(DEVICE_OK == ret);

   // Name
   ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameMojoPMW, MM::String, true);
   assert(DEVICE_OK == ret);

   // parent ID display
   CreateHubIDProperty();

   // Number of lasers
   CPropertyAction* pAct = new CPropertyAction(this, &MojoPWM::OnNumberOfChannels);
   CreateProperty("Number of PMW", "2", MM::Integer, false, pAct, true);
   SetPropertyLimits("Number of PMW", 1, g_maxpwm);
}

MojoPWM::~MojoPWM()
{
   Shutdown();
}

void MojoPWM::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameMojoPMW);
}


int MojoPWM::Initialize()
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel); 

   // set property list
   // -----------------

   // State
   // -----

   // Allocate memory for channels
   state_ = new long [GetNumberOfChannels()];

   CPropertyActionEx *pExAct;
   int nRet;

   for(unsigned int i=0;i<GetNumberOfChannels();i++){
	   state_[i] = 0;

	   std::stringstream sstm;
	   sstm << "Position" << i;

	   pExAct = new CPropertyActionEx (this, &MojoPWM::OnState,i);
	   nRet = CreateProperty(sstm.str().c_str(), "0", MM::Integer, false, pExAct);
	   if (nRet != DEVICE_OK)
		  return nRet;
	   SetPropertyLimits(sstm.str().c_str(), 0, 255);
   }

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   return DEVICE_OK;
}

int MojoPWM::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int MojoPWM::WriteToPort(long address, long position)
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }

   MMThreadGuard myLock(hub->GetLock());

   hub->PurgeComPortH();

   int ret = hub->SendWriteRequest(address, position);
   if (ret != DEVICE_OK)
      return ret;

   return DEVICE_OK;
}


///////////////////////////////////////
/////////// Action handlers
int MojoPWM::OnNumberOfChannels(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet){
      pProp->Set(numChannels_);
   } else if (pAct == MM::AfterSet){
      pProp->Get(numChannels_);
   }
   return DEVICE_OK;
}

int MojoPWM::OnState(MM::PropertyBase* pProp, MM::ActionType pAct, long channel)
{
   if (pAct == MM::BeforeGet)
   {
      // use cached state
      pProp->Set(state_[channel]);
   }
   else if (pAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

	  if(pos<0 || pos>255){
		  pos = 0;
	  }

      int ret = WriteToPort(g_offsetaddressPMW+channel,pos); 
	  if (ret != DEVICE_OK)
		return ret;
	  state_[channel] = pos;
	}
   
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////
//////
MojoInput::MojoInput() :
   initialized_ (false)
{
   InitializeDefaultErrorMessages();

   // custom error messages??

   // Description
   int ret = CreateProperty(MM::g_Keyword_Description, "Mojo AnalogInput", MM::String, true);
   assert(DEVICE_OK == ret);

   // Name
   ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameMojoInput, MM::String, true);
   assert(DEVICE_OK == ret);

   // parent ID display
   CreateHubIDProperty();

   // Number of lasers
   CPropertyAction* pAct = new CPropertyAction(this, &MojoInput::OnNumberOfChannels);
   CreateProperty("Number of channels", "3", MM::Integer, false, pAct, true);
   SetPropertyLimits("Number of channels", 1, g_maxanaloginput);
}

MojoInput::~MojoInput()
{
   Shutdown();
}

void MojoInput::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameMojoInput);
}

bool MojoInput::Busy()
{
   return false;
}

int MojoInput::Initialize()
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel);

   // set property list
   // -----------------

   // State
   // -----

   // Allocate memory for inputs
   state_ = new long [GetNumberOfChannels()];

   CPropertyActionEx *pExAct;
   int nRet;

   for(unsigned int i=0;i<GetNumberOfChannels();i++){
	   state_[i] = 0;

	   std::stringstream sstm;
	   sstm << "AnalogInput" << i;

	   pExAct = new CPropertyActionEx (this, &MojoInput::OnAnalogInput,i);
	   nRet = CreateProperty(sstm.str().c_str(), "0", MM::Integer, true, pExAct);
	   if (nRet != DEVICE_OK)
		  return nRet;
   }

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   return DEVICE_OK;
}

int MojoInput::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int MojoInput::WriteToPort(long address)
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }

   MMThreadGuard myLock(hub->GetLock());

   hub->PurgeComPortH();

   int ret = hub->SendReadRequest(address);
   if (ret != DEVICE_OK)
      return ret;

   return DEVICE_OK;
}

int MojoInput::ReadFromPort(long& answer)
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub) {
      return ERR_NO_PORT_SET;
   }
   int ret = hub->ReadAnswer(answer);
   if (ret != DEVICE_OK)
      return ret;

   return DEVICE_OK;
}
///////////////////////////////////////
/////////// Action handlers
int MojoInput::OnNumberOfChannels(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet){
      pProp->Set(numChannels_);
   } else if (pAct == MM::AfterSet){
      pProp->Get(numChannels_);
   }
   return DEVICE_OK;
}

int MojoInput::OnAnalogInput(MM::PropertyBase* pProp, MM::ActionType pAct, long channel)
{
   MojoHub* hub = static_cast<MojoHub*>(GetParentHub());
   if (!hub)
      return ERR_NO_PORT_SET;

   if (pAct == MM::BeforeGet)
   {
      MMThreadGuard myLock(hub->GetLock());

      int ret = hub->SendReadRequest(g_offsetaddressAnalogInput+channel);
      if (ret != DEVICE_OK)
         return ret;

	  long answer;
      ret = ReadFromPort(answer);
      if (ret != DEVICE_OK)
         return ret;


      pProp->Set(answer);
	  state_[channel]=answer;
   }
   return DEVICE_OK;
}
