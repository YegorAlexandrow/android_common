#ifndef SOLICALL_API_H
#define SOLICALL_API_H

#ifdef _SOLICALL_DLL
#define SOLICALLSDK_STDCALL __stdcall
#define SOLICALL_RC_DLL  int
#else // If not in DLL mode
#define SOLICALLSDK_STDCALL
#define SOLICALL_RC_DLL SOLICALL_RC
#endif


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef _WINDOWS
#include <windows.h>
#endif
#ifdef _WINCE
#include <windows.h>
#endif



#ifdef _LINUX
typedef short SHORT;
typedef unsigned char  BYTE;
typedef char  CHAR;
#endif

// In our evaluation package we have time limit (in minutes) for every call
#ifdef _DEMO
#define MINUTES_LIMIT_IN_DEMO  2
#endif


// channels allowed are 0,1,..,(MAX_NUM_SOLICALL_SDK_CHANNELS-1)
#define MAX_NUM_SOLICALL_SDK_CHANNELS 2 

#define MAX_NUM_SOLICALL_SDK_AEC_CHANNELS 2

//RNR mechanism
#define MAX_NUM_SOLICALL_SDK_AEC_REFERENCES 1 // 10   // 6 

// Source Separation
#define SOURCE_SEPARATION_MAX_REAL_FREQUENCY_RATIO  6 // 2 //4



// AEC mechanisms - REGULAR and/or RNR
#define SOLICALL_ENABLED_AEC_MECHANISM_ECHO_ONLY     0x1
#define SOLICALL_ENABLED_AEC_MECHANISM_RNR_ONLY      0x2
#define SOLICALL_ENABLED_AEC_MECHANISM_ECHO_AND_RNR  0x3

#define SOLICALL_ENABLED_AEC_MECHANISM      SOLICALL_ENABLED_AEC_MECHANISM_ECHO_AND_RNR


#define MAX_LINE_SIZE_IN_CONF_FILE 10000
#define SIZE_OF_EXTERNAL_FILE 300 // including the path 



/* SOLICALL_RC Definitions */

typedef BYTE SOLICALL_RC;

#define SOLICALL_RC_SUCCESS 0 // execution was successful
#define SOLICALL_RC_ERROR 1   // error in this command
#define SOLICALL_RC_FATAL 2   // program should be terminated


    
    

#define SOLICALL_API_VERSION 6

// Detect Aggressive Level is a number between 0 to 4
#define SOLICALL_LOWEST_DETECT_AGGRESSIVE_LEVEL 0
#define SOLICALL_HIGHEST_DETECT_AGGRESSIVE_LEVEL 4

// Clean Aggressive Level is a number between 0 to 12
#define SOLICALL_LOWEST_CLEAN_AGGRESSIVE_LEVEL   0
#define SOLICALL_HIGHEST_CLEAN_AGGRESSIVE_LEVEL 12


// CPU Power is a number between 0 to 10. 
#define SOLICALL_LOWEST_CPU_POWER 0
#define SOLICALL_HIGHEST_CPU_POWER 10

// Frame size is a number between 1 to 8. 
#define SOLICALL_MIN_FRAME_SIZE 1
#define SOLICALL_MAX_FRAME_SIZE 8


// the higher the number the more aggressive the AEC will be
#define SOLICALL_LOWEST_MAXCOEFINAECPARAM 10
#define SOLICALL_HIGHEST_MAXCOEFINAECPARAM 1000

// the higher the number the more aggressive the AEC will be
#define SOLICALL_LOWEST_MINCOEFINAECPARAM 1
#define SOLICALL_HIGHEST_MINCOEFINAECPARAM 150

#define SOLICALL_ENABLE_VERY_LONG_TAIL
#ifdef SOLICALL_ENABLE_VERY_LONG_TAIL
    // to enable echoPath of up to 2900 msec
    #define SOLICALL_LOWEST_AEC_TAIL_TYPE -58
    #define SOLICALL_HIGHEST_AEC_TAIL_TYPE 5

    #define SOLICALL_LOWEST_AEC_MIN_TAIL_TYPE -58
    #define SOLICALL_HIGHEST_AEC_MIN_TAIL_TYPE 0
#else
    // to enable echoPath of up to 1900 msec
    #define SOLICALL_LOWEST_AEC_TAIL_TYPE -38
    #define SOLICALL_HIGHEST_AEC_TAIL_TYPE 3

    #define SOLICALL_LOWEST_AEC_MIN_TAIL_TYPE -38
    #define SOLICALL_HIGHEST_AEC_MIN_TAIL_TYPE 0
