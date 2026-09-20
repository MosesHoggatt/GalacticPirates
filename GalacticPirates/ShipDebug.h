#pragma once

#include "CoreMinimal.h"

class AGalacticPiratesCharacter;
class UWorld;

int32 GPShipDebugLevel();
bool GPShipPlaytestEnabled();
bool GPInteriorTestEnabled();
void GPShipDebugEvent(const TCHAR* Message);
void GPShipDebugSnapshot(const AGalacticPiratesCharacter* Character, const TCHAR* Reason);
void GPShipDebugScreen(const FString& Message, const FColor& Color, float Duration = 6.0f);
void GPStartInteriorWalkTest(AGalacticPiratesCharacter* Character);
void GPTickInteriorWalkTest(AGalacticPiratesCharacter* Character, float DeltaTime);
bool GPIsInteriorWalkTestRunning();
void GPStartExtremeFlightTest(AGalacticPiratesCharacter* Character);
void GPTickExtremeFlightTest(AGalacticPiratesCharacter* Character, float DeltaTime);
bool GPIsExtremeFlightTestRunning();
void GPStartHelmSteerTest(AGalacticPiratesCharacter* Character);
void GPTickHelmSteerTest(AGalacticPiratesCharacter* Character, float DeltaTime);
void GPStartHelmJitterTest(AGalacticPiratesCharacter* Character);
void GPTickHelmJitterTest(AGalacticPiratesCharacter* Character, float DeltaTime);
void GPSampleHelmJitter(AGalacticPiratesCharacter* Character);
void GPStartShipJumpTest(AGalacticPiratesCharacter* Character);
void GPTickShipJumpTest(AGalacticPiratesCharacter* Character, float DeltaTime);
void GPStartDedicatedNetTest(UWorld* World);
void GPTickDedicatedNetTest(UWorld* World, float DeltaTime);
bool GPDedicatedNetTestEnabled();
void GPStartShipDuelTest(UWorld* World);
void GPTickShipDuelTest(UWorld* World, float DeltaTime);
int32 GPShipMoveLogLevel();
void GPStartShipMoveProbe(UWorld* World);
void GPTickShipMoveProbe(UWorld* World, float DeltaTime);
