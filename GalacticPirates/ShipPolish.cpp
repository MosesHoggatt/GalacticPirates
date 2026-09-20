#include "ShipPolish.h"
#include "ShipCameraShakes.h"
#include "GalacticPirates.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/UnrealMemory.h"

struct FPolishClip
{
	TArray<uint8> PCM;
	int32 SampleRate = 44100;
	int32 NumChannels = 1;
	float Duration = 0.1f;
};

static TMap<FName, FPolishClip> GPolishClips;

static bool GPParseWavFile(const FString& FilePath, FPolishClip& OutClip)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath) || FileData.Num() < 44)
	{
		return false;
	}

	const uint8* Data = FileData.GetData();
	const int32 Size = FileData.Num();
	if (FMemory::Memcmp(Data, "RIFF", 4) != 0 || FMemory::Memcmp(Data + 8, "WAVE", 4) != 0)
	{
		return false;
	}

	int32 Offset = 12;
	int32 Channels = 1;
	int32 SampleRate = 44100;
	int32 BitsPerSample = 16;
	const uint8* PCMPtr = nullptr;
	int32 PCMSize = 0;

	while (Offset + 8 <= Size)
	{
		const char* ChunkId = reinterpret_cast<const char*>(Data + Offset);
		const int32 ChunkSize = *reinterpret_cast<const int32*>(Data + Offset + 4);
		Offset += 8;
		if (Offset + ChunkSize > Size)
		{
			break;
		}

		if (FMemory::Memcmp(ChunkId, "fmt ", 4) == 0 && ChunkSize >= 16)
		{
			Channels = *reinterpret_cast<const int16*>(Data + Offset + 2);
			SampleRate = *reinterpret_cast<const int32*>(Data + Offset + 4);
			BitsPerSample = *reinterpret_cast<const int16*>(Data + Offset + 14);
		}
		else if (FMemory::Memcmp(ChunkId, "data", 4) == 0)
		{
			PCMPtr = Data + Offset;
			PCMSize = ChunkSize;
			break;
		}

		Offset += ChunkSize + (ChunkSize & 1);
	}

	if (!PCMPtr || PCMSize <= 0 || BitsPerSample != 16 || Channels <= 0 || SampleRate <= 0)
	{
		return false;
	}

	OutClip.PCM.SetNumUninitialized(PCMSize);
	FMemory::Memcpy(OutClip.PCM.GetData(), PCMPtr, PCMSize);
	OutClip.SampleRate = SampleRate;
	OutClip.NumChannels = Channels;
	const float BytesPerSecond = static_cast<float>(SampleRate * Channels * (BitsPerSample / 8));
	OutClip.Duration = BytesPerSecond > 0.0f ? static_cast<float>(PCMSize) / BytesPerSecond : 0.1f;
	return true;
}

static const FPolishClip* GPGetPolishClip(const TCHAR* AssetName)
{
	if (!AssetName)
	{
		return nullptr;
	}

	const FName Key(AssetName);
	if (const FPolishClip* Existing = GPolishClips.Find(Key))
	{
		return Existing;
	}

	const FString DiskPath = FPaths::ProjectContentDir() / TEXT("Polish/Audio") / FString(AssetName) + TEXT(".wav");
	FPolishClip Clip;
	if (!GPParseWavFile(DiskPath, Clip))
	{
		UE_LOG(LogGalacticPirates, Verbose, TEXT("[Polish] Missing wav %s"), *DiskPath);
		return nullptr;
	}

	return &GPolishClips.Add(Key, MoveTemp(Clip));
}

static USoundWaveProcedural* GPMakeProceduralWave(const FPolishClip& Clip)
{
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>();
	Wave->SetSampleRate(Clip.SampleRate);
	Wave->NumChannels = Clip.NumChannels;
	Wave->Duration = Clip.Duration;
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->bLooping = false;
	Wave->QueueAudio(Clip.PCM.GetData(), Clip.PCM.Num());
	return Wave;
}

USoundBase* GPLoadPolishSound(const TCHAR* AssetName)
{
	if (!AssetName)
	{
		return nullptr;
	}

	const FString Path = FString::Printf(TEXT("/Game/Polish/Audio/%s.%s"), AssetName, AssetName);
	if (USoundBase* Sound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)))
	{
		return Sound;
	}

	if (const FPolishClip* Clip = GPGetPolishClip(AssetName))
	{
		return GPMakeProceduralWave(*Clip);
	}

	return nullptr;
}