#endif

#define SOLICALL_MIN_AEC_ROBUSTNESS_LEVEL 0
#define SOLICALL_MAX_AEC_ROBUSTNESS_LEVEL 20

#define SOLICALL_MIN_AEC_STABILITY_LEVEL 0
#define SOLICALL_MAX_AEC_STABILITY_LEVEL 20

#define SOLICALL_MIN_AEC_SPECIAL_AGGRESSIVE_LEVEL 0
#define SOLICALL_MAX_AEC_SPECIAL_AGGRESSIVE_LEVEL 20

#define SOLICALL_MIN_AEC_ENVIRONEMT_TYPE 0
#define SOLICALL_MAX_AEC_ENVIRONEMT_TYPE 20



#define SOLICALL_MIN_AEC_TYPE 0
#define SOLICALL_MAX_AEC_TYPE 23

#define SOLICALL_MIN_AEC_ASYNC_DELAY  1
#define SOLICALL_MAX_AEC_ASYNC_DELAY 50

#define SOLICALL_MIN_AEC_SENSITIVITY_LEVEL  0
#define SOLICALL_MAX_AEC_SENSITIVITY_LEVEL 10

#define SOLICALL_MIN_AEC_STARTUP_AGGRESSIVE_LEVEL  0
#define SOLICALL_MAX_AEC_STARTUP_AGGRESSIVE_LEVEL 20

#define SOLICALL_MIN_AEC_AGGRESSIVE_LEVEL  0
#define SOLICALL_MAX_AEC_AGGRESSIVE_LEVEL 20

#define SOLICALL_MIN_AEC_HOWLING_LEVEL  0
#define SOLICALL_MAX_AEC_HOWLING_LEVEL 20



typedef struct {
    SHORT sVersion;         // Should be filled with SOLICALL_API_VERSION
    char  *pcSoliCallBin;   // Location of bin directory. If not NULL the current directory is used. 
} sSoliCallPackageInit ;

