#include "ShipCameraShakes.h"

UShipCannonCameraShake::UShipCannonCameraShake()
{
	OscillationDuration = 0.28f;
	OscillationBlendInTime = 0.02f;
	OscillationBlendOutTime = 0.18f;
	RotOscillation.Pitch.Amplitude = 3.5f;
	RotOscillation.Pitch.Frequency = 22.0f;
	RotOscillation.Yaw.Amplitude = 1.8f;
	RotOscillation.Yaw.Frequency = 18.0f;
	LocOscillation.X.Amplitude = 4.0f;
	LocOscillation.X.Frequency = 16.0f;
}

UShipExplosionCameraShake::UShipExplosionCameraShake()
{
	OscillationDuration = 0.7f;
	OscillationBlendInTime = 0.04f;
	OscillationBlendOutTime = 0.35f;
	RotOscillation.Pitch.Amplitude = 8.0f;
	RotOscillation.Pitch.Frequency = 14.0f;
	RotOscillation.Yaw.Amplitude = 5.0f;
	RotOscillation.Yaw.Frequency = 11.0f;
	LocOscillation.Z.Amplitude = 12.0f;
	LocOscillation.Z.Frequency = 9.0f;
}