UTexture2D* GPLoadPolishTexture(const TCHAR* AssetName)
{
	if (!AssetName)
	{
		return nullptr;
	}

	static TMap<FName, TObjectPtr<UTexture2D>> Cache;
	const FName Key(AssetName);
	if (TObjectPtr<UTexture2D>* Existing = Cache.Find(Key))
	{
		if (Existing->Get())
		{
			return Existing->Get();
		}
	}

	const TCHAR* Folders[] = { TEXT("Textures"), TEXT("VFX") };
	UTexture2D* Texture = nullptr;
	for (const TCHAR* Folder : Folders)
	{
		const FString ObjectPath = FString::Printf(TEXT("/Game/Polish/%s/%s.%s"), Folder, AssetName, AssetName);
		Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
		if (Texture)
		{
			break;
		}

		const FString DiskPath = FPaths::ProjectContentDir() / TEXT("Polish") / Folder / FString(AssetName) + TEXT(".png");
		if (FPaths::FileExists(DiskPath))
		{
			Texture = FImageUtils::ImportFileAsTexture2D(DiskPath);
			if (Texture)
			{
				Texture->AddToRoot();
				Texture->Filter = TF_Bilinear;
				Texture->SRGB = true;
				Texture->AddressX = TA_Wrap;
				Texture->AddressY = TA_Wrap;
				Texture->UpdateResource();
				break;
			}
		}
	}

	if (Texture)
	{
		Cache.Add(Key, Texture);
	}
	return Texture;
}

UMaterialInstanceDynamic* GPApplyPolishVfxMaterial(UPrimitiveComponent* Mesh, const TCHAR* TextureName, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return nullptr;
	}

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
	}
	if (!Base)
	{
		Base = Mesh->GetMaterial(0);
	}
	if (!Base)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!MID || MID->GetBaseMaterial() != Base->GetBaseMaterial())
	{
		MID = UMaterialInstanceDynamic::Create(Base, Mesh);
		Mesh->SetMaterial(0, MID);
	}

	if (UTexture2D* Texture = GPLoadPolishTexture(TextureName))
	{
		MID->SetTextureParameterValue(TEXT("Texture"), Texture);
	}

	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
	MID->SetVectorParameterValue(TEXT("TintColorAndOpacity"), Color);
	return MID;
}

void GPPlayPolishSound2D(const UObject* WorldContext, const TCHAR* AssetName, float Volume)
{
	if (USoundBase* Sound = GPLoadPolishSound(AssetName))
	{
		UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume);
	}
}

void GPPlayPolishSoundAt(const UObject* WorldContext, const TCHAR* AssetName, const FVector& Location, float Volume)
{
	if (USoundBase* Sound = GPLoadPolishSound(AssetName))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, Volume);
	}
}

void GPAttachStationLabel(USceneComponent* Parent, const FText& Label, const FColor& Color)
{
	if (!Parent || !Parent->GetOwner())
	{
		return;
	}

	UTextRenderComponent* Text = NewObject<UTextRenderComponent>(Parent->GetOwner());
	if (!Text)
	{
		return;
	}

	Text->SetupAttachment(Parent);
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 72.0f));
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextBottom);
	Text->SetWorldSize(22.0f);
	Text->SetTextRenderColor(Color);
	Text->SetText(Label);
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Text->RegisterComponent();
}

void GPPlayCannonCameraShake(UWorld* World, const FVector& Epicenter, float InnerRadius, float OuterRadius, float Scale)
{
	if (!World)
	{
		return;
	}

	UGameplayStatics::PlayWorldCameraShake(World, UShipCannonCameraShake::StaticClass(), Epicenter, InnerRadius, OuterRadius, Scale, false);
}

void GPPlayExplosionCameraShake(UWorld* World, const FVector& Epicenter, float InnerRadius, float OuterRadius, float Scale)
{
	if (!World)
	{
		return;
	}

	UGameplayStatics::PlayWorldCameraShake(World, UShipExplosionCameraShake::StaticClass(), Epicenter, InnerRadius, OuterRadius, Scale, false);
}