typedef struct {
    int   iCPUPower;        // A number between SOLICALL_LOWEST_CPU_POWER to SOLICALL_HIGHEST_CPU_POWER. 
                            // The higher the number, the more CPU will be used by the algorithm
    int  iReserved; // Reserved flag - no need to set this parameter.
    SHORT sBitsPerSample;   // Should be either 16 or 8.
    int   iFrequency; // Number between 8000 to 64000 that is a multiple of 8000 (i.e. 8000, 16000, ...). (A.K.A sampling rate).
    SHORT sFrameSize; // multiply of 4ms. For example the value of 5 means frame size of 20ms (= 4 x 5)
                      // the maximum value is 8 - meaning a frame size of 32ms (= 4 x 8).
					  // This value can be between SOLICALL_MIN_FRAME_SIZE and SOLICALL_MAX_FRAME_SIZE 

    SHORT sLookAheadSize; // Number of ms in look ahead buffer. Any number in the range of
                      // 0 (default, no look ahead buffer) - 12 (12 ms in look ahead buffer).

    bool  bNeedToCheckDTMF; // does the package look for DTMF signals ?
    bool  bRemoveNonSelfFrequencies; // need to remove from the output non-self (i.e. noise) frequencies ?
    
    
    // Following values define the behavior when the signal contains only background noise.
    // By default sCNGInitialValue = 15, sCNGDecrease = 5 & sCNGEndValue = 0, as a result
    // the signal is initially reduce to 15%, after half a second the signal is reduced to 10% (= 15-5),
    // and so on, until the signal is reduced to 0% (i.e. total silence).
    SHORT sCNGInitialValue,sCNGDecrease,sCNGEndValue;
    

    // Following values define the behavior when the SDK suspects the end of the burst
    // Since this issue is very delicate and the SDK can not be certain this is really the end
    // (e.g. when there is still more un-voiced portions ahead). You need to be very careful
    // if you decide to change these values.
    // Default values: sBurstEndDecrease=20 - amplitude is decreased in 20% every step
    // sBurstEndNumDecreaseSteps = 1  - number of decrease steps in a second
    // sBurstEndLowerValue = 20 - the final value is 20% of the original amplitude. (please note that the 
    // SDK will make sure this value will not be lower than the sCNGInitialValue).
    SHORT  sBurstEndDecrease,sBurstEndNumDecreaseSteps,sBurstEndLowerValue;
    
    
    // Following value is used to automatically increase the result. In some systems SoliCall
    // might reduce the voice by 5%-10%. If sOutputAMPIncrease=5, SoliCall will
    // automatically increase the output by 5%. By default  sOutputAMPIncrease=0 (i.e.
    // no automatic increase of amplitude).
    // Note, this increase will be done only when SoliCall detects voice and by no chance it will
    // increase the original signal.
    SHORT sOutputAMPIncrease;
    
    SHORT sDelaySize; // multiply of 4ms. Can be between 2 to 50.  For example the value of 6 means 
                      // a delay of 24ms (= 4 x 6). Default value should be 6.
                      
    // Level of aggressivenes in detecting the speaker. This number ranges from
    // SOLICALL_LOWEST_DETECT_AGGRESSIVE_LEVEL to  SOLICALL_HIGHEST_DETECT_AGGRESSIVE_LEVEL
    // The higher the number, the algorithm will be more aggressive in detection.
    SHORT  sDetectAggressiveLevel;

    // Level of aggressivenes in cleaning the noise.  This number ranges from
    // SOLICALL_LOWEST_CLEAN_AGGRESSIVE_LEVEL to  SOLICALL_HIGHEST_CLEAN_AGGRESSIVE_LEVEL
    // The higher the number, the algorithm will be more aggressive in removing the noise.
    SHORT  sCleanAggressiveLevel;


    bool    bCancelAcousticShock;  //Should be set to “true” in case “acoustic shock” might happen in the call. 
            // If “true”, the peaks in the voice of the speaker might be slightly reduced. 

    bool    bDoNotChangeTheOutput;  //Should be set to “false” unless you want the output not to be filtered.

    bool    bBypassVAD;  //Should be set to “false” unless you want to bypass VAD algorithms.

    bool    bActivateAGC;  //Should be set to “true” if you want to activate AGC. Note: AGC is not performed in AEC channels.
    int     iDesiredAGCAmp;  //If AGC is activated, this parameter sets the desired Amp. Usually should be a number around 32,000 (the mid-point).
    int     iMinAGCCoef,iMaxAGCCoef; // Boundaries for the AGC coef. E.g. 30-300.
        
    // The following section is used only when using the API SoliCallMyVoiceInit
    bool  bStartRegistration; 
    char  *pcSpeakerInformation;
    bool  bUseGlogalSpeakerInfo; // If true, then the channel will use the global speaker information. This option saves memory in case of multi-channel environment. 
        // In such case, a one time call to SoliCallInitializeGlobalSpeakerInfo should be made before the channels are initialized.

    // If bStartRegistration == true, we start a registration call in which the voice frames will be used to learn about the 
    // speaker. In this case pcSpeakerInformation should be NULL.
    // If bStartRegistration == false,  then pcSpeakerInformation should point to a storage area that contains 
    // personal informatmion on the speaker.

    // The following parameter is the AEC type (algorithm) to be used.
    // This number ranges from SOLICALL_MIN_AEC_TYPE to  SOLICALL_MAX_AEC_TYPE  
    SHORT   sAECTypeParam;

    // The following parameter is the maximal delay of speaker (in frames) due to asynch input.
	// If more than this amount of speaker frames are missing (compared to the mic) than silent frame is assumed.
    // This number ranges from SOLICALL_MIN_AEC_ASYNC_DELAY to SOLICALL_MAX_AEC_ASYNC_DELAY  
    // Value of zero means that any speaker frame that is missing is a silent frame.  
    SHORT   sMaxAsyncSpeakerDelayAECParam;

     // The following parameter is the maximal delay of microphone (in frames) due to asynch input.
	// If more than this amount of microphone frames are missing (compared to the speaker) than silent frame is assumed.
    // This number ranges from SOLICALL_MIN_AEC_ASYNC_DELAY to SOLICALL_MAX_AEC_ASYNC_DELAY  
    // Value of zero means that any microphone frame that is missing is a silent frame.  
    SHORT   sMaxAsyncMicDelayAECParam;

   // The following parameter is the sensitivity level of the AEC. 
    // This number ranges from SOLICALL_MIN_AEC_SENSITIVITY_LEVEL to SOLICALL_MAX_AEC_SENSITIVITY_LEVEL  
    // The higher the number, the more sensitive the AEC will be.  
    SHORT   sSensitivityLevelAECParam;

    // The following parameters is the aggressive level of the AEC before any convergence is done. 
    // This number ranges from SOLICALL_MIN_AEC_STARTUP_AGGRESSIVE_LEVEL to SOLICALL_MAX_AEC_STARTUP_AGGRESSIVE_LEVEL  
    // The higher the number, the more aggressive the AEC will be at start.  
    SHORT   sAecStartupAggressiveLevel;

    // The following parameters is the aggressive level of the AEC. 
    // This number ranges from SOLICALL_MIN_AEC_AGGRESSIVE_LEVEL to SOLICALL_MAX_AEC_AGGRESSIVE_LEVEL  
    // The higher the number, the more aggressive the AEC will be.  
    SHORT   sAggressiveLevelAECParam;

	// The level needed for howling treatment. When no howling exist - value 0 should be used.
    // The higher the number, the more aggressive the howling removal will be.
	// This number ranges from SOLICALL_MIN_AEC_HOWLING_LEVEL to  SOLICALL_MAX_AEC_HOWLING_LEVEL. 
    SHORT   sAECHowlingLevelTreatment;
    
    // The following parameter is the highest coef that the echo can have. The higher the number the more aggressive the echo
    // cancellation will be. The number range from SOLICALL_LOWEST_MAXCOEFINAECPARAM to SOLICALL_HIGHEST_MAXCOEFINAECPARAM
    SHORT   sMaxCoefInAECParam;
    
    // The following parameter is the lowest coef that the echo can have. The higher the number the more aggressive the echo
    // cancellation will be.  This number should alwasy be lower than sMaxCoefInAECParam.
    // The number range from SOLICALL_LOWEST_MINCOEFINAECPARAM to SOLICALL_HIGHEST_MINCOEFINAECPARAM
    SHORT   sMinCoefInAECParam;

    // the following parameter is the maximal tail used for AEC. 
    // This number ranges from SOLICALL_LOWEST_AEC_TAIL_TYPE to  SOLICALL_HIGHEST_AEC_TAIL_TYPE  
	// Negative value --> delay may be up to -type*50msec. 
	// Zero value --> delay may be up to 400msec
	// Positive value --> delay may be up to 400msec + type*500msec. 
	// For example: A value of 1 means that the delay may be up to 900msec, and value of -5 means that the delay may be up to 250msec..
    SHORT   sAECTailType;

	// the following parameter is the minimal tail used for AEC. 
    // This number ranges from SOLICALL_LOWEST_AEC_MIN_TAIL_TYPE to  SOLICALL_HIGHEST_AEC_MIN_TAIL_TYPE 
	// Zero value --> delay may start from 0msec
	// Negative value --> delay may start from -type*50msec. 
    // For example: A value of -10 means that the delay may not be below 500msec.
    SHORT   sAECMinTailType;
    
	// the following parameter is the minimal Robustness level used for AEC. 
    // This number ranges from SOLICALL_MIN_AEC_ROBUSTNESS_LEVEL to SOLICALL_MAX_AEC_ROBUSTNESS_LEVEL 
    // For example: A value of SOLICALL_MIN_AEC_ROBUSTNESS_LEVEL means that the system try to focus as much as possible. It is suitable for systems with stable narrow echo.
    SHORT   sAECMinRobustnessLevel;

	// the following parameter is the maximal Robustness level used for AEC. 
    // This number ranges from SOLICALL_MIN_AEC_ROBUSTNESS_LEVEL to SOLICALL_MAX_AEC_ROBUSTNESS_LEVEL 
    // For example: A value of SOLICALL_MAX_AEC_ROBUSTNESS_LEVEL means that the system will have maximum robustness when needed.
    SHORT   sAECMaxRobustnessLevel;

	// the following parameter is the stability level of the environment. 
    // This number ranges from SOLICALL_MIN_AEC_STABILITY_LEVEL to SOLICALL_MAX_AEC_STABILITY_LEVEL 
    // For example: A value of SOLICALL_MAX_AEC_STABILITY_LEVEL means that the system is very stable and there are almost no packet losses or EP changes.
    SHORT   sAECStabilityLevel;

	// the following parameter is the advanced aggressive level. It is rellevant only if sAECTypeParam=8. 
    // This number ranges from SOLICALL_MIN_AEC_SPECIAL_AGGRESSIVE_LEVEL to SOLICALL_MAX_AEC_SPECIAL_AGGRESSIVE_LEVEL 
    // The higher the number, the more aggressive the AEC will be.  
    SHORT   sAECAdvancedAggressiveLevel;

	// the following parameter is the environment type. 
    // This number ranges from SOLICALL_MIN_AEC_ENVIRONEMT_TYPE to SOLICALL_MAX_AEC_ENVIRONEMT_TYPE 
    SHORT   sAECEnvironmentType;

	// the following parameter indicates on the length of the AEC burst. AEC burst is used to overcome small interruption
    // during double talk.  Zero means no bursts will be allowed. We recommend starting with the value 2000 (i.e. 250ms). 
    // In addition, from v50_58_72 (old1214) this parameter also supports echo burst - meaning a time after echo is detected that the system will output silence.
    // This is important  for systems with significantly long reverberation. In order to support the echo burst the parameter used is iNumberOfSamplesInAECBurst 
    // consists of 2 parts:
    // a. EchoBurst as the high short value  
    // b. SpeechBurst as the low short value
    // e.g. for using AECBurst of 2000 samples and EchoBurst of 8000 samples you need to set the value of this parameter to 2000 + (8000<<16) 
    int iNumberOfSamplesInAECBurst;

	// the following parameter indicates on the length of the high confidence AEC burst. High confidence AEC burst is used to overcome small interruption
    // during double talk.  Zero means no bursts will be allowed.
    int iNumberOfSamplesInHighConfidenceAECBurst;
    
    // The lowest percentage to be output when an echo is detected. 
    SHORT   sAECMinOutputPercentageDuringEcho;
  
	// the following parameter is the percent of comfort noise to be added to the output for AEC channel.
    // It is recommended to start with the value of 100 especially if you run PNR after the AEC.
    SHORT   sComfortNoisePercent;

} sSoliCallInit ;


