#pragma once

#include "utils/common.h"
#include "solical/solicall_api.h"
#include <fstream>
#include <string>
#include <iostream>
#include <memory>
#include <fstream>

#define SOLICALL_RC_RESTARTED 4

namespace aec {

    //todo
    constexpr auto FREQUENCY = DeviceOptions::Freq;
    constexpr auto FRAME_MULTIPLIER = DeviceOptions::FrameMultiplayer;
    constexpr auto CHANNEL_ID = 1;
    constexpr const char *DBGDIR = "/storage/emulated/0/Download/";

    auto fillPackageInit() {
        sSoliCallPackageInit x{};
        x.sVersion = 6;
        x.pcSoliCallBin = const_cast<char *>(DBGDIR);
        return x;
    }

    auto fillAecInit() {
        sSoliCallInit x{};
        x.iCPUPower = 2;
        x.sBitsPerSample = 16;
        x.iFrequency = FREQUENCY;
        x.sFrameSize = FRAME_MULTIPLIER;
        x.sLookAheadSize = 0;
        x.bDoNotChangeTheOutput = false;
        x.sAECTypeParam = 8;
        x.sDelaySize = 9;
        x.sMaxAsyncSpeakerDelayAECParam = 5;
        x.sMaxAsyncMicDelayAECParam = 5;
        x.sSensitivityLevelAECParam = 6;
        x.sAggressiveLevelAECParam = 10;
        x.sAECHowlingLevelTreatment = 10;
        x.sMaxCoefInAECParam = 100;
        x.sMinCoefInAECParam = 1;
        x.sAECTailType = 0;
        x.sAECMinTailType = 0;

        x.sAECMinRobustnessLevel = 0;
        x.sAECMaxRobustnessLevel = 15;
        x.sAECStabilityLevel = 10;
        x.sAECAdvancedAggressiveLevel = 10;
        x.sAECEnvironmentType = 10;

        x.iNumberOfSamplesInAECBurst = 2000;
        x.iNumberOfSamplesInHighConfidenceAECBurst = 2000;
        x.sAECMinOutputPercentageDuringEcho = 100;
        x.sAecStartupAggressiveLevel = 10; // startup heuristic
        x.sComfortNoisePercent = 100;  //comfort noise
        return x;
    }

    auto fillNrInit() {
        sSoliCallInit x = fillAecInit();
        x.bNeedToCheckDTMF = false;
        x.bRemoveNonSelfFrequencies = false;
        x.sCNGInitialValue = 15;
        x.sCNGDecrease = 5;
        x.sCNGEndValue = 60;
        x.sBurstEndDecrease = 40;
        x.sBurstEndNumDecreaseSteps = 1;
        x.sBurstEndLowerValue = 100;
        x.sOutputAMPIncrease = 5;
        x.sDetectAggressiveLevel = 2;
        x.sCleanAggressiveLevel = 4;
        x.bCancelAcousticShock = false;
        x.bBypassVAD = false;
        x.bActivateAGC = false; // or true to turn ON AGC
        x.iDesiredAGCAmp = 32000;
        x.iMinAGCCoef = 50;
        x.iMaxAGCCoef = 500;
        return x;
    }

    int init() {
        using namespace std::literals;
        static auto packageInit = fillPackageInit();
        static auto aecInit = fillAecInit();
        static auto nrInit = fillNrInit();

        static /* must once */ auto resPackage = SoliCallPackageInit(&packageInit);
        if (resPackage != SOLICALL_RC_SUCCESS) {
            myLog<Prio::E>("SOLICALL INIT FAILED: %i", resPackage);
            return resPackage;
        }

        auto resAec = SoliCallAECInit(CHANNEL_ID, &aecInit);
        if (resAec != SOLICALL_RC_SUCCESS) {
            myLog<Prio::E>("SOLICALL AEC INIT FAILED: %i", resAec);
            return resAec;
        }

        auto resNr = SoliCallInit(CHANNEL_ID, &nrInit);
        if (resNr != SOLICALL_RC_SUCCESS) {
            myLog<Prio::E>("SOLICALL NR INIT FAILED: %i", resNr);
            return resNr;
        }

        myLog("SoliCall activated");
        return SOLICALL_RC_SUCCESS;
    }

    int destroy() {
        if (SoliCallAECTerminate(CHANNEL_ID) != SOLICALL_RC_SUCCESS) {
            myLog<Prio::E>("error in terminate AEC");
            return SOLICALL_RC_ERROR;
        }
        if (SoliCallTerminate(CHANNEL_ID) != SOLICALL_RC_SUCCESS) {
            myLog<Prio::E>("error in terminate");
            return SOLICALL_RC_ERROR;
        }
//        std::ofstream outfile("/storage/emulated/0/Download/TESTSETSET.txt");
//        outfile << 123 << std::endl;
//        outfile.close();

        myLog("SoliCall terminated");
        return SOLICALL_RC_SUCCESS;
    }

    int restart() {
        if (aec::destroy() != SOLICALL_RC_SUCCESS) return SOLICALL_RC_ERROR;
        if (aec::init() != SOLICALL_RC_SUCCESS) return SOLICALL_RC_ERROR;
        return SOLICALL_RC_RESTARTED;
    }

    bool fakeError() {
        if constexpr(_FAKE_RESTART) {
            static uint16_t counter{};
            return !(++counter % (0x4ff));
        } else { return false; }
    }

    int processSpeaker(int count, BYTE *speaker) {
//        myLog<Prio::I>("processSpeaker");
        if (fakeError() ||
            SoliCallAECProcessSpkFrame(CHANNEL_ID, speaker, count) != SOLICALL_RC_SUCCESS) {
            myLog<Prio::E>(
                    "Error in process speaker frame. Did you pass the call length limit? Restarting...");
            return SOLICALL_RC_RESTARTED;
        }
        return SOLICALL_RC_SUCCESS;
    }

    int processMicro(int count, BYTE *micro, BYTE *out, int &out_count) {
//        myLog<Prio::I>("processMicro");
        static int iTmpCurrEchoAmplitude, iVAD, iConfidentVAD, iDTMF, iLastNoiseAmplitude, iEstimatedVoiceAmplitude, iCalculatedAGCCoef;

        if (fakeError() ||
            SoliCallComboAECNRProcessFrame(CHANNEL_ID,
                                           micro,
                                           count,
                                           out,
                                           &out_count,
                                           &iVAD,
                                           &iConfidentVAD,
                                           &iDTMF,
                                           &iLastNoiseAmplitude,
                                           &iEstimatedVoiceAmplitude,
                                           &iCalculatedAGCCoef,
                                           &iTmpCurrEchoAmplitude
            ) != SOLICALL_RC_SUCCESS) {
            myLog<Prio::E>(
                    "Error in process micro frame. Did you pass the call length limit? Restarting...");
            return SOLICALL_RC_RESTARTED;
        }
//        myLog<Prio::I>(
//                "out_count: %i\tiVAD: %i\tiConfidentVAD: %i\tiDTMF: %i\t"
//                "iLastNoiseAmplitude: %i\tiEstimatedVoiceAmplitude: %i\tiCalculatedAGCCoef: %i\tiTmpCurrEchoAmplitude: %i",
//                out_count, iVAD, iConfidentVAD, iDTMF, iLastNoiseAmplitude,
//                iEstimatedVoiceAmplitude, iCalculatedAGCCoef, iTmpCurrEchoAmplitude
//        );
        return SOLICALL_RC_SUCCESS;
    }


}