typedef struct {
    char    *pcPnrDb;       // Pointer to PnrDb
    size_t  sPnrDbSize;     // Size of PnrDb
    SHORT   sAggressiveLevel;
    SHORT   sLowPowerDecreaseLevel; // 0-None, 1 low aggressive .. 4+ high aggressive - default is 1
    SHORT   sLearnCharacteristics;    //0-do not learn, 1-learn Only speaker characteristics, 2- Learn Only noise characteristics, 3 - Learn all 
    SHORT   sParam5;
    SHORT   sParam6;
    SHORT   sParam7;

} sSoliCallPnrInit;

#ifdef __cplusplus
extern "C" {
#endif
    
// Get Solicall version (32 bit unsigned int). 
// SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallVersion(/* OUT*/ unsigned int *puVersion);

// Initialize global data structures and parametes for all the SDK.
// This function must be called only one time and it should be before calling any other API in this SDK.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallPackageInit(/* IN */ sSoliCallPackageInit *psSoliCallPackageInit); 
    

// Initialize data structures and parametes.
// This function must be called in the beginning of each voice call.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallInit(/* IN */ SHORT sChannel, /* IN */ sSoliCallInit *psSoliCallInit); 


// Process one frame. pbOutput should point to an already allocated memory.
// If both pbOutput and pbInput are the same - the process will be done "in place".
// Every call you should give a full frame and SoliCall will return a clean frame.
// There is one exceptions:
// 1. At the beginning of the conversation, SoliCall will not return a full frame until it will fill its itnernal buffers.
//
// If piVAD is set to 1, then the SDK decided that the output data contain voice. Otherwise, piVAD is set to 0. You could use this indication
// to generate your own Comfort Noise whenever piVAD equals 0.
// if piConfidentVAD is set to 1, then the SDK has good confidence the data contains voice.
// If piDTMF is set to 1, then the SDK decided that the output data contain DTMF. Otherwise, piDTMF is set to 0.  In order for the SDK
// to detect DTMF, you need to set the the parameter bNeedToCheckDTMF when you call the function SoliCallPackageInit.
// If bRemoveNonSelfFrequencies was set to true, the piLastNoiseAmplitude will contain the last known noise amplitude.
// piEstimatedVoiceAmplitude contains the estimated amplitude of voice.
// piCalculatedAGCCoef contians the calculated AGC Coef.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallProcessFrame(/* IN */ SHORT sChannel, /* IN */ BYTE *pbInput,/* IN */ int iNumInBytes, 
    /* OUT */ BYTE *pbOutput, /* OUT */ int *piNumOutBytes, /* OUT */int *piVAD, /* OUT */ int *piConfidentVAD,
    /* OUT */ int *piDTMF, /* OUT */ int *piLastNoiseAmplitude, /* OUT */ int *piEstimatedVoiceAmplitude, /* OUT */ int *piCalculatedAGCCoef);


// Frees allocated memory - if the dynamic memory is used. 
// If no dynamnic allocations are used there is no need to call this function.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallTerminate(/* IN */ SHORT sChannel);


// Initialize data structures and parametes.
// This function must be called in the beginning of each voice call to activate the new PNR.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallMyPnrVoiceInit(/* IN */ SHORT sChannel, /* IN */ sSoliCallInit *psSoliCallInit, /* IN */ sSoliCallPnrInit *psSoliCallPnrInit); 
        

// Initialize data structures and parametes.
// This function must be called in the beginning of each voice call.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallMyVoiceInit(/* IN */ SHORT sChannel, /* IN */ sSoliCallInit *psSoliCallInit); 

// Completes Registration. Returns the number of bytes that are requiredto store the speaker information.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallCompleteRegistration(/* IN */ SHORT sChannel, /* OUT */ size_t  *psSizeOfSpeakerInformation);

// Fills the psSpeakerInformation with personal information on the speaker.
// psSpeakerInformation should point to an already allocated storage area with size that was provided in the call to SoliCallCompleteRegistration
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallCopySpeakerInformationAfterRegistration(/* IN */ SHORT sChannel, /* OUT */ char  *pcSpeakerInformation);



// Fills the pcDTMFBuffer with the last detected DTMF buffer (i.e. the last detected DTMF burst).
// pcDTMFBuffer should point to an already allocated string array.  The size of pcDTMFBuffer
// is indicated by iSizeOfDTMFBuffer.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallGetLastDTMFBuffer(/* IN */ SHORT sChannel, /* OUT */ char  *pcDTMFBuffer,/* IN */ int iSizeOfDTMFBuffer);


// Initialize data structures and parametes.
// This function must be called in the beginning of each voice call.
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallAECInit(/* IN */ SHORT sAECChannel, /* IN */ sSoliCallInit *psSoliCallInit); 


// Process one frame from the Mic. pbOutput should point to an already allocated memory.
// If both pbOutput and pbInput are the same - the process will be done "in place".
// Every call you should give a full frame and SoliCall will return a clean frame (except for the first calls in which SoliCall fills its internal buffers).
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallAECProcessMicFrame(/* IN */ SHORT sAECChannel, /* IN */ BYTE *pbInput,/* IN */ int iNumInBytes, 
    /* OUT */ BYTE *pbOutput, /* OUT */ int *piNumOutBytes, /* OUT */  int *piCurrEchoAmplitude);

// Process one frame from the Spk. 
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallAECProcessSpkFrame(/* IN */ SHORT sAECChannel, /* IN */ BYTE *pbInput,/* IN */ int iNumInBytes);

// Frees allocated memory - if the dynamic memory is used. 
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallAECTerminate(/* IN */ SHORT sAECChannel);

// Calls SoliCallAECProcessMicFrame and then SoliCallProcessFrame while enabling information sharing between the AEC channel and Noise Reduction channel
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallComboAECNRProcessFrame(SHORT sChannel,BYTE *pbInput,int iNumInBytes, BYTE *pbOutput, int *piNumOutBytes,
    int *piVAD,int *piConfidentVAD,int *piDTMF,int *piLastNoiseAmplitude,int *piEstimatedVoiceAmplitude, int *piCalculatedAGCCoef,int *piCurrEchoAmplitude);

// Calls SoliCallProcessFrame and then SoliCallAECProcessMicFrame  while enabling information sharing between the AEC channel and Noise Reduction channel
// In case a success, SOLICALL_RC_SUCCESS is returned.
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallComboNRAECProcessFrame(SHORT sChannel,BYTE *pbInput,int iNumInBytes, BYTE *pbOutput, int *piNumOutBytes,
    int *piVAD,int *piConfidentVAD,int *piDTMF,int *piLastNoiseAmplitude,int *piEstimatedVoiceAmplitude, int *piCalculatedAGCCoef,int *piCurrEchoAmplitude);

// Stores the profile in global storage that can be shared between all channels. 
// In case a success, SOLICALL_RC_SUCCESS is returned
SOLICALL_RC_DLL  SOLICALLSDK_STDCALL SoliCallInitializeGlobalSpeakerInfo(/* IN */ char *pcSpeakerInformation);


#ifdef __cplusplus
}
#endif

#endif
