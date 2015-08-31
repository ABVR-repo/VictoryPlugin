/*
	By Rama
*/
#include "VictoryBPLibraryPrivatePCH.h"

#include "StaticMeshResources.h"

#include "Developer/ImageWrapper/Public/Interfaces/IImageWrapper.h"
#include "Developer/ImageWrapper/Public/Interfaces/IImageWrapperModule.h"

//Body Setup
#include "PhysicsEngine/BodySetup.h"


//~~~ PhysX ~~~
#include "PhysXIncludes.h"
#include "PhysXPublic.h"		//For the ptou conversions
//~~~~~~~~~~~
 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//									Saxon Rah Random Nodes
// Chrono and Random

//Order Matters, 
//		has to be after PhysX includes to avoid isfinite name definition issues
#include <chrono>
#include <random>
 



/*
	~~~ Rama File Operations CopyRight ~~~ 
	
	If you use any of my file operation code below, 
	please credit me somewhere appropriate as "Rama"
*/
template <class FunctorType>
class PlatformFileFunctor : public IPlatformFile::FDirectoryVisitor	//GenericPlatformFile.h
{
public:
	
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		return Functor(FilenameOrDirectory, bIsDirectory);
	}

	PlatformFileFunctor(FunctorType&& FunctorInstance)
		: Functor(MoveTemp(FunctorInstance))
	{
	}

private:
	FunctorType Functor;
};

template <class Functor>
PlatformFileFunctor<Functor> MakeDirectoryVisitor(Functor&& FunctorInstance)
{
	return PlatformFileFunctor<Functor>(MoveTemp(FunctorInstance));
}

static FDateTime GetFileTimeStamp(const FString& File)
{
	return FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*File);
}
static void SetTimeStamp(const FString& File, const FDateTime& TimeStamp)
{
	FPlatformFileManager::Get().GetPlatformFile().SetTimeStamp(*File,TimeStamp);
}	
	
//Radial Result Struct
struct FFileStampSort
{
	FString* FileName;
	FDateTime FileStamp;
	
	FFileStampSort(FString* IN_Name, FDateTime Stamp)
		: FileName(IN_Name)
		, FileStamp(Stamp)
	{}
};

//For Array::Sort()
FORCEINLINE bool operator< (const FFileStampSort& Left, const FFileStampSort& Right)
{
	return Left.FileStamp < Right.FileStamp; 
}

//Written by Rama, please credit me if you use this code elsewhere
static bool GetFiles(const FString& FullPathOfBaseDir, TArray<FString>& FilenamesOut, bool Recursive=false, const FString& FilterByExtension = "")
{
	//Format File Extension, remove the "." if present
	const FString FileExt = FilterByExtension.Replace(TEXT("."),TEXT("")).ToLower();
	
	FString Str;
	auto FilenamesVisitor = MakeDirectoryVisitor(
		[&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) 
		{
			//Files
			if ( ! bIsDirectory)
			{
				//Filter by Extension
				if(FileExt != "")
				{
					Str = FPaths::GetCleanFilename(FilenameOrDirectory);
				
					//Filter by Extension
					if(FPaths::GetExtension(Str).ToLower() == FileExt) 
					{
						if(Recursive) 
						{
							FilenamesOut.Push(FilenameOrDirectory); //need whole path for recursive
						}
						else 
						{
							FilenamesOut.Push(Str);
						}
					}
				}
				
				//Include All Filenames!
				else
				{
					//Just the Directory
					Str = FPaths::GetCleanFilename(FilenameOrDirectory);
					
					if(Recursive) 
					{
						FilenamesOut.Push(FilenameOrDirectory); //need whole path for recursive
					}
					else 
					{
						FilenamesOut.Push(Str);
					}
				}
			}
			return true;
		}
	);
	if(Recursive) 
	{
		return FPlatformFileManager::Get().GetPlatformFile().IterateDirectoryRecursively(*FullPathOfBaseDir, FilenamesVisitor);
	}
	else 
	{
		return FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*FullPathOfBaseDir, FilenamesVisitor);
	}
}	

static bool FileExists(const FString& File)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*File);
}
static bool FolderExists(const FString& Dir)
{
	return FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Dir);
}
static bool RenameFile(const FString& Dest, const FString& Source)
{
	//Make sure file modification time gets updated!
	SetTimeStamp(Source,FDateTime::Now());
	
	return FPlatformFileManager::Get().GetPlatformFile().MoveFile(*Dest,*Source);
}
	
//Create Directory, Creating Entire Structure as Necessary
//		so if JoyLevels and Folder1 do not exist in JoyLevels/Folder1/Folder2
//			they will be created so that Folder2 can be created!

//This is my solution for fact that trying to create a directory fails 
//		if its super directories do not exist
static bool VCreateDirectory(FString FolderToMake) //not const so split can be used, and does not damage input
{
	if(FolderExists(FolderToMake))
	{
		return true;
	}
	
	// Normalize all / and \ to TEXT("/") and remove any trailing TEXT("/") if the character before that is not a TEXT("/") or a colon
	FPaths::NormalizeDirectoryName(FolderToMake);
	
	//Normalize removes the last "/", but is needed by algorithm
	//  Guarantees While loop will end in a timely fashion.
	FolderToMake += "/";
	
	FString Base;
	FString Left;
	FString Remaining;
	
	//Split off the Root
	FolderToMake.Split(TEXT("/"),&Base,&Remaining);
	Base += "/"; //add root text and Split Text to Base
	
	while(Remaining != "")
	{
		Remaining.Split(TEXT("/"),&Left,&Remaining);
		
		//Add to the Base
		Base += Left + FString("/"); //add left and split text to Base
		
		//Create Incremental Directory Structure!
		if ( ! FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*Base) && 
			! FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Base) )
		{
			return false;
			//~~~~~
		}
	}
	
	return true;
}
/*
	~~~ Rama File Operations CopyRight ~~~ 
	
	If you use any of my file operation code above, 
	please credit me somewhere appropriate as "Rama"
*/



//////////////////////////////////////////////////////////////////////////
// UVictoryBPFunctionLibrary

UVictoryBPFunctionLibrary::UVictoryBPFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}
 
//~~~~~~~
//		AI
//~~~~~~~ 
EPathFollowingRequestResult::Type UVictoryBPFunctionLibrary::Victory_AI_MoveToWithFilter(
	APawn* Pawn, 
	const FVector& Dest, 
	TSubclassOf<UNavigationQueryFilter> FilterClass ,
	float AcceptanceRadius , 
	bool bProjectDestinationToNavigation ,
	bool bStopOnOverlap ,
	bool bCanStrafe 
){
	if(!Pawn) 
	{
		return EPathFollowingRequestResult::Failed;
	}
	
	AAIController* AIControl = Cast<AAIController>(Pawn->GetController());
	if(!AIControl) 
	{
		return EPathFollowingRequestResult::Failed;
	} 
	
	return AIControl->MoveToLocation(
		Dest, 
		AcceptanceRadius,
		bStopOnOverlap, 	//bStopOnOverlap
		true,						//bUsePathfinding 
		bProjectDestinationToNavigation, 
		bCanStrafe,			//bCanStrafe
		FilterClass			//<~~~
	);
}
	
//~~~~~~
//Physics
//~~~~~~ 
float UVictoryBPFunctionLibrary::GetDistanceToCollision(UPrimitiveComponent* CollisionComponent, const FVector& Point, FVector& ClosestPointOnCollision)
{
	if(!CollisionComponent) return -1;
	 
	return CollisionComponent->GetDistanceToCollision(Point,ClosestPointOnCollision);
}

float UVictoryBPFunctionLibrary::GetDistanceBetweenComponentSurfaces(UPrimitiveComponent* CollisionComponent1, UPrimitiveComponent* CollisionComponent2, FVector& PointOnSurface1, FVector& PointOnSurface2)
{
	if(!CollisionComponent1 || !CollisionComponent2) return -1;
 
	//Closest Point on 2 to 1
	CollisionComponent2->GetDistanceToCollision(CollisionComponent1->GetComponentLocation(), PointOnSurface2);
  
	//Closest Point on 1 to closest point on surface of 2
	return CollisionComponent1->GetDistanceToCollision(PointOnSurface2, PointOnSurface1);
}

bool UVictoryBPFunctionLibrary::VictoryPhysics_UpdateAngularDamping(UPrimitiveComponent* CompToUpdate, float NewAngularDamping)
{
	if(!CompToUpdate) return false;
	
	FBodyInstance* Body = CompToUpdate->GetBodyInstance();
	if(!Body) return false;
	
	//Deep safety check
	if(!Body->IsValidBodyInstance()) return false;
	
	//Set!
	Body->AngularDamping = NewAngularDamping;
	
	//~~~~~~~~~~~~~~~~~
	Body->UpdateDampingProperties();
	//~~~~~~~~~~~~~~~~~
	
	return true;
}
	 
static int32 GetChildBones(const FReferenceSkeleton& ReferenceSkeleton, int32 ParentBoneIndex, TArray<int32> & Children)
{ 
	Children.Empty();

	const int32 NumBones = ReferenceSkeleton.GetNum();
	for(int32 ChildIndex=ParentBoneIndex+1; ChildIndex<NumBones; ChildIndex++)
	{
		if ( ParentBoneIndex == ReferenceSkeleton.GetParentIndex(ChildIndex) )
		{
			Children.Add(ChildIndex);
		}
	}

	return Children.Num();
}
static void GetChildBoneNames_Recursive(USkeletalMeshComponent* SkeletalMeshComp, int32 ParentBoneIndex, TArray<FName>& ChildBoneNames)
{	
	TArray<int32> BoneIndicies;  
	GetChildBones(SkeletalMeshComp->SkeletalMesh->RefSkeleton, ParentBoneIndex, BoneIndicies);
	   
	if(BoneIndicies.Num() < 1)
	{
		//Stops the recursive skeleton search
		return;
	}
	 
	for(const int32& BoneIndex : BoneIndicies)
	{
		FName ChildBoneName = SkeletalMeshComp->GetBoneName(BoneIndex);
		ChildBoneNames.Add(ChildBoneName);
		 
		//Recursion
		GetChildBoneNames_Recursive(SkeletalMeshComp, BoneIndex,ChildBoneNames);
	}
}

int32 UVictoryBPFunctionLibrary::GetAllBoneNamesBelowBone( USkeletalMeshComponent* SkeletalMeshComp, FName StartingBoneName,  TArray<FName>& BoneNames )
{
	BoneNames.Empty();
	
	if(!SkeletalMeshComp || !SkeletalMeshComp->SkeletalMesh)
	{
		return -1;
		//~~~~
	}
	 
	int32 StartingBoneIndex = SkeletalMeshComp->GetBoneIndex(StartingBoneName);
	 
	//Recursive
	GetChildBoneNames_Recursive(SkeletalMeshComp, StartingBoneIndex, BoneNames);
	     
	return BoneNames.Num();
}


//~~~~~~
// File IO
//~~~~~~ 
bool UVictoryBPFunctionLibrary::JoyFileIO_GetFiles(TArray<FString>& Files, FString RootFolderFullPath, FString Ext)
{
	if(RootFolderFullPath.Len() < 1) return false;
	
	FPaths::NormalizeDirectoryName(RootFolderFullPath);
	
	IFileManager& FileManager = IFileManager::Get();
	 
	if(!Ext.Contains(TEXT("*")))
	{
		if(Ext == "") 
		{
			Ext = "*.*";
		}
		else
		{
			Ext = (Ext.Left(1) == ".") ? "*" + Ext : "*." + Ext;
		}
	}
	
	FString FinalPath = RootFolderFullPath + "/" + Ext;
	FileManager.FindFiles(Files, *FinalPath, true, false);
	return true;				  
}
bool UVictoryBPFunctionLibrary::JoyFileIO_GetFilesInRootAndAllSubFolders(TArray<FString>& Files, FString RootFolderFullPath, FString Ext)
{
	if(RootFolderFullPath.Len() < 1) return false;
	
	FPaths::NormalizeDirectoryName(RootFolderFullPath);
	
	IFileManager& FileManager = IFileManager::Get();
	 
	if(!Ext.Contains(TEXT("*")))
	{
		if(Ext == "") 
		{
			Ext = "*.*";
		}
		else
		{
			Ext = (Ext.Left(1) == ".") ? "*" + Ext : "*." + Ext;
		}
	}
	
	FileManager.FindFilesRecursive(Files, *RootFolderFullPath, *Ext, true, false);
	return true;
}

bool UVictoryBPFunctionLibrary::ScreenShots_Rename_Move_Most_Recent(
	FString& OriginalFileName,
	FString NewName, 
	FString NewAbsoluteFolderPath, 
	bool HighResolution
){ 
	OriginalFileName = "None";

	//File Name given?
	if(NewName.Len() <= 0) return false;
	
	//Normalize
	FPaths::NormalizeDirectoryName(NewAbsoluteFolderPath);
	
	//Ensure target directory exists, 
	//		_or can be created!_ <3 Rama
	if(!VCreateDirectory(NewAbsoluteFolderPath)) return false;
	
	FString ScreenShotsDir = VictoryPaths__ScreenShotsDir();
	
	//Find all screenshots
	TArray<FString> Files;									//false = not recursive, not expecting subdirectories
	bool Success = GetFiles(ScreenShotsDir, Files, false);
	
	if(!Success)
	{
		return false;
	}
	
	//Filter
	TArray<FString> ToRemove; //16 bytes each, more stable than ptrs though since RemoveSwap is involved
	for(FString& Each : Files)
	{
		if(HighResolution)
		{
			//remove those that dont have it
			if(Each.Left(4) != "High")
			{
				ToRemove.Add(Each);
			}
		}
		else
		{ 
			//Remove those that have it!
			if(Each.Left(4) == "High")
			{
				ToRemove.Add(Each);
			}
		}
	}
	
	//Remove!
	for(FString& Each : ToRemove)
	{
		Files.RemoveSwap(Each); //Fast remove! Does not preserve order
	}
	
	//No files?
	if(Files.Num() < 1)
	{ 
		return false;
	}
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Rama's Sort Files by Time Stamp Feature
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//Sort the file names by time stamp
	//  This is my custom c++ struct to do this,
	//  	combined with my operator< and UE4's
	//			TArray::Sort() function!
	TArray<FFileStampSort> FileSort;
	for(FString& Each : Files)
	{
		FileSort.Add(FFileStampSort(&Each,GetFileTimeStamp(Each)));
		
	}

	//Sort all the file names by their Time Stamp!
	FileSort.Sort();
	
	//Biggest value = last entry
	OriginalFileName = *FileSort.Last().FileName;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	    
	//Generate new Full File Path!
	FString NewFullFilePath = NewAbsoluteFolderPath + "/" + NewName + ".bmp";
	
	//Move File!
	return RenameFile(NewFullFilePath, ScreenShotsDir + "/" + OriginalFileName);
}

void UVictoryBPFunctionLibrary::VictoryISM_GetAllVictoryISMActors(UObject* WorldContextObject, TArray<AVictoryISM*>& Out)
{
	if(!WorldContextObject) return;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	
	Out.Empty();
	for(TActorIterator<AVictoryISM> Itr(World); Itr; ++Itr)
	{
		Out.Add(*Itr);
	}
}
	
void UVictoryBPFunctionLibrary::VictoryISM_ConvertToVictoryISMActors(
	UObject* WorldContextObject, 
	TSubclassOf<AActor> ActorClass, 
	TArray<AVictoryISM*>& CreatedISMActors, 
	bool DestroyOriginalActors,
	int32 MinCountToCreateISM
){
	//User Input Safety
	if(MinCountToCreateISM < 1) MinCountToCreateISM = 1; //require for array access safety
	
	CreatedISMActors.Empty();
	
	if(!WorldContextObject) return;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	
	//I want one array of actors for each unique static mesh asset!  -Rama
	TMap< UStaticMesh*,TArray<AActor*> > VictoryISMMap;
	
	//Note the ActorClass filter on the Actor Iterator! -Rama
	for (TActorIterator<AActor> Itr(World, ActorClass); Itr; ++Itr)
	{
		//Get Static Mesh Component!
		UStaticMeshComponent* Comp = Itr->FindComponentByClass<UStaticMeshComponent>();
		if(!Comp) continue; 
		if(!Comp->IsValidLowLevel()) continue;
		//~~~~~~~~~
		
		//Add Key if not present!
		if(!VictoryISMMap.Contains(Comp->StaticMesh))
		{
			VictoryISMMap.Add(Comp->StaticMesh);
			VictoryISMMap[Comp->StaticMesh].Empty(); //ensure array is properly initialized
		}
		
		//Add the actor!
		VictoryISMMap[Comp->StaticMesh].Add(*Itr);
	}
	  
	//For each Static Mesh Asset in the Victory ISM Map
	for (TMap< UStaticMesh*,TArray<AActor*> >::TIterator It(VictoryISMMap); It; ++It)
	{
		//Get the Actor Array for this particular Static Mesh Asset!
		TArray<AActor*>& ActorArray = It.Value();
		
		//No entries?
		if(ActorArray.Num() < MinCountToCreateISM) continue;
		//~~~~~~~~~~~~~~~~~~
		  
		//Get the Root
		UStaticMeshComponent* RootSMC = ActorArray[0]->FindComponentByClass<UStaticMeshComponent>();
		if(!RootSMC) continue;
		//~~~~~~~~~~
		
		//Gather transforms!
		TArray<FTransform> WorldTransforms;
		for(AActor* Each : ActorArray)
		{
			WorldTransforms.Add(Each->GetTransform());
			 
			//Destroy original?
			if(DestroyOriginalActors)
			{
				Each->Destroy();
			}
		}
		  
		//Create Victory ISM
		FActorSpawnParameters SpawnInfo;
		//SpawnInfo.bNoCollisionFail 		= true; //always create!
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.bDeferConstruction 	= false;
		 
		AVictoryISM* NewISM = World->SpawnActor<AVictoryISM>(
			AVictoryISM::StaticClass(), 
			RootSMC->GetComponentLocation() ,
			RootSMC->GetComponentRotation(), 
			SpawnInfo 
		);
		
		if(!NewISM) continue;
		//~~~~~~~~~~
		
		//Mesh
		NewISM->Mesh->SetStaticMesh(RootSMC->StaticMesh);
	
		//Materials
		const int32 MatTotal = RootSMC->GetNumMaterials();
		for(int32 v = 0; v < MatTotal; v++)
		{
			NewISM->Mesh->SetMaterial(v,RootSMC->GetMaterial(v));
		}
		 
		//Set Transforms!
		for(const FTransform& Each : WorldTransforms)
		{
			NewISM->Mesh->AddInstanceWorldSpace(Each);
		}
		
		//Add new ISM!
		CreatedISMActors.Add(NewISM);
	}
	
	//Clear memory
	VictoryISMMap.Empty();
}
	 
	
	 
void UVictoryBPFunctionLibrary::SaveGameObject_GetAllSaveSlotFileNames(TArray<FString>& FileNames)
{
	FileNames.Empty();
	FString Path = VictoryPaths__SavedDir() + "SaveGames";
	GetFiles(Path,FileNames); //see top of this file, my own file IO code - Rama
}

//~~~ Victory Paths ~~~

FString UVictoryBPFunctionLibrary::VictoryPaths__Win64Dir_BinaryLocation()
{
	return FString(FPlatformProcess::BaseDir());
}

FString UVictoryBPFunctionLibrary::VictoryPaths__WindowsNoEditorDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::RootDir());
}

FString UVictoryBPFunctionLibrary::VictoryPaths__GameRootDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::GameDir());
}

FString UVictoryBPFunctionLibrary::VictoryPaths__SavedDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir());
}

FString UVictoryBPFunctionLibrary::VictoryPaths__ScreenShotsDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir());
}
 
FString UVictoryBPFunctionLibrary::VictoryPaths__LogsDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::GameLogDir());
}


//~~~~~~~~~~~~~~~~~
	
	
FVector2D UVictoryBPFunctionLibrary::Vector2DInterpTo(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed)
{
	return FMath::Vector2DInterpTo( Current, Target, DeltaTime, InterpSpeed );
}
FVector2D UVictoryBPFunctionLibrary::Vector2DInterpTo_Constant(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed)
{
	return FMath::Vector2DInterpConstantTo( Current, Target, DeltaTime, InterpSpeed );
}

float UVictoryBPFunctionLibrary::MapRangeClamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB)
{ 
	return FMath::GetMappedRangeValueClamped(FVector2D(InRangeA,InRangeB),FVector2D(OutRangeA,OutRangeB),Value);
}

FVictoryInput UVictoryBPFunctionLibrary::VictoryGetVictoryInput(const FKeyEvent& KeyEvent)
{
	FVictoryInput VInput;
	 
	VInput.Key 			= KeyEvent.GetKey();
	VInput.KeyAsString 	= VInput.Key.GetDisplayName().ToString();
	
	VInput.bAlt 		= KeyEvent.IsAltDown();
	VInput.bCtrl 		= KeyEvent.IsControlDown();
	VInput.bShift 	= KeyEvent.IsShiftDown();
	VInput.bCmd 		= KeyEvent.IsCommandDown();
	
	return VInput;
}
FVictoryInputAxis UVictoryBPFunctionLibrary::VictoryGetVictoryInputAxis(const FKeyEvent& KeyEvent)
{
	FVictoryInputAxis VInput;
	 
	VInput.Key 			= KeyEvent.GetKey();
	VInput.KeyAsString 	= VInput.Key.GetDisplayName().ToString();
	
	VInput.Scale = 1;
	
	return VInput;
}

void UVictoryBPFunctionLibrary::VictoryGetAllAxisKeyBindings(TArray<FVictoryInputAxis>& Bindings)
{
	Bindings.Empty();
	 
	const UInputSettings* Settings = GetDefault<UInputSettings>();
	if(!Settings) return;
	
	const TArray<FInputAxisKeyMapping>& Axi = Settings->AxisMappings;
	
	for(const FInputAxisKeyMapping& Each : Axi)
	{
		Bindings.Add(FVictoryInputAxis(Each));
	}
}

void UVictoryBPFunctionLibrary::VictoryGetAllActionKeyBindings(TArray<FVictoryInput>& Bindings)
{
	Bindings.Empty();
	 
	const UInputSettings* Settings = GetDefault<UInputSettings>();
	if(!Settings) return;
	
	const TArray<FInputActionKeyMapping>& Actions = Settings->ActionMappings;
	
	for(const FInputActionKeyMapping& Each : Actions)
	{
		Bindings.Add(FVictoryInput(Each));
	}
}

bool UVictoryBPFunctionLibrary::VictoryReBindAxisKey(FVictoryInputAxis Original, FVictoryInputAxis NewBinding)
{
	UInputSettings* Settings = const_cast<UInputSettings*>(GetDefault<UInputSettings>());
	if(!Settings) return false;

	TArray<FInputAxisKeyMapping>& Axi = Settings->AxisMappings;
	
	//~~~
	 
	bool Found = false;
	for(FInputAxisKeyMapping& Each : Axi)
	{
		//Search by original
		if(Each.AxisName.ToString() == Original.AxisName &&
			Each.Key == Original.Key
		){   
			//Update to new!
			UVictoryBPFunctionLibrary::UpdateAxisMapping(Each,NewBinding);
			Found = true;
			break;
		}  
	}
	 
	if(Found) 
	{
		//SAVES TO DISK
		const_cast<UInputSettings*>(Settings)->SaveKeyMappings();
		   
		//REBUILDS INPUT, creates modified config in Saved/Config/Windows/Input.ini
		for (TObjectIterator<UPlayerInput> It; It; ++It)
		{
			It->ForceRebuildingKeyMaps(true);
		}
	}
	return Found;
}

bool UVictoryBPFunctionLibrary::VictoryReBindActionKey(FVictoryInput Original, FVictoryInput NewBinding)
{
	UInputSettings* Settings = const_cast<UInputSettings*>(GetDefault<UInputSettings>());
	if(!Settings) return false;

	TArray<FInputActionKeyMapping>& Actions = Settings->ActionMappings;
	
	//~~~
	
	bool Found = false;
	for(FInputActionKeyMapping& Each : Actions)
	{
		//Search by original
		if(Each.ActionName.ToString() == Original.ActionName &&
			Each.Key == Original.Key
		){   
			//Update to new!
			UVictoryBPFunctionLibrary::UpdateActionMapping(Each,NewBinding);
			Found = true;
			break;
		}  
	}
	
	if(Found) 
	{
		//SAVES TO DISK
		const_cast<UInputSettings*>(Settings)->SaveKeyMappings();
		   
		//REBUILDS INPUT, creates modified config in Saved/Config/Windows/Input.ini
		for (TObjectIterator<UPlayerInput> It; It; ++It)
		{
			It->ForceRebuildingKeyMaps(true);
		}
	}
	return Found;
}

void UVictoryBPFunctionLibrary::GetAllWidgetsOfClass(UObject* WorldContextObject, TSubclassOf<UUserWidget> WidgetClass, TArray<UUserWidget*>& FoundWidgets,bool TopLevelOnly)
{
	//Prevent possibility of an ever-growing array if user uses this in a loop
	FoundWidgets.Empty();
	//~~~~~~~~~~~~
	 
	if(!WidgetClass) return;
	if(!WorldContextObject) return;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	
	for(TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
	{
		if(Itr->GetWorld() != World) continue;
		//~~~~~~~~~~~~~~~~~~~~~
		
		if( ! Itr->IsA(WidgetClass)) continue;
		//~~~~~~~~~~~~~~~~~~~
		 
		//Top Level?
		if(TopLevelOnly)
		{
			//only add top level widgets
			if(Itr->IsInViewport())			
			{
				FoundWidgets.Add(*Itr);
			}
		}
		else
		{
			//add all internal widgets
			FoundWidgets.Add(*Itr);
		}
	}
} 
void UVictoryBPFunctionLibrary::RemoveAllWidgetsOfClass(UObject* WorldContextObject, TSubclassOf<UUserWidget> WidgetClass)
{
	if(!WidgetClass) return;
	if(!WorldContextObject) return;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	 
	for(TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
	{
		if(Itr->GetWorld() != World) continue;
		//~~~~~~~~~~~~~~~~~~~~~
		
		if( ! Itr->IsA(WidgetClass)) continue;
		//~~~~~~~~~~~~~~~~~~~
		 
		//only add top level widgets
		if(Itr->IsInViewport())			//IsInViewport in 4.6
		{
			Itr->RemoveFromViewport();
		}
	}
}

bool UVictoryBPFunctionLibrary::IsWidgetOfClassInViewport(UObject* WorldContextObject, TSubclassOf<UUserWidget> WidgetClass)
{ 
	if(!WidgetClass) return false;
	if(!WorldContextObject) return false;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return false;
	//~~~~~~~~~~~
	  
	for(TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
	{
		if(Itr->GetWorld() != World) continue;
		//~~~~~~~~~~~~~~~~~~~~~
		
		if( ! Itr->IsA(WidgetClass)) continue;
		//~~~~~~~~~~~~~~~~~~~
		    
		if(Itr->GetIsVisible())			//IsInViewport in 4.6
		{
			return true;
		}
	}
	
	return false;
}
 
void UVictoryBPFunctionLibrary::ServerTravel(UObject* WorldContextObject, FString MapName,bool bNotifyPlayers)
{ 
	if(!WorldContextObject) return;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	 
	World->ServerTravel(MapName,false,bNotifyPlayers); //abs //notify players
}
APlayerStart* UVictoryBPFunctionLibrary::GetPlayerStart(UObject* WorldContextObject,FString PlayerStartName)
{
	if(!WorldContextObject) return nullptr;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return nullptr;
	//~~~~~~~~~~~
	
	for(TActorIterator<APlayerStart> Itr(World); Itr; ++Itr)
	{
		if(Itr->GetName() == PlayerStartName)
		{
			return *Itr;
		}
	}
	return nullptr;
}

bool UVictoryBPFunctionLibrary::VictorySoundVolumeChange(USoundClass* SoundClassObject, float NewVolume)
 {
	if(!SoundClassObject) 
	{
		return false;
	}
	
	SoundClassObject->Properties.Volume = NewVolume;
	return true; 
	   
	 /*
	FAudioDevice* Device = GEngine->GetAudioDevice();
	if (!Device || !SoundClassObject)
	{
		return false;
	}
	    
	bool bFound = Device->SoundClasses.Contains(SoundClassObject);
	if(bFound)
	{ 
		Device->SetClassVolume(SoundClassObject, NewVolume);
		return true;
	}
	return false;
	*/
	
	/*
		bool SetBaseSoundMix( class USoundMix* SoundMix );
	
	*/
 }
float UVictoryBPFunctionLibrary::VictoryGetSoundVolume(USoundClass* SoundClassObject)
{
	FAudioDevice* Device = GEngine->GetMainAudioDevice();
	if (!Device || !SoundClassObject)
	{
		return -1;
	}
	    
	FSoundClassProperties* Props = Device->GetSoundClassCurrentProperties(SoundClassObject);
	if(!Props) return -1;
	return Props->Volume;
}








//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool UVictoryBPFunctionLibrary::VictoryGetCustomConfigVar_Bool(FString SectionName,FString VariableName)
{
	if(!GConfig) return false;
	//~~~~~~~~~~~
 
	bool Value;
	GConfig->GetBool(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
	return Value;
}
int32 UVictoryBPFunctionLibrary::VictoryGetCustomConfigVar_Int(FString SectionName,FString VariableName)
{
	if(!GConfig) return 0;
	//~~~~~~~~~~~
 
	int32 Value;
	GConfig->GetInt(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
	return Value;
}
float UVictoryBPFunctionLibrary::VictoryGetCustomConfigVar_Float(FString SectionName,FString VariableName)
{
	if(!GConfig) return 0;
	//~~~~~~~~~~~
 
	float Value;
	GConfig->GetFloat(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
	return Value;
}
FVector UVictoryBPFunctionLibrary::VictoryGetCustomConfigVar_Vector(FString SectionName,FString VariableName)
{
	if(!GConfig) return FVector::ZeroVector;
	//~~~~~~~~~~~
 
	FVector Value;
	GConfig->GetVector(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
	return Value;
}
FRotator UVictoryBPFunctionLibrary::VictoryGetCustomConfigVar_Rotator(FString SectionName,FString VariableName)
{
	if(!GConfig) return FRotator::ZeroRotator;
	//~~~~~~~~~~~
 
	FRotator Value;
	GConfig->GetRotator(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
	return Value;
}
FLinearColor UVictoryBPFunctionLibrary::VictoryGetCustomConfigVar_Color(FString SectionName,FString VariableName)
{
	if(!GConfig) return FColor::Black;
	//~~~~~~~~~~~
  
	FColor Value;
	GConfig->GetColor(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
	return FLinearColor(Value);
}
FString UVictoryBPFunctionLibrary::VictoryGetCustomConfigVar_String(FString SectionName,FString VariableName)
{
	if(!GConfig) return "";
	//~~~~~~~~~~~
 
	FString Value;
	GConfig->GetString(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
	return Value;
}

FVector2D UVictoryBPFunctionLibrary::VictoryGetCustomConfigVar_Vector2D(FString SectionName, FString VariableName)
{
	if(!GConfig) return FVector2D::ZeroVector;
	//~~~~~~~~~~~
 
	FVector Value;
	GConfig->GetVector(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
	return FVector2D(Value.X,Value.Y);
} 
  
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void UVictoryBPFunctionLibrary::VictorySetCustomConfigVar_Vector2D(FString SectionName, FString VariableName, FVector2D Value)
{
	if(!GConfig) return;
	//~~~~~~~~~~~
	
	GConfig->SetVector(
		*SectionName,
		*VariableName,
		FVector(Value.X,Value.Y,0),
		GGameIni
	);
}

void UVictoryBPFunctionLibrary::VictorySetCustomConfigVar_Bool(FString SectionName,FString VariableName, bool Value)
{
	if(!GConfig) return;
	//~~~~~~~~~~~
 
	GConfig->SetBool(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
}
void UVictoryBPFunctionLibrary::VictorySetCustomConfigVar_Int(FString SectionName,FString VariableName, int32 Value)
{
	if(!GConfig) return;
	//~~~~~~~~~~~
 
	GConfig->SetInt(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
}
void UVictoryBPFunctionLibrary::VictorySetCustomConfigVar_Float(FString SectionName,FString VariableName, float Value)
{
	if(!GConfig) return;
	//~~~~~~~~~~~
	
	GConfig->SetFloat(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
}
void UVictoryBPFunctionLibrary::VictorySetCustomConfigVar_Vector(FString SectionName,FString VariableName, FVector Value)
{
	if(!GConfig) return;
	//~~~~~~~~~~~
	
	GConfig->SetVector(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
}
void UVictoryBPFunctionLibrary::VictorySetCustomConfigVar_Rotator(FString SectionName,FString VariableName, FRotator Value)
{
	if(!GConfig) return;
	//~~~~~~~~~~~
	
	GConfig->SetRotator(
		*SectionName,
		*VariableName,
		Value,
		GGameIni
	);
}
void UVictoryBPFunctionLibrary::VictorySetCustomConfigVar_Color(FString SectionName,FString VariableName, FLinearColor Value)
{
	if(!GConfig) return;
	//~~~~~~~~~~~
	 
	GConfig->SetColor(
		*SectionName,
		*VariableName,
		Value.ToFColor(true),
		GGameIni
	);
}
void UVictoryBPFunctionLibrary::VictorySetCustomConfigVar_String(FString SectionName,FString VariableName, FString Value)
{
	if(!GConfig) return;
	//~~~~~~~~~~~
 
	GConfig->SetString(
		*SectionName,
		*VariableName,
		*Value,
		GGameIni
	);
}






//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~























UObject* UVictoryBPFunctionLibrary::LoadObjectFromAssetPath(TSubclassOf<UObject> ObjectClass,FName Path,bool& IsValid)
{
	IsValid = false;
	
	if(Path == NAME_None) return NULL;
	//~~~~~~~~~~~~~~~~~~~~~
	
	UObject* LoadedObj = StaticLoadObject( ObjectClass, NULL,*Path.ToString());
	 
	IsValid = LoadedObj != nullptr;
	
	return LoadedObj;
}
FName UVictoryBPFunctionLibrary::GetObjectPath(UObject* Obj)
{
	if(!Obj) return NAME_None;
	if(!Obj->IsValidLowLevel()) return NAME_None;
	//~~~~~~~~~~~~~~~~~~~~~~~~~
	
	FStringAssetReference ThePath = FStringAssetReference(Obj);
		
	if(!ThePath.IsValid()) return "";
	
	//The Class FString Name For This Object
	FString str=Obj->GetClass()->GetDescription();
	
	//Remove spaces in Material Instance Constant class description!
	str = str.Replace(TEXT(" "),TEXT(""));
	
	str += "'";
	str += ThePath.ToString();
	str += "'";
	
	return FName(*str);
}
int32 UVictoryBPFunctionLibrary::GetPlayerUniqueNetID()
{
	TObjectIterator<APlayerController> ThePC;
	if(!ThePC) 					return -1; 
	if(!ThePC->PlayerState) return -1;
	//~~~~~~~~~~~~~~~~~~~
	
	return ThePC->PlayerState->PlayerId;
}
UObject* UVictoryBPFunctionLibrary::CreateObject(UObject* WorldContextObject,UClass* TheObjectClass)
{
	if(!TheObjectClass) return NULL;
	//~~~~~~~~~~~~~~~~~
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return NULL;
	//~~~~~~~~~~~
	    
	//Need to submit pull request for custom name and custom class both
	return NewObject<UObject>(World,TheObjectClass);
}
UPrimitiveComponent* UVictoryBPFunctionLibrary::CreatePrimitiveComponent(
	UObject* WorldContextObject, 
	TSubclassOf<UPrimitiveComponent> CompClass, 
	FName Name,
	FVector Location, 
	FRotator Rotation
){
	if(!CompClass) return NULL;
	//~~~~~~~~~~~~~~~~~
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return NULL;
	//~~~~~~~~~~~
	 
	UPrimitiveComponent* NewComp = NewObject<UPrimitiveComponent>(World, Name);
	if(!NewComp) return NULL;
	//~~~~~~~~~~~~~
	 
	NewComp->SetWorldLocation(Location);
	NewComp->SetWorldRotation(Rotation);
	NewComp->RegisterComponentWithWorld(World);
	
	return NewComp;
}

AActor* UVictoryBPFunctionLibrary::SpawnActorIntoLevel(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FName Level, FVector Location, FRotator Rotation,bool SpawnEvenIfColliding)
{
	if(!ActorClass) return NULL;
	//~~~~~~~~~~~~~~~~~
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return NULL;
	//~~~~~~~~~~~
	
	FActorSpawnParameters SpawnParameters;
	if (SpawnEvenIfColliding)
	{
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	}
	
	SpawnParameters.bDeferConstruction = false;
	 
	 
	//Get Level from Name!
	ULevel* FoundLevel = NULL;
	
	for(const ULevelStreaming* EachLevel : World->StreamingLevels)
	{
		if( ! EachLevel) continue;
		//~~~~~~~~~~~~~~~~
	
		ULevel* LevelPtr = EachLevel->GetLoadedLevel();
		
		//Valid?
		if(!LevelPtr) continue;
		
		if(EachLevel->GetWorldAssetPackageFName() == Level)
		{
			FoundLevel = LevelPtr; 
			break;
		} 
	}
	//~~~~~~~~~~~~~~~~~~~~~
	if(FoundLevel)
	{
		SpawnParameters.OverrideLevel = FoundLevel;
	}
	//~~~~~~~~~~~~~~~~~~~~~
	
	return World->SpawnActor( ActorClass, &Location, &Rotation, SpawnParameters);
	
}
void UVictoryBPFunctionLibrary::GetNamesOfLoadedLevels(UObject* WorldContextObject, TArray<FString>& NamesOfLoadedLevels)
{
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	
	NamesOfLoadedLevels.Empty();
	
	//Get Level from Name!
	ULevel* FoundLevel = NULL;
	
	for(const ULevelStreaming* EachLevel : World->StreamingLevels)
	{
		if( ! EachLevel) continue;
		//~~~~~~~~~~~~~~~~
	
		ULevel* LevelPtr = EachLevel->GetLoadedLevel();
		
		//Valid?
		if(!LevelPtr) continue;
		
		//Is This Level Visible?
		if(!LevelPtr->bIsVisible) continue;
		//~~~~~~~~~~~~~~~~~~~
		 
		NamesOfLoadedLevels.Add(EachLevel->GetWorldAssetPackageFName().ToString());
	}
}
		
	
void UVictoryBPFunctionLibrary::Loops_ResetBPRunawayCounter()
{
	//Reset Runaway loop counter (use carefully)
	GInitRunaway();
}

void UVictoryBPFunctionLibrary::GraphicsSettings__SetFrameRateToBeUnbound()
{
	if(!GEngine) return;
	//~~~~~~~~~
	
	GEngine->bSmoothFrameRate = 0; 
}
void UVictoryBPFunctionLibrary::GraphicsSettings__SetFrameRateCap(float NewValue)
{
	if(!GEngine) return;
	//~~~~~~~~~
	   
	GEngine->bSmoothFrameRate = 1; 
	GEngine->SmoothedFrameRateRange = FFloatRange(10,NewValue);
}

FVector2D UVictoryBPFunctionLibrary::ProjectWorldToScreenPosition(const FVector& WorldLocation)
{
	TObjectIterator<APlayerController> ThePC;
	if(!ThePC) return FVector2D::ZeroVector;
	
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(ThePC->Player);
	if (LocalPlayer != NULL && LocalPlayer->ViewportClient != NULL && LocalPlayer->ViewportClient->Viewport != NULL)
	{
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		
		// Create a view family for the game viewport
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues(
			LocalPlayer->ViewportClient->Viewport,
			ThePC->GetWorld()->Scene,
			LocalPlayer->ViewportClient->EngineShowFlags )
			.SetRealtimeUpdate(true) );

		// Calculate a view where the player is to update the streaming from the players start location
		FVector ViewLocation;
		FRotator ViewRotation;
		FSceneView* SceneView = LocalPlayer->CalcSceneView( &ViewFamily, /*out*/ ViewLocation, /*out*/ ViewRotation, LocalPlayer->ViewportClient->Viewport );

		//Valid Scene View?
		if (SceneView) 
		{
			//Return
			FVector2D ScreenLocation;
			SceneView->WorldToPixel(WorldLocation,ScreenLocation );
			return ScreenLocation;
		}
	} 
	
	return FVector2D::ZeroVector;
}



bool UVictoryBPFunctionLibrary::GetStaticMeshVertexLocations(UStaticMeshComponent* Comp, TArray<FVector>& VertexPositions)
{
	VertexPositions.Empty();
	 
	if(!Comp) 						
	{
		return false;
	}
	
	if(!Comp->IsValidLowLevel()) 
	{
		return false;
	}
	//~~~~~~~~~~~~~~~~~~~~~~~
	
	//Component Transform
	FTransform RV_Transform = Comp->GetComponentTransform(); 
	
	//Body Setup valid?
	UBodySetup* BodySetup = Comp->GetBodySetup();
	
	if(!BodySetup || !BodySetup->IsValidLowLevel())
	{
		return false;
	}  
	
	for(PxTriangleMesh* EachTriMesh : BodySetup->TriMeshes)
	{
		if (!EachTriMesh)
		{
			return false;
		}
		//~~~~~~~~~~~~~~~~

		//Number of vertices
		PxU32 VertexCount = EachTriMesh->getNbVertices();

		//Vertex array
		const PxVec3* Vertices = EachTriMesh->getVertices();

		//For each vertex, transform the position to match the component Transform 
		for (PxU32 v = 0; v < VertexCount; v++)
		{
			VertexPositions.Add(RV_Transform.TransformPosition(P2UVector(Vertices[v])));
		}
	}
	return true;
	
	/*
	//See this wiki for more info on getting triangles
	//		https://wiki.unrealengine.com/Accessing_mesh_triangles_and_vertex_positions_in_build
	*/
} 


void UVictoryBPFunctionLibrary::AddToActorRotation(AActor* TheActor, FRotator AddRot)
{
	if (!TheActor) return;
	//~~~~~~~~~~~

	FTransform TheTransform = TheActor->GetTransform();
	TheTransform.ConcatenateRotation(AddRot.Quaternion());
	TheTransform.NormalizeRotation();
	TheActor->SetActorTransform(TheTransform);
}




void UVictoryBPFunctionLibrary::DrawCircle(
	UObject* WorldContextObject,
	FVector Center, 
	float Radius, 
	int32 NumPoints,
	float Thickness,
	FLinearColor LineColor,
	FVector YAxis,
	FVector ZAxis,
	float Duration,
	bool PersistentLines
){ 
	
	
	if(!WorldContextObject) return ;
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	 
	/* //FOR PULL REQUEST TO EPIC
	FMatrix TM;
	TM.SetOrigin(Center);
	TM.SetAxis(0, FVector(1,0,0));
	TM.SetAxis(1, YAxis);
	TM.SetAxis(2, ZAxis);
	
	DrawDebugCircle(
		World, 
		TM, 
		Radius, NumPoints, 
		FColor::Red, 
		false, 
		-1.f, 
		0
	);
*/
	 
	// Need at least 4 segments  
	NumPoints = FMath::Max(NumPoints, 4);
	const float AngleStep = 2.f * PI / float(NumPoints);

	float Angle = 0.f;
	for(int32 v = 0; v < NumPoints; v++)
	{  
		const FVector Vertex1 = Center + Radius * (YAxis * FMath::Cos(Angle) + ZAxis * FMath::Sin(Angle));
		Angle += AngleStep;
		const FVector Vertex2 = Center + Radius * (YAxis * FMath::Cos(Angle) + ZAxis * FMath::Sin(Angle));
		   
		DrawDebugLine(
			World, 
			Vertex1, 
			Vertex2,
			LineColor.ToFColor(true),  
			PersistentLines, 
			Duration,
			0, 				//depth  
			Thickness          
		);
	}
}


bool UVictoryBPFunctionLibrary::LoadStringArrayFromFile(TArray<FString>& StringArray, int32& ArraySize, FString FullFilePath, bool ExcludeEmptyLines)
{
	ArraySize = 0;
	
	if(FullFilePath == "" || FullFilePath == " ") return false;
	
	//Empty any previous contents!
	StringArray.Empty();
	
	TArray<FString> FileArray;
	
	if( ! FFileHelper::LoadANSITextFileToStrings(*FullFilePath, NULL, FileArray))
	{
		return false;
	}
	
	if(ExcludeEmptyLines)
	{
		for(const FString& Each : FileArray )
		{
			if(Each == "") continue;
			//~~~~~~~~~~~~~
			
			//check for any non whitespace
			bool FoundNonWhiteSpace = false;
			for(int32 v = 0; v < Each.Len(); v++)
			{
				if(Each[v] != ' ' && Each[v] != '\n')
				{
					FoundNonWhiteSpace = true;
					break;
				}
				//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			}
			
			if(FoundNonWhiteSpace)
			{
				StringArray.Add(Each);
			}
		}
	}
	else
	{
		StringArray.Append(FileArray);
	}
	
	ArraySize = StringArray.Num();
	return true; 
}

 
AActor* UVictoryBPFunctionLibrary::GetClosestActorOfClassInRadiusOfLocation(
	UObject* WorldContextObject, 
	TSubclassOf<AActor> ActorClass, 
	FVector Center, 
	float Radius, 
	bool& IsValid
){ 
	IsValid = false;
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return NULL;
	//~~~~~~~~~~~
	
	AActor* ClosestActor 		= NULL;
	float MinDistanceSq 		= Radius*Radius;	//Max Radius
	
	for (TActorIterator<AActor> Itr(World, ActorClass); Itr; ++Itr)
	{
		const float DistanceSquared = FVector::DistSquared(Center, Itr->GetActorLocation());

		//Is this the closest possible actor within the max radius?
		if (DistanceSquared < MinDistanceSq)
		{
			ClosestActor = *Itr;					//New Output!
			MinDistanceSq = DistanceSquared;		//New Min!
		}
	}

   IsValid = true;
   return ClosestActor;
} 

AActor* UVictoryBPFunctionLibrary::GetClosestActorOfClassInRadiusOfActor(
	UObject* WorldContextObject, 
	TSubclassOf<AActor> ActorClass, 
	AActor* ActorCenter, 
	float Radius, 
	bool& IsValid
){ 
	IsValid = false;
	  
	if(!ActorCenter)
	{
		return nullptr;
	}
	
	const FVector Center = ActorCenter->GetActorLocation();
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return NULL;
	//~~~~~~~~~~~
	
	AActor* ClosestActor 		= NULL;
	float MinDistanceSq 		= Radius*Radius;	//Max Radius
	
	for (TActorIterator<AActor> Itr(World, ActorClass); Itr; ++Itr)
	{
		//Skip ActorCenter!
		if(*Itr == ActorCenter) continue;
		//~~~~~~~~~~~~~~~~~
		
		const float DistanceSquared = FVector::DistSquared(Center, Itr->GetActorLocation());

		//Is this the closest possible actor within the max radius?
		if (DistanceSquared < MinDistanceSq)
		{
			ClosestActor = *Itr;					//New Output!
			MinDistanceSq = DistanceSquared;		//New Min!
		}
	}

   IsValid = true;
   return ClosestActor;
}

void UVictoryBPFunctionLibrary::Selection_SelectionBox(UObject* WorldContextObject,TArray<AActor*>& SelectedActors, FVector2D AnchorPoint,FVector2D DraggedPoint,TSubclassOf<AActor> ClassFilter)
{
	if(!WorldContextObject) return ;
	
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	
	SelectedActors.Empty();
	
	FBox2D Box;
	Box+=DraggedPoint;
	Box+=AnchorPoint;
	
	for(TActorIterator<AActor> Itr(World); Itr; ++Itr)
	{
		if(!Itr->IsA(ClassFilter)) continue;
		//~~~~~~~~~~~~~~~~~~
		
		if(Box.IsInside(UVictoryBPFunctionLibrary::ProjectWorldToScreenPosition(Itr->GetActorLocation())))
		{
			SelectedActors.Add(*Itr);
		}
	}
}

bool UVictoryBPFunctionLibrary::PlayerController_GetControllerID(APlayerController* ThePC, int32& ControllerID)
{
	if(!ThePC) return false;
	
	ULocalPlayer * LP = Cast<ULocalPlayer>(ThePC->Player);
    if(!LP) return false;
  
    ControllerID = LP->GetControllerId();
	
	return true;
}
	
bool UVictoryBPFunctionLibrary::PlayerState_GetPlayerID(APlayerController* ThePC, int32& PlayerID)
{
	if(!ThePC) return false;
	
	if(!ThePC->PlayerState) return false;
	
	PlayerID = ThePC->PlayerState->PlayerId;
	
	return true;
}

void UVictoryBPFunctionLibrary::Open_URL_In_Web_Browser(FString TheURL)
{
	FPlatformProcess::LaunchURL( *TheURL, nullptr, nullptr );
}

void UVictoryBPFunctionLibrary::OperatingSystem__GetCurrentPlatform(
	bool& Windows_, 		//some weird bug of making it all caps engine-side
	bool& Mac,
	bool& Linux, 
	bool& iOS, 
	bool& Android,
	bool& PS4,
	bool& XBoxOne,
	bool& HTML5,
	bool& WinRT_Arm,
	bool& WinRT
){
	//#define's in UE4 source code
	Windows_ = 				PLATFORM_WINDOWS;
	Mac = 						PLATFORM_MAC;
	Linux = 					PLATFORM_LINUX;
	
	PS4 = 						PLATFORM_PS4;
	XBoxOne = 				PLATFORM_XBOXONE;
	
	iOS = 						PLATFORM_IOS;
	Android = 				PLATFORM_ANDROID;
	
	HTML5 = 					PLATFORM_HTML5;
	
	WinRT_Arm =	 			PLATFORM_WINRT_ARM;
	WinRT 	= 				PLATFORM_WINRT;
}

FString UVictoryBPFunctionLibrary::RealWorldTime__GetCurrentOSTime( 
	int32& MilliSeconds,
	int32& Seconds, 
	int32& Minutes, 
	int32& Hours12,
	int32& Hours24,
	int32& Day,
	int32& Month,
	int32& Year
){
	const FDateTime Now = FDateTime::Now();
	
	MilliSeconds = 		Now.GetMillisecond( );
	Seconds = 			Now.GetSecond( );
	Minutes = 				Now.GetMinute( );
	Hours12 = 			Now.GetHour12( );
	Hours24 = 			Now.GetHour( ); //24
	Day = 					Now.GetDay( );
	Month = 				Now.GetMonth( );
	Year = 				Now.GetYear( );
	
	return Now.ToString();
}

void UVictoryBPFunctionLibrary::RealWorldTime__GetTimePassedSincePreviousTime(
		const FString& PreviousTime,
		float& Milliseconds,
		float& Seconds,
		float& Minutes,
		float& Hours
){
	FDateTime ParsedDateTime;
	FDateTime::Parse(PreviousTime,ParsedDateTime);
	const FTimespan TimeDiff = FDateTime::Now() - ParsedDateTime;
	
	Milliseconds 	= TimeDiff.GetTotalMilliseconds( );
	Seconds 		= TimeDiff.GetTotalSeconds( );
	Minutes 		= TimeDiff.GetTotalMinutes( );
	Hours 			= TimeDiff.GetTotalHours( );
}
	
void UVictoryBPFunctionLibrary::RealWorldTime__GetDifferenceBetweenTimes(
		const FString& PreviousTime1,
		const FString& PreviousTime2,
		float& Milliseconds,
		float& Seconds,
		float& Minutes,
		float& Hours
){
	FDateTime ParsedDateTime1;
	FDateTime::Parse(PreviousTime1,ParsedDateTime1);
	
	FDateTime ParsedDateTime2;
	FDateTime::Parse(PreviousTime2,ParsedDateTime2);
	
	FTimespan TimeDiff; 
	
	if(PreviousTime1 < PreviousTime2)
	{
		TimeDiff = ParsedDateTime2 - ParsedDateTime1;
	}
	else
	{
		TimeDiff = ParsedDateTime1 - ParsedDateTime2;
	}
	
	Milliseconds 	= TimeDiff.GetTotalMilliseconds( );
	Seconds 		= TimeDiff.GetTotalSeconds( );
	Minutes 		= TimeDiff.GetTotalMinutes( );
	Hours 			= TimeDiff.GetTotalHours( );
}



void UVictoryBPFunctionLibrary::MaxOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMaxValue, float& MaxValue)
{
	MaxValue = UVictoryBPFunctionLibrary::Max(FloatArray,&IndexOfMaxValue);
}

void UVictoryBPFunctionLibrary::MaxOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMaxValue, int32& MaxValue)
{
	MaxValue = UVictoryBPFunctionLibrary::Max(IntArray,&IndexOfMaxValue);
}

void UVictoryBPFunctionLibrary::MinOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMinValue, float& MinValue)
{
	MinValue = UVictoryBPFunctionLibrary::Min(FloatArray,&IndexOfMinValue);
}

void UVictoryBPFunctionLibrary::MinOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMinValue, int32& MinValue)
{
	MinValue = UVictoryBPFunctionLibrary::Min(IntArray,&IndexOfMinValue);
}



bool UVictoryBPFunctionLibrary::CharacterMovement__SetMaxMoveSpeed(ACharacter* TheCharacter, float NewMaxMoveSpeed)
{
	if(!TheCharacter)
	{
		return false;
	}
	if(!TheCharacter->CharacterMovement)
	{
		return false;
	}
	
	TheCharacter->CharacterMovement->MaxWalkSpeed = NewMaxMoveSpeed;
	
	return true;
}
	


int32 UVictoryBPFunctionLibrary::Conversion__FloatToRoundedInteger(float IN_Float)
{
	return FGenericPlatformMath::RoundToInt(IN_Float);
}


bool UVictoryBPFunctionLibrary::HasSubstring(const FString& SearchIn, const FString& Substring, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir)
{
	return SearchIn.Contains(Substring, SearchCase, SearchDir);
}

FString UVictoryBPFunctionLibrary::String__CombineStrings(FString StringFirst, FString StringSecond, FString Separator, FString StringFirstLabel, FString StringSecondLabel)
{
	return StringFirstLabel + StringFirst + Separator + StringSecondLabel + StringSecond;
}
FString UVictoryBPFunctionLibrary::String__CombineStrings_Multi(FString A, FString B)
{  
	return A + " " + B;
}

bool UVictoryBPFunctionLibrary::OptionsMenu__GetDisplayAdapterScreenResolutions(TArray<int32>& Widths, TArray<int32>& Heights, TArray<int32>& RefreshRates,bool IncludeRefreshRates)
{
	//Clear any Previous
	Widths.Empty();
	Heights.Empty();
	RefreshRates.Empty();
	
	TArray<FString> Unique;	
	
	FScreenResolutionArray Resolutions;
	if(RHIGetAvailableResolutions(Resolutions, false))
	{
		for (const FScreenResolutionRHI& EachResolution : Resolutions)
		{
			FString Str = "";
			Str += FString::FromInt(EachResolution.Width);
			Str += FString::FromInt(EachResolution.Height);
			
			//Include Refresh Rates?
			if(IncludeRefreshRates)
			{
				Str += FString::FromInt(EachResolution.RefreshRate);
			}		
			
			if(Unique.Contains(Str))
			{
				//Skip! This is duplicate!
				continue;
			}
			else
			{
				//Add to Unique List!
				Unique.AddUnique(Str);
			}
			
			//Add to Actual Data Output!
			Widths.Add(			EachResolution.Width);
			Heights.Add(			EachResolution.Height);
			RefreshRates.Add(	EachResolution.RefreshRate);
		}

		return true;
	}
	return false;
}


//Make .h's for these two!
FRotator UVictoryBPFunctionLibrary::TransformVectorToActorSpaceAngle(AActor* Actor, const FVector& InVector)
{
	if(!Actor) return FRotator::ZeroRotator;
	return Actor->ActorToWorld().InverseTransformVector(InVector).Rotation();
}
FVector UVictoryBPFunctionLibrary::TransformVectorToActorSpace(AActor* Actor, const FVector& InVector)
{
	if(!Actor) return FVector::ZeroVector;
	return Actor->ActorToWorld().InverseTransformVector(InVector);
}

AStaticMeshActor* UVictoryBPFunctionLibrary::Clone__StaticMeshActor(UObject* WorldContextObject, bool&IsValid, AStaticMeshActor* ToClone, FVector LocationOffset,FRotator RotationOffset)
{
	IsValid = false;
	if(!ToClone) return NULL;
	if(!ToClone->IsValidLowLevel()) return NULL;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~
	  
	if(!WorldContextObject) return NULL;
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return NULL;
	//~~~~~~~~~~~
	
	//For BPS
	UClass* SpawnClass = ToClone->GetClass();
	
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.Owner 				= ToClone;
	SpawnInfo.Instigator				= NULL;
	SpawnInfo.bDeferConstruction 	= false;
	
	AStaticMeshActor* NewSMA = World->SpawnActor<AStaticMeshActor>(SpawnClass, ToClone->GetActorLocation() + FVector(0,0,512) ,ToClone->GetActorRotation(), SpawnInfo );
	
	if(!NewSMA) return NULL;
	
	//Copy Transform
	NewSMA->SetActorTransform(ToClone->GetTransform());
	
	//Mobility
	NewSMA->StaticMeshComponent->SetMobility(EComponentMobility::Movable	);
	
	//copy static mesh
	NewSMA->StaticMeshComponent->SetStaticMesh(ToClone->StaticMeshComponent->StaticMesh);
	
	//~~~
	
	//copy materials
	TArray<UMaterialInterface*> Mats;
	ToClone->StaticMeshComponent->GetUsedMaterials(Mats);
	
	const int32 Total = Mats.Num();
	for(int32 v = 0; v < Total; v++ )
	{
		NewSMA->StaticMeshComponent->SetMaterial(v,Mats[v]);
	}
	
	//~~~
	
	//copy physics state
	if(ToClone->StaticMeshComponent->IsSimulatingPhysics())
	{
		NewSMA->StaticMeshComponent->SetSimulatePhysics(true);
	}
	
	//~~~
	
	//Add Location Offset
	const FVector SpawnLoc = ToClone->GetActorLocation() + LocationOffset;
	NewSMA->SetActorLocation(SpawnLoc);
	
	//Add Rotation offset
	FTransform TheTransform = NewSMA->GetTransform();
	TheTransform.ConcatenateRotation(RotationOffset.Quaternion());
	TheTransform.NormalizeRotation();
	
	//Set Transform
	NewSMA->SetActorTransform(TheTransform);
	
	IsValid = true;
	return NewSMA;
}

bool UVictoryBPFunctionLibrary::Actor__TeleportToActor(AActor* ActorToTeleport, AActor* DestinationActor)
{
	if(!ActorToTeleport) 							return false;
	if(!ActorToTeleport->IsValidLowLevel()) 	return false;
	if(!DestinationActor) 							return false;
	if(!DestinationActor->IsValidLowLevel()) 	return false;
	
	//Set Loc
	ActorToTeleport->SetActorLocation(DestinationActor->GetActorLocation());
	
	//Set Rot
	ActorToTeleport->SetActorRotation(DestinationActor->GetActorRotation());
	
	return true;
}

bool UVictoryBPFunctionLibrary::WorldType__InEditorWorld(UObject* WorldContextObject)
{
	if(!WorldContextObject) return false;
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return false;
	//~~~~~~~~~~~
	
    return (World->WorldType == EWorldType::Editor );
}

bool UVictoryBPFunctionLibrary::WorldType__InPIEWorld(UObject* WorldContextObject)
{
	if(!WorldContextObject) return false;
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return false;
	//~~~~~~~~~~~
	
    return (World->WorldType == EWorldType::PIE );
}
bool UVictoryBPFunctionLibrary::WorldType__InGameInstanceWorld(UObject* WorldContextObject)
{
	if(!WorldContextObject) return false;
	
	//using a context object to get the world!
    UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return false;
	//~~~~~~~~~~~
	
    return (World->WorldType == EWorldType::Game );
}
	
FString UVictoryBPFunctionLibrary::Accessor__GetNameAsString(const UObject* TheObject)
{
	if (!TheObject) return "";
	return TheObject->GetName();
}

FRotator UVictoryBPFunctionLibrary::Conversions__VectorToRotator(const FVector& TheVector)
{
	return TheVector.Rotation();
}
FVector UVictoryBPFunctionLibrary::Conversions__RotatorToVector(const FRotator& TheRotator)
{
	return TheRotator.Vector();
}
FRotator UVictoryBPFunctionLibrary::Character__GetControllerRotation(AActor * TheCharacter)
{
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);

	if (!AsCharacter) return FRotator::ZeroRotator;
	
	return AsCharacter->GetControlRotation();
}

//Draw From Socket on Character's Mesh
void UVictoryBPFunctionLibrary::Draw__Thick3DLineFromCharacterSocket(AActor* TheCharacter,  const FVector& EndPoint, FName Socket, FLinearColor LineColor, float Thickness, float Duration)
{
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return;
	if (!AsCharacter->Mesh) return;
	//~~~~~~~~~~~~~~~~~~~~
	
	//Get World
	UWorld* TheWorld = AsCharacter->GetWorld();
	if (!TheWorld) return;
	//~~~~~~~~~~~~~~~~~
	
	const FVector SocketLocation = AsCharacter->Mesh->GetSocketLocation(Socket);
	DrawDebugLine(
		TheWorld, 
		SocketLocation, 
		EndPoint, 
		LineColor.ToFColor(true), 
		false, 
		Duration, 
		0, 
		Thickness
	);
	
}
/** Draw 3D Line of Chosen Thickness From Mesh Socket to Destination */
void UVictoryBPFunctionLibrary::Draw__Thick3DLineFromSocket(USkeletalMeshComponent* Mesh, const FVector& EndPoint, FName Socket, FLinearColor LineColor, float Thickness, float Duration)
{
	if (!Mesh) return;
	//~~~~~~~~~~~~~~
	
	//Get an actor to GetWorld() from
	TObjectIterator<APlayerController> Itr;
	if (!Itr) return;
	//~~~~~~~~~~~~
	
	//Get World
	UWorld* TheWorld = Itr->GetWorld();
	if (!TheWorld) return;
	//~~~~~~~~~~~~~~~~~
	
	const FVector SocketLocation = Mesh->GetSocketLocation(Socket);
	
	DrawDebugLine(
		TheWorld, 
		SocketLocation, 
		EndPoint, 
		LineColor.ToFColor(true),
		false, 
		Duration, 
		0, 
		Thickness
	);
}
/** Draw 3D Line of Chosen Thickness Between Two Actors */
void UVictoryBPFunctionLibrary::Draw__Thick3DLineBetweenActors(AActor * StartActor, AActor * EndActor, FLinearColor LineColor, float Thickness, float Duration)
{
	if (!StartActor) return;
	if (!EndActor) return;
	//~~~~~~~~~~~~~~~~
	
	DrawDebugLine(
		StartActor->GetWorld(), 
		StartActor->GetActorLocation(), 
		EndActor->GetActorLocation(), 
		LineColor.ToFColor(true),
		false, 
		Duration, 
		0, 
		Thickness
	);
}
	
bool UVictoryBPFunctionLibrary::Animation__GetAimOffsets(AActor* AnimBPOwner, float& Pitch, float& Yaw)
{
	//Get Owning Character
	ACharacter * TheCharacter = Cast<ACharacter>(AnimBPOwner);
	
	if (!TheCharacter) return false;
	//~~~~~~~~~~~~~~~
	
	//Get Owning Controller Rotation
	const FRotator TheCtrlRotation = TheCharacter->GetControlRotation();
	
	const FVector RotationDir = TheCtrlRotation.Vector();
	
	//Inverse of ActorToWorld matrix is Actor to Local Space
		//so this takes the direction vector, the PC or AI rotation
		//and outputs what this dir is 
		//in local actor space &
		
		//local actor space is what we want for aiming offsets
		
	const FVector LocalDir = TheCharacter->ActorToWorld().InverseTransformVectorNoScale(RotationDir);
	const FRotator LocalRotation = LocalDir.Rotation();
		
	//Pass out Yaw and Pitch
	Yaw = LocalRotation.Yaw;
	Pitch = LocalRotation.Pitch;
	
	return true;
}
bool UVictoryBPFunctionLibrary::Animation__GetAimOffsetsFromRotation(AActor * AnimBPOwner, const FRotator & TheRotation, float & Pitch, float & Yaw)
{
	//Get Owning Character
	ACharacter * TheCharacter = Cast<ACharacter>(AnimBPOwner);
	
	if (!TheCharacter) return false;
	//~~~~~~~~~~~~~~~
	
	//using supplied rotation
	const FVector RotationDir = TheRotation.Vector();
	
	//Inverse of ActorToWorld matrix is Actor to Local Space
		//so this takes the direction vector, the PC or AI rotation
		//and outputs what this dir is 
		//in local actor space &
		
		//local actor space is what we want for aiming offsets
		
	const FVector LocalDir = TheCharacter->ActorToWorld().InverseTransformVectorNoScale(RotationDir);
	const FRotator LocalRotation = LocalDir.Rotation();
		
	//Pass out Yaw and Pitch
	Yaw = LocalRotation.Yaw;
	Pitch = LocalRotation.Pitch;
	
	return true;
}

void UVictoryBPFunctionLibrary::Visibility__GetRenderedActors(UObject* WorldContextObject, TArray<AActor*>& CurrentlyRenderedActors, float MinRecentTime)
{
	if(!WorldContextObject) return;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	
	//Empty any previous entries
	CurrentlyRenderedActors.Empty();
	
	//Iterate Over Actors
	for ( TActorIterator<AActor> Itr(World); Itr; ++Itr )
	{
		if (World->GetTimeSeconds() - Itr->GetLastRenderTime() <= MinRecentTime)
		{
			CurrentlyRenderedActors.Add( * Itr);
		}
	} 
} 
void UVictoryBPFunctionLibrary::Visibility__GetNotRenderedActors(UObject* WorldContextObject, TArray<AActor*>& CurrentlyNotRenderedActors, float MinRecentTime)
{
	if(!WorldContextObject) return;
	 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(!World) return;
	//~~~~~~~~~~~
	
	//Empty any previous entries
	CurrentlyNotRenderedActors.Empty();
	
	//Iterate Over Actors
	for ( TActorIterator<AActor> Itr(World); Itr; ++Itr )
	{
		if (World->GetTimeSeconds() - Itr->GetLastRenderTime() > MinRecentTime)
		{
			CurrentlyNotRenderedActors.Add( * Itr);
		}
	}
}

void UVictoryBPFunctionLibrary::Rendering__FreezeGameRendering()
{
	FViewport::SetGameRenderingEnabled(false);
}
void UVictoryBPFunctionLibrary::Rendering__UnFreezeGameRendering()
{
	FViewport::SetGameRenderingEnabled(true);
}
	
bool UVictoryBPFunctionLibrary::ClientWindow__GameWindowIsForeGroundInOS()
{
	//Iterate Over Actors
	UWorld* TheWorld = NULL;
	for ( TObjectIterator<AActor> Itr; Itr; ++Itr )
	{
		TheWorld = Itr->GetWorld();
		if (TheWorld) break;
		//~~~~~~~~~~~~~~~~~~~~~~~
	}
	//Get Player
	ULocalPlayer* VictoryPlayer = 
            TheWorld->GetFirstLocalPlayerFromController(); 

	if (!VictoryPlayer) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	//get view port ptr
	UGameViewportClient * VictoryViewportClient = 
		Cast < UGameViewportClient > (VictoryPlayer->ViewportClient);
		
	if (!VictoryViewportClient) return false;
	//~~~~~~~~~~~~~~~~~~~~
	 
	FViewport * VictoryViewport = VictoryViewportClient->Viewport;
	
	if (!VictoryViewport) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
    return VictoryViewport->IsForegroundWindow();
}
bool UVictoryBPFunctionLibrary::FileIO__SaveStringTextToFile(
	FString SaveDirectory, 
	FString JoyfulFileName, 
	FString SaveText,
	bool AllowOverWriting
){
	//Dir Exists?
	if ( !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *SaveDirectory))
	{
		//create directory if it not exist
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectory( *SaveDirectory);
		
		//still could not make directory?
		if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *SaveDirectory))
		{
			//Could not make the specified directory
			return false;
			//~~~~~~~~~~~~~~~~~~~~~~
		}
	}
	
	//get complete file path
	SaveDirectory += "\\";
	SaveDirectory += JoyfulFileName;
	
	//No over-writing?
	if (!AllowOverWriting)
	{
		//Check if file exists already
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists( * SaveDirectory))
		{
			//no overwriting
			return false;
		}
	}
	
	return FFileHelper::SaveStringToFile(SaveText, * SaveDirectory);
}
bool UVictoryBPFunctionLibrary::FileIO__SaveStringArrayToFile(FString SaveDirectory, FString JoyfulFileName, TArray<FString> SaveText, bool AllowOverWriting)  
{
	//Dir Exists?
	if ( !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *SaveDirectory))
	{
		//create directory if it not exist
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectory( *SaveDirectory);
		
		//still could not make directory?
		if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *SaveDirectory))
		{
			//Could not make the specified directory
			return false;
			//~~~~~~~~~~~~~~~~~~~~~~
		}
	}
	
	//get complete file path
	SaveDirectory += "\\";
	SaveDirectory += JoyfulFileName;
	
	//No over-writing?
	if (!AllowOverWriting)
	{
		//Check if file exists already
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists( * SaveDirectory))
		{
			//no overwriting
			return false;
		}
	}
	 
	FString FinalStr = "";
	for(FString& Each : SaveText)
	{
		FinalStr += Each;
		FinalStr += LINE_TERMINATOR;
	}
	return FFileHelper::SaveStringToFile(FinalStr, * SaveDirectory);
}
float UVictoryBPFunctionLibrary::Calcs__ClosestPointToSourcePoint(const FVector & Source, const TArray<FVector>& OtherPoints, FVector& ClosestPoint)
{
	float CurDist = 0;
	float ClosestDistance = -1;
	int32 ClosestVibe = 0;
	ClosestPoint = FVector::ZeroVector;
	
	if (OtherPoints.Num() <= 0) return ClosestDistance;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	for (int32 Itr = 0; Itr < OtherPoints.Num(); Itr++)
	{
		if (Source == OtherPoints[Itr]) continue;
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		
		//Dist
		CurDist = FVector::Dist(Source, OtherPoints[Itr]);
		
		//Min
		if (ClosestDistance < 0 || ClosestDistance >= CurDist)
		{
			ClosestVibe = Itr;
			ClosestDistance = CurDist;
		}
	}
	
	//Out
	ClosestPoint = OtherPoints[ClosestVibe];
	return ClosestDistance;
}

bool UVictoryBPFunctionLibrary::Data__GetCharacterBoneLocations(AActor * TheCharacter, TArray<FVector>& BoneLocations)
{
	ACharacter * Source = Cast<ACharacter>(TheCharacter);
	if (!Source) return false;
	
	if (!Source-> Mesh) return false;
	//~~~~~~~~~~~~~~~~~~~~~~~~~
	TArray<FName> BoneNames;
	
	BoneLocations.Empty();
	
	
	//Get Bone Names
	Source-> Mesh->GetBoneNames(BoneNames);
	
	//Get Bone Locations
	for (int32 Itr = 0; Itr < BoneNames.Num(); Itr++ )
	{
		BoneLocations.Add(Source->Mesh->GetBoneLocation(BoneNames[Itr]));
	}
	
	return true;
}

USkeletalMeshComponent* UVictoryBPFunctionLibrary::Accessor__GetCharacterSkeletalMesh(AActor * TheCharacter, bool& IsValid)
{
	IsValid = false;
	
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return NULL;
	//~~~~~~~~~~~~~~~~~
	
	//Is Valid?
	if (AsCharacter->Mesh)
		if (AsCharacter->Mesh->IsValidLowLevel() ) 
			IsValid = true;
			
	return AsCharacter->Mesh;
}

bool UVictoryBPFunctionLibrary::TraceData__GetTraceDataFromCharacterSocket(
	FVector & TraceStart, //out
	FVector & TraceEnd,	//out
	AActor * TheCharacter,
	const FRotator& TraceRotation, 
	float TraceLength,
	FName Socket, 
	bool DrawTraceData, 
	FLinearColor TraceDataColor, 
	float TraceDataThickness
) {
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	//Mesh Exists?
	if (!AsCharacter->Mesh) return false;
	
	//Socket Exists?
	if (!AsCharacter->Mesh->DoesSocketExist(Socket)) return false;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	TraceStart = AsCharacter->Mesh->GetSocketLocation(Socket);
	TraceEnd = TraceStart + TraceRotation.Vector() * TraceLength;
	
	if (DrawTraceData) 
	{
		//Get World
		UWorld* TheWorld = AsCharacter->GetWorld();
		if (!TheWorld) return false;
		//~~~~~~~~~~~~~~~~~
	
		DrawDebugLine(
			TheWorld, 
			TraceStart, 
			TraceEnd, 
			TraceDataColor.ToFColor(true),
			false, 
			0.0333, 
			0, 
			TraceDataThickness
		);
	}
	
	return true;
}
bool UVictoryBPFunctionLibrary::TraceData__GetTraceDataFromSkeletalMeshSocket(
	FVector & TraceStart, //out
	FVector & TraceEnd,	//out
	USkeletalMeshComponent * Mesh,
	const FRotator & TraceRotation,	
	float TraceLength,
	FName Socket, 
	bool DrawTraceData, 
	FLinearColor TraceDataColor, 
	float TraceDataThickness
) {
	//Mesh Exists?
	if (!Mesh) return false;
	
	//Socket Exists?
	if (!Mesh->DoesSocketExist(Socket)) return false;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	TraceStart = Mesh->GetSocketLocation(Socket);
	TraceEnd = TraceStart + TraceRotation.Vector() * TraceLength;
	
	if (DrawTraceData) 
	{
		//Get a PC to GetWorld() from
		TObjectIterator<APlayerController> Itr;
		if (!Itr) return false;
		
		//~~~~~~~~~~~~
		
		//Get World
		UWorld* TheWorld = Itr->GetWorld();
		if (!TheWorld) return false;
		//~~~~~~~~~~~~~~~~~
	
		DrawDebugLine(
			TheWorld, 
			TraceStart, 
			TraceEnd, 
			TraceDataColor.ToFColor(true),
			false, 
			0.0333, 
			0, 
			TraceDataThickness
		);
	}
	
	return true;
}
AActor*  UVictoryBPFunctionLibrary::Traces__CharacterMeshTrace___ClosestBone(
	AActor* TraceOwner,
	const FVector & TraceStart, 
	const FVector & TraceEnd, 
	FVector & OutImpactPoint, 
	FVector & OutImpactNormal, 
	FName & ClosestBoneName,
	FVector & ClosestBoneLocation,
	bool& IsValid
)
{
	IsValid = false;
	AActor * HitActor = NULL;
	//~~~~~~~~~~~~~~~~~~~~~~
	
	//Get a PC to GetWorld() from
	TObjectIterator<APlayerController> Itr;
	if (!Itr) return NULL;
	
	//~~~~~~~~~~~~
	
	//Get World
	UWorld* TheWorld = Itr->GetWorld();
	if (TheWorld == nullptr) return NULL;
	//~~~~~~~~~~~~~~~~~
	
	
	//Simple Trace First
	FCollisionQueryParams TraceParams(FName(TEXT("VictoryBPTrace::CharacterMeshTrace")), true, HitActor);
	TraceParams.bTraceComplex = true;
	TraceParams.bTraceAsyncScene = true;
	TraceParams.bReturnPhysicalMaterial = false;
	TraceParams.AddIgnoredActor(TraceOwner);
	
	//initialize hit info
	FHitResult RV_Hit(ForceInit);
	
	TheWorld->LineTraceSingleByChannel(
		RV_Hit,		//result
		TraceStart, 
		TraceEnd, 
		ECC_Pawn, //collision channel
		TraceParams
	);
		
	//Hit Something!
	if (!RV_Hit.bBlockingHit) return HitActor;
	
	
	//Character?
	HitActor = RV_Hit.GetActor();
	ACharacter * AsCharacter = Cast<ACharacter>(HitActor);
	if (!AsCharacter) return HitActor;
	
	//Mesh
	if (!AsCharacter->Mesh) return HitActor;
	
	//Component Trace
	IsValid = AsCharacter->Mesh->K2_LineTraceComponent(
		TraceStart, 
		TraceEnd, 
		true, 
		false, 
		OutImpactPoint, 
		OutImpactNormal,
		ClosestBoneName
	); 
	
	//Location
	ClosestBoneLocation = AsCharacter->Mesh->GetBoneLocation(ClosestBoneName);
	
	return HitActor;
}

AActor* UVictoryBPFunctionLibrary::Traces__CharacterMeshTrace___ClosestSocket(
	const AActor * TraceOwner, 
	const FVector & TraceStart, 
	const FVector & TraceEnd, 
	FVector & OutImpactPoint, 
	FVector & OutImpactNormal, 
	FName & ClosestSocketName, 
	FVector & SocketLocation, 
	bool & IsValid
)
{
	IsValid = false;
	AActor * HitActor = NULL;
	//~~~~~~~~~~~~~~~~~~~~~~
	 
	//There may not be a trace owner so dont rely on it
	
	//Get a PC to GetWorld() from
	TObjectIterator<APlayerController> Itr;
	if (!Itr) return NULL;
	
	//~~~~~~~~~~~~
	
	//Get World
	UWorld* TheWorld = Itr->GetWorld();
	if (TheWorld == nullptr) return NULL;
	//~~~~~~~~~~~~~~~~~
	
	
	//Simple Trace First
	FCollisionQueryParams TraceParams(FName(TEXT("VictoryBPTrace::CharacterMeshSocketTrace")), true, HitActor);
	TraceParams.bTraceComplex = true;
	TraceParams.bTraceAsyncScene = true;
	TraceParams.bReturnPhysicalMaterial = false;
	TraceParams.AddIgnoredActor(TraceOwner);
	
	//initialize hit info
	FHitResult RV_Hit(ForceInit);
	
	TheWorld->LineTraceSingleByChannel(
		RV_Hit,		//result
		TraceStart, 
		TraceEnd, 
		ECC_Pawn, //collision channel
		TraceParams
	);
		
	//Hit Something!
	if (!RV_Hit.bBlockingHit) return HitActor;
	
	
	//Character?
	HitActor = RV_Hit.GetActor();
	ACharacter * AsCharacter = Cast<ACharacter>(HitActor);
	if (!AsCharacter) return HitActor;
	
	//Mesh
	if (!AsCharacter->Mesh) return HitActor;
	
	//Component Trace
	FName BoneName;
	if (! AsCharacter->Mesh->K2_LineTraceComponent(
		TraceStart, 
		TraceEnd, 
		true, 
		false, 
		OutImpactPoint, 
		OutImpactNormal,
		BoneName
	)) return HitActor;
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//					Socket Names
	TArray<FComponentSocketDescription> SocketNames;
	
	//Get Bone Names
	AsCharacter->Mesh->QuerySupportedSockets(SocketNames);
	
	//						Min
	FVector CurLoc;
	float CurDist = 0;
	float ClosestDistance = -1;
	int32 ClosestVibe = 0;
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//Check All Bones Locations
	for (int32 Itr = 0; Itr < SocketNames.Num(); Itr++ )
	{
		//Is this a Bone not a socket?
		if(SocketNames[Itr].Type == EComponentSocketType::Bone) continue;
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		
		CurLoc = AsCharacter->Mesh->GetSocketLocation(SocketNames[Itr].Name);
		
		//Dist
		CurDist = FVector::Dist(OutImpactPoint, CurLoc);
		
		//Min
		if (ClosestDistance < 0 || ClosestDistance >= CurDist)
		{
			ClosestVibe = Itr;
			ClosestDistance = CurDist;
		}
	}
	
	//Name
	ClosestSocketName = SocketNames[ClosestVibe].Name;
	
	//Location
	SocketLocation = AsCharacter->Mesh->GetSocketLocation(ClosestSocketName);
	
	//Valid
	IsValid = true;
	
	//Actor
	return HitActor;
}
	
//Most HUD stuff is in floats so I do the conversion internally
bool UVictoryBPFunctionLibrary::Viewport__SetMousePosition(const APlayerController* ThePC, const float& PosX, const float& PosY)
{
	if (!ThePC) return false;
	//~~~~~~~~~~~~~
	
	//Get Player
	const ULocalPlayer * VictoryPlayer = Cast<ULocalPlayer>(ThePC->Player); 
											//PlayerController::Player is UPlayer
           
	if (!VictoryPlayer) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	//get view port ptr
	const UGameViewportClient * VictoryViewportClient = 
		Cast < UGameViewportClient > (VictoryPlayer->ViewportClient);
		
	if (!VictoryViewportClient) return false;
	//~~~~~~~~~~~~~~~~~~~~
	 
	FViewport * VictoryViewport = VictoryViewportClient->Viewport;
	
	if (!VictoryViewport) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	//Set Mouse
	VictoryViewport->SetMouse(int32(PosX), int32(PosY));
	
	return true;
}

APlayerController * UVictoryBPFunctionLibrary::Accessor__GetPlayerController(
	AActor * TheCharacter, 
	bool & IsValid
)
{
	IsValid = false;
	
	//Cast to Character
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return NULL;
	
	//cast to PC
	APlayerController * ThePC = Cast < APlayerController > (AsCharacter->GetController());
	
	if (!ThePC ) return NULL;
	
	IsValid = true;
	return ThePC;
}
	
bool UVictoryBPFunctionLibrary::Viewport__GetCenterOfViewport(const APlayerController * ThePC, float & PosX, float & PosY)
{
	if (!ThePC) return false;
	//~~~~~~~~~~~~~
	
	//Get Player
	const ULocalPlayer * VictoryPlayer = Cast<ULocalPlayer>(ThePC->Player); 
											//PlayerController::Player is UPlayer
           
	if (!VictoryPlayer) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	//get view port ptr
	const UGameViewportClient * VictoryViewportClient = 
		Cast < UGameViewportClient > (VictoryPlayer->ViewportClient);
		
	if (!VictoryViewportClient) return false;
	//~~~~~~~~~~~~~~~~~~~~
	 
	FViewport * VictoryViewport = VictoryViewportClient->Viewport;
	
	if (!VictoryViewport) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	//Get Size
	FIntPoint Size = VictoryViewport->GetSizeXY();
	
	//Center
	PosX = Size.X / 2;
	PosY = Size.Y / 2;
	
	return true;
}

bool UVictoryBPFunctionLibrary::Viewport__GetMousePosition(const APlayerController * ThePC, float & PosX, float & PosY)
{
	if (!ThePC) return false;
	//~~~~~~~~~~~~~
	
	//Get Player
	const ULocalPlayer * VictoryPlayer = Cast<ULocalPlayer>(ThePC->Player); 
											//PlayerController::Player is UPlayer
           
	if (!VictoryPlayer) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	//get view port ptr
	const UGameViewportClient * VictoryViewportClient = 
		Cast < UGameViewportClient > (VictoryPlayer->ViewportClient);
		
	if (!VictoryViewportClient) return false;
	//~~~~~~~~~~~~~~~~~~~~
	 
	FViewport * VictoryViewport = VictoryViewportClient->Viewport;
	
	if (!VictoryViewport) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	PosX = float(VictoryViewport->GetMouseX());
	PosY = float(VictoryViewport->GetMouseY());
	
	return true;
}





bool UVictoryBPFunctionLibrary::Physics__EnterRagDoll(AActor * TheCharacter)
{
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	//Mesh?
	if (!AsCharacter->Mesh) return false;
	
	//Physics Asset?
	if(!AsCharacter->Mesh->GetPhysicsAsset()) return false;
	
	//Victory Ragdoll
	AsCharacter->Mesh->SetPhysicsBlendWeight(1);
	AsCharacter->Mesh->bBlendPhysics = true;
	
	return true;
}


bool UVictoryBPFunctionLibrary::Physics__LeaveRagDoll(
	AActor* TheCharacter,
	float HeightAboveRBMesh,
	const FVector& InitLocation, 
	const FRotator& InitRotation
){
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	//Mesh?
	if (!AsCharacter->Mesh) return false;
	
	//Set Actor Location to Be Near Ragdolled Mesh
	//Calc Ref Bone Relative Pos for use with IsRagdoll
	TArray<FName> BoneNames;
	AsCharacter->Mesh->GetBoneNames(BoneNames);
	if(BoneNames.Num() > 0)
	{
		AsCharacter->SetActorLocation(FVector(0,0,HeightAboveRBMesh) + AsCharacter->Mesh->GetBoneLocation(BoneNames[0]));
	}
	
	//Exit Ragdoll
	AsCharacter->Mesh->SetPhysicsBlendWeight(0); //1 for full physics

	//Restore Defaults
	AsCharacter->Mesh->SetRelativeRotation(InitRotation);
	AsCharacter->Mesh->SetRelativeLocation(InitLocation);
	
	//Set Falling After Final Capsule Relocation
	if(AsCharacter->CharacterMovement) AsCharacter->CharacterMovement->SetMovementMode(MOVE_Falling);	
	
	return true;
}	

bool UVictoryBPFunctionLibrary::Physics__IsRagDoll(AActor* TheCharacter)
{
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	//Mesh?
	if (!AsCharacter->Mesh) return false;
	
	return AsCharacter->Mesh->IsAnySimulatingPhysics();
}	

bool UVictoryBPFunctionLibrary::Physics__GetLocationofRagDoll(AActor* TheCharacter, FVector& RagdollLocation)
{
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	//Mesh?
	if (!AsCharacter->Mesh) return false;
	
	TArray<FName> BoneNames;
	AsCharacter->Mesh->GetBoneNames(BoneNames);
	if(BoneNames.Num() > 0)
	{
		RagdollLocation = AsCharacter->Mesh->GetBoneLocation(BoneNames[0]);
	}
	else return false;
	
	return true;
}

bool UVictoryBPFunctionLibrary::Physics__InitializeVictoryRagDoll(
	AActor* TheCharacter, 
	FVector& InitLocation, 
	FRotator& InitRotation
){
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	//Mesh?
	if (!AsCharacter->Mesh) return false;
	
	InitLocation = AsCharacter->Mesh->GetRelativeTransform().GetLocation();
	InitRotation = AsCharacter->Mesh->GetRelativeTransform().Rotator();
	
	return true;
}

bool UVictoryBPFunctionLibrary::Physics__UpdateCharacterCameraToRagdollLocation(
	AActor* TheCharacter, 
	float HeightOffset,
	float InterpSpeed
){
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	//Mesh?
	if (!AsCharacter->Mesh) return false;
	
	//Ragdoll?
	if(!AsCharacter->Mesh->IsAnySimulatingPhysics()) return false;
	
	FVector RagdollLocation = FVector(0,0,0);
	TArray<FName> BoneNames;
	AsCharacter->Mesh->GetBoneNames(BoneNames);
	if(BoneNames.Num() > 0)
	{
		RagdollLocation = AsCharacter->Mesh->GetBoneLocation(BoneNames[0]);
	}
	
	//Interp
	RagdollLocation = FMath::VInterpTo(AsCharacter->GetActorLocation(), RagdollLocation + FVector(0,0,HeightOffset), AsCharacter->GetWorld()->DeltaTimeSeconds, InterpSpeed);

	//Set Loc
	AsCharacter->SetActorLocation(RagdollLocation);
	
	return true;
}
/*
bool UVictoryBPFunctionLibrary::Accessor__GetSocketLocalTransform(const USkeletalMeshComponent* Mesh, FTransform& LocalTransform, FName SocketName)
{
	if(!Mesh) return false;
	
	LocalTransform =  Mesh->GetSocketLocalTransform(SocketName);
	
	return true;
}
*/
 
void UVictoryBPFunctionLibrary::StringConversion__GetFloatAsStringWithPrecision(float TheFloat, FString & FloatString, int32 Precision, bool IncludeLeadingZero)
{ 
	FNumberFormattingOptions NumberFormat;					//Text.h
	NumberFormat.MinimumIntegralDigits = (IncludeLeadingZero) ? 1 : 0;
	NumberFormat.MaximumIntegralDigits = 10000;
	NumberFormat.MinimumFractionalDigits = Precision;
	NumberFormat.MaximumFractionalDigits = Precision; 
	FloatString = FText::AsNumber(TheFloat, &NumberFormat).ToString();
}

bool UVictoryBPFunctionLibrary::LensFlare__GetLensFlareOffsets(
	APlayerController* PlayerController,
	AActor* LightSource, 
	float& PitchOffset, 
	float& YawOffset, 
	float& RollOffset
){
	if(!PlayerController) return false;
	if(!LightSource) return false;
	//~~~~~~~~~~~~~~~~~~~
	
	//angle from player to light source
	const FRotator AngleToLightSource = (LightSource->GetActorLocation() - PlayerController->GetFocalLocation()).Rotation();
	
	const FRotator Offsets = AngleToLightSource - PlayerController->GetControlRotation();
	
	PitchOffset = Offsets.Pitch;
	YawOffset = Offsets.Yaw;
	RollOffset = Offsets.Roll;
	return true;
}


bool UVictoryBPFunctionLibrary::AnimatedVertex__GetAnimatedVertexLocations(
	USkeletalMeshComponent* Mesh, 
	TArray<FVector>& Locations
){
	if(!Mesh) return false;
	if(!Mesh->SkeletalMesh) return false;
	//~~~~~~~~~
	 
	//~~~~~~~~~~~~~
	Locations.Empty(); 
	//~~~~~~~~~~~~~
	 
	Mesh->ComputeSkinnedPositions(Locations);
	
	FTransform ToWorld = Mesh->GetComponentTransform();
	FVector WorldLocation = ToWorld.GetLocation();
	
	for(FVector& Each : Locations)
	{
		Each = WorldLocation + ToWorld.TransformVector(Each);
	} 
	
	return true;
}
	
/*
bool UVictoryBPFunctionLibrary::AnimatedVertex__GetAnimatedVertexLocationsAndNormals(
	USkeletalMeshComponent* Mesh, 
	TArray<FVector>& Locations, 
	TArray<FVector>& Normals 
)
{
	if(!Mesh) return false;
	if(!Mesh->SkeletalMesh) return false;
	//~~~~~~~~~
	
	Locations.Empty(); 
	Normals.Empty();
	//~~~~~~~~~~~~~~~~~~~
	
	//	Get the Verticies For Each Bone, Most Influenced by That Bone!
	//					Vertices are in Bone space.
	TArray<FBoneVertInfo> BoneVertexInfos;
	FSkeletalMeshTools::CalcBoneVertInfos(Mesh->SkeletalMesh,BoneVertexInfos,true); //true = only dominant influence
	
	//~~~~~~~~~~~~~~~~~~~~~
	int32 VertItr = 0;
	FBoneVertInfo* EachBoneVertInfo;
	FVector BoneWorldPos;
	int32 NumOfVerticies;
	FTransform RV_Transform;
	FVector RV_Vect;
	for(int32 Itr=0; Itr < BoneVertexInfos.Num() ; Itr++)
	{
		EachBoneVertInfo = &BoneVertexInfos[Itr];
		//~~~~~~~~~~~~~~~~~~~~~~~~
		
		//Bone Transform To World Space, and Location
		RV_Transform = Mesh->GetBoneTransform(Itr);
		BoneWorldPos = RV_Transform.GetLocation();
		
		//How many verts is this bone influencing?
		NumOfVerticies = EachBoneVertInfo->Positions.Num();
		for(VertItr=0; VertItr < NumOfVerticies ; VertItr++)
		{
			//Animated Vertex Location!
			Locations.Add(  BoneWorldPos + RV_Transform.TransformVector(EachBoneVertInfo->Positions[VertItr])  );
		
			//Animated Vertex Normal for rotating the emitter!!!!!
			Normals.Add(  RV_Transform.TransformVector(EachBoneVertInfo->Normals[VertItr])  );
		}
	}
	
	//~~~ Cleanup ~~~
	BoneVertexInfos.Empty();
	
	return true;
}
	
bool UVictoryBPFunctionLibrary::AnimatedVertex__DrawAnimatedVertexLocations(
	UObject* WorldContextObject,
	USkeletalMeshComponent* Mesh, 
	float ChanceToSkipAVertex, 
	bool DrawNormals
)
{
	UWorld* const TheWorld = GEngine->GetWorldFromContextObject(WorldContextObject);
	
	if(!TheWorld) return false;
	if(!Mesh) return false;
	if(!Mesh->SkeletalMesh) return false;
	//~~~~~~~~~
	
	//	Get the Verticies For Each Bone, Most Influenced by That Bone!
	//					Vertices are in Bone space.
	TArray<FBoneVertInfo> BoneVertexInfos;
	FSkeletalMeshTools::CalcBoneVertInfos(Mesh->SkeletalMesh,BoneVertexInfos,true); //true = only dominant influence
	
	//~~~~~~~~~~~~~~~~~~~~~
	int32 VertItr = 0;
	FBoneVertInfo* EachBoneVertInfo;
	FVector BoneWorldPos;
	int32 NumOfVerticies;
	FTransform RV_Transform;
	FVector RV_Vect;
	
	const FColor HappyRed = FColor(255,0,0);
	const FColor HappyBlue = FColor(0,0,255);
	for(int32 Itr=0; Itr < BoneVertexInfos.Num() ; Itr++)
	{
		EachBoneVertInfo = &BoneVertexInfos[Itr];
		//~~~~~~~~~~~~~~~~~~~~~~~~
		
		//Bone Transform To World Space, and Location
		RV_Transform = Mesh->GetBoneTransform(Itr);
		BoneWorldPos = RV_Transform.GetLocation();
		
		//How many verts is this bone influencing?
		NumOfVerticies = EachBoneVertInfo->Positions.Num();
		for(VertItr=0; VertItr < NumOfVerticies ; VertItr++)
		{
			if(FMath::FRandRange(0, 1) < ChanceToSkipAVertex) continue;
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			
			RV_Vect = BoneWorldPos + RV_Transform.TransformVector(EachBoneVertInfo->Positions[VertItr]);
			
			DrawDebugPoint(
				TheWorld, 
				RV_Vect,
				12, 
				HappyRed, 
				false, 
				0.03
			);
			
			if(DrawNormals)
			{
			DrawDebugLine(
				TheWorld, 
				RV_Vect, 
				RV_Vect + RV_Transform.TransformVector(EachBoneVertInfo->Normals[VertItr] * 64),  
				HappyBlue, 
				false, 
				0.03, 
				0, 
				1
			);
			}
		}
	}
	
	//~~~ Cleanup ~~~
	BoneVertexInfos.Empty();
	
	return true;
}
	
bool UVictoryBPFunctionLibrary::AnimatedVertex__GetCharacterAnimatedVertexLocations(
	AActor* TheCharacter, 
	TArray<FVector>& Locations
)
{
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	USkeletalMeshComponent* Mesh = AsCharacter->Mesh;
	if (!Mesh) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	AnimatedVertex__GetAnimatedVertexLocations(Mesh,Locations);
	
	return true;
}
	
bool UVictoryBPFunctionLibrary::AnimatedVertex__GetCharacterAnimatedVertexLocationsAndNormals(
	AActor* TheCharacter, 
	TArray<FVector>& Locations, 
	TArray<FVector>& Normals 
)
{
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	USkeletalMeshComponent* Mesh = AsCharacter->Mesh;
	if (!Mesh) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	AnimatedVertex__GetAnimatedVertexLocationsAndNormals(Mesh,Locations,Normals);
	
	return true;
}
	
bool UVictoryBPFunctionLibrary::AnimatedVertex__DrawCharacterAnimatedVertexLocations(
	AActor* TheCharacter, 
	float ChanceToSkipAVertex, 
	bool DrawNormals
)
{	
	ACharacter * AsCharacter = Cast<ACharacter>(TheCharacter);
	if (!AsCharacter) return false;
	
	USkeletalMeshComponent* Mesh = AsCharacter->Mesh;
	if (!Mesh) return false;
	//~~~~~~~~~~~~~~~~~~~~
	
	AnimatedVertex__DrawAnimatedVertexLocations(TheCharacter,Mesh,ChanceToSkipAVertex,DrawNormals);
	
	return true;
}
*/

//SMA Version
float UVictoryBPFunctionLibrary::DistanceToSurface__DistaceOfPointToMeshSurface(AStaticMeshActor* TheSMA, const FVector& TestPoint, FVector& ClosestSurfacePoint)
{
	if(!TheSMA) return -1;
	if(!TheSMA->StaticMeshComponent) return -1;
	//~~~~~~~~~~
	
	//Dist of pt to Surface, retrieve closest Surface Point to Actor
	return TheSMA->StaticMeshComponent->GetDistanceToCollision(TestPoint, ClosestSurfacePoint);
}

bool UVictoryBPFunctionLibrary::Mobility__SetSceneCompMobility(
	USceneComponent* SceneComp,
	EComponentMobility::Type NewMobility
)
{
	if(!SceneComp) return false;
	//~~~~~~~~~~~
	
	SceneComp->SetMobility(NewMobility);
	
	return true;
}



















//~~~~~~~~~~~~~~~~~~
//		FullScreen
//~~~~~~~~~~~~~~~~~~	
TEnumAsByte<EJoyGraphicsFullScreen::Type> UVictoryBPFunctionLibrary::JoyGraphicsSettings__FullScreen_Get()
{
	return TEnumAsByte<EJoyGraphicsFullScreen::Type>(JoyGraphics_FullScreen_GetFullScreenType());
}
	  
void UVictoryBPFunctionLibrary::JoyGraphicsSettings__FullScreen_Set(TEnumAsByte<EJoyGraphicsFullScreen::Type> NewSetting)
{  
	JoyGraphics_FullScreen_SetFullScreenType(NewSetting.GetValue()); 
}














//~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~
//			  Contributed by Others

	/**
	* Contributed by: SaxonRah
	* Better random numbers. Seeded with a random device. if the random device's entropy is 0; defaults to current time for seed.
	* can override with seed functions;
	*/
//----------------------------------------------------------------------------------------------BeginRANDOM
	std::random_device rd;		
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

	std::mt19937 rand_MT;
	std::default_random_engine rand_DRE;

	/** Construct a random device and set seed for engines dependent on entropy */
	void UVictoryBPFunctionLibrary::constructRand()
	{
		seed = std::chrono::system_clock::now().time_since_epoch().count();

		if (rd.entropy() == 0)
		{
			seedRand(seed);
		}else{
			seedRand(rd());
		}
	}
	/** Set seed for Rand */
	void UVictoryBPFunctionLibrary::seedRand(int32 _seed)
	{
		seed = _seed;
	}

	/** Set seed with time for Rand */
	void UVictoryBPFunctionLibrary::seedRandWithTime()
	{
		seed = std::chrono::system_clock::now().time_since_epoch().count();
	}

	/** Set seed with entropy for Rand */
	void UVictoryBPFunctionLibrary::seedRandWithEntropy()
	{
		seedRand(rd());
	}

	/** Random Bool - Bernoulli distribution */
	bool UVictoryBPFunctionLibrary::RandBool_Bernoulli(float fBias)
	{
		std::bernoulli_distribution dis(fBias);
		return dis(rand_DRE);
	}

	/** Random Integer - Uniform distribution */
	int32 UVictoryBPFunctionLibrary::RandInt_uniDis()
	{
		std::uniform_int_distribution<int32> dis(0, 1);
		return dis(rand_DRE);
	}
	/** Random Integer - Uniform distribution */
	int32 UVictoryBPFunctionLibrary::RandInt_MINMAX_uniDis(int32 iMin, int32 iMax)
	{
		std::uniform_int_distribution<int32> dis(iMin, iMax);
		return dis(rand_DRE);
	}

	/** Random Float - Zero to One Uniform distribution */
	float UVictoryBPFunctionLibrary::RandFloat_uniDis()
	{
		std::uniform_real_distribution<float> dis(0, 1);
		return dis(rand_DRE);
	}
	/** Random Float - MIN to MAX Uniform distribution */
	float UVictoryBPFunctionLibrary::RandFloat_MINMAX_uniDis(float iMin, float iMax)
	{
		std::uniform_real_distribution<float> dis(iMin, iMax);
		return dis(rand_DRE);
	}

	/** Random Bool - Bernoulli distribution  -  Mersenne Twister */
	bool UVictoryBPFunctionLibrary::RandBool_Bernoulli_MT(float fBias)
	{
		std::bernoulli_distribution dis(fBias);
		return dis(rand_MT);
	}

	/** Random Integer - Uniform distribution  -  Mersenne Twister */
	int32 UVictoryBPFunctionLibrary::RandInt_uniDis_MT()
	{
		std::uniform_int_distribution<int32> dis(0, 1);
		return dis(rand_MT);
	}
	/** Random Integer - Uniform distribution  -  Mersenne Twister */
	int32 UVictoryBPFunctionLibrary::RandInt_MINMAX_uniDis_MT(int32 iMin, int32 iMax)
	{
		std::uniform_int_distribution<int32> dis(iMin, iMax);
		return dis(rand_MT);
	}

	/** Random Float - Zero to One Uniform distribution  -  Mersenne Twister */
	float UVictoryBPFunctionLibrary::RandFloat_uniDis_MT()
	{
		std::uniform_real_distribution<float> dis(0, 1);
		return dis(rand_MT);
	}
	/** Random Float - MIN to MAX Uniform distribution  -  Mersenne Twister */
	float UVictoryBPFunctionLibrary::RandFloat_MINMAX_uniDis_MT(float iMin, float iMax)
	{
		std::uniform_real_distribution<float> dis(iMin, iMax);
		return dis(rand_MT);
	}
//----------------------------------------------------------------------------------------------ENDRANDOM



void UVictoryBPFunctionLibrary::String__ExplodeString(TArray<FString>& OutputStrings, FString InputString, FString Separator, int32 limit, bool bTrimElements)
{
	OutputStrings.Empty();
	//~~~~~~~~~~~
	
	if (InputString.Len() > 0 && Separator.Len() > 0) {
		int32 StringIndex = 0;
		int32 SeparatorIndex = 0;

		FString Section = "";
		FString Extra = "";

		int32 PartialMatchStart = -1;

		while (StringIndex < InputString.Len()) {

			if (InputString[StringIndex] == Separator[SeparatorIndex]) {
				if (SeparatorIndex == 0) {
					//A new partial match has started.
					PartialMatchStart = StringIndex;
				}
				Extra.AppendChar(InputString[StringIndex]);
				if (SeparatorIndex == (Separator.Len() - 1)) {
					//We have matched the entire separator.
					SeparatorIndex = 0;
					PartialMatchStart = -1;
					if (bTrimElements == true) {
						OutputStrings.Add(FString(Section).Trim().TrimTrailing());
					}
					else {
						OutputStrings.Add(FString(Section));
					}

					//if we have reached the limit, stop.
					if (limit > 0 && OutputStrings.Num() >= limit) 
					{
						return;
						//~~~~
					}

					Extra.Empty();
					Section.Empty();
				}
				else {
					++SeparatorIndex;
				}
			}
			else {
				//Not matched.
				//We should revert back to PartialMatchStart+1 (if there was a partial match) and clear away extra.
				if (PartialMatchStart >= 0) {
					StringIndex = PartialMatchStart;
					PartialMatchStart = -1;
					Extra.Empty();
					SeparatorIndex = 0;
				}
				Section.AppendChar(InputString[StringIndex]);
			}

			++StringIndex;
		}

		//If there is anything left in Section or Extra. They should be added as a new entry.
		if (bTrimElements == true) {
			OutputStrings.Add(FString(Section + Extra).Trim().TrimTrailing());
		}
		else {
			OutputStrings.Add(FString(Section + Extra));
		}

		Section.Empty();
		Extra.Empty();
	}
}

UTexture2D* UVictoryBPFunctionLibrary::LoadTexture2D_FromDDSFile(const FString& FullFilePath)
{
	UTexture2D* Texture = NULL;

	FString TexturePath = FullFilePath;//FPaths::GameContentDir( ) + TEXT( "../Data/" ) + TextureFilename;
	TArray<uint8> FileData;

	/* Load DDS texture */
	if( FFileHelper::LoadFileToArray( FileData, *TexturePath, 0 ) )
	{
		FDDSLoadHelper DDSLoadHelper( &FileData[ 0 ], FileData.Num( ) );
		if( DDSLoadHelper.IsValid2DTexture( ) )
		{
			int32 NumMips = DDSLoadHelper.ComputeMipMapCount( );
			EPixelFormat Format = DDSLoadHelper.ComputePixelFormat( );
			int32 BlockSize = 16;

			if( NumMips == 0 )
			{
				NumMips = 1;
			}

			if( Format == PF_DXT1 )
			{
				BlockSize = 8;
			}

			/* Create transient texture */
			Texture = UTexture2D::CreateTransient( DDSLoadHelper.DDSHeader->dwWidth, DDSLoadHelper.DDSHeader->dwHeight, Format );
			if(!Texture) return NULL;
			Texture->PlatformData->NumSlices = 1;
			Texture->NeverStream = true;
		
			/* Get pointer to actual data */
			uint8* DataPtr = (uint8*) DDSLoadHelper.GetDDSDataPointer( );

			uint32 CurrentWidth = DDSLoadHelper.DDSHeader->dwWidth;
			uint32 CurrentHeight = DDSLoadHelper.DDSHeader->dwHeight;

			/* Iterate through mips */
			for( int32 i = 0; i < NumMips; i++ )
			{
				/* Lock to 1x1 as smallest size */
				CurrentWidth = ( CurrentWidth < 1 ) ? 1 : CurrentWidth;
				CurrentHeight = ( CurrentHeight < 1 ) ? 1 : CurrentHeight;

				/* Get number of bytes to read */
				int32 NumBytes = CurrentWidth * CurrentHeight * 4;
				if( Format == PF_DXT1 || Format == PF_DXT3 || Format == PF_DXT5 )
				{
					/* Compressed formats */
					NumBytes = ( ( CurrentWidth + 3 ) / 4 ) * ( ( CurrentHeight + 3 ) / 4 ) * BlockSize;
				}

				/* Write to existing mip */
				if( i < Texture->PlatformData->Mips.Num( ) )
				{
					FTexture2DMipMap& Mip = Texture->PlatformData->Mips[ i ];

					void* Data = Mip.BulkData.Lock( LOCK_READ_WRITE );
					FMemory::Memcpy( Data, DataPtr, NumBytes );
					Mip.BulkData.Unlock( );
				}

				/* Add new mip */
				else
				{
					FTexture2DMipMap* Mip = new( Texture->PlatformData->Mips ) FTexture2DMipMap( );
					Mip->SizeX = CurrentWidth;
					Mip->SizeY = CurrentHeight;

					Mip->BulkData.Lock( LOCK_READ_WRITE );
					Mip->BulkData.Realloc( NumBytes );
					Mip->BulkData.Unlock( );

					void* Data = Mip->BulkData.Lock( LOCK_READ_WRITE );
					FMemory::Memcpy( Data, DataPtr, NumBytes );
					Mip->BulkData.Unlock( );
				}

				/* Set next mip level */
				CurrentWidth /= 2;
				CurrentHeight /= 2;

				DataPtr += NumBytes;
			}

			Texture->UpdateResource( );
		}
	}

	return Texture;
}


//this is how you can make cpp only internal functions!
static EImageFormat::Type GetJoyImageFormat(EJoyImageFormats JoyFormat)
{
	/*
	ImageWrapper.h
	namespace EImageFormat
	{
	
	Enumerates the types of image formats this class can handle
	
	enum Type
	{
		//Portable Network Graphics
		PNG,

		//Joint Photographic Experts Group 
		JPEG,

		//Single channel jpeg
		GrayscaleJPEG,	

		//Windows Bitmap 
		BMP,

		//Windows Icon resource 
		ICO,

		//OpenEXR (HDR) image file format 
		EXR,

		//Mac icon 
		ICNS
	};
};
	*/
	switch(JoyFormat)
	{
		case EJoyImageFormats::JPG : return EImageFormat::JPEG;
		case EJoyImageFormats::PNG : return EImageFormat::PNG;
		case EJoyImageFormats::BMP : return EImageFormat::BMP;
		case EJoyImageFormats::ICO : return EImageFormat::ICO;
		case EJoyImageFormats::EXR : return EImageFormat::EXR;
		case EJoyImageFormats::ICNS : return EImageFormat::ICNS;
	}
	return EImageFormat::JPEG;
} 

static FString GetJoyImageExtension(EJoyImageFormats JoyFormat)
{
	switch(JoyFormat)
	{
		case EJoyImageFormats::JPG : return ".jpg";
		case EJoyImageFormats::PNG : return ".png";
		case EJoyImageFormats::BMP : return ".bmp";
		case EJoyImageFormats::ICO : return ".ico";
		case EJoyImageFormats::EXR : return ".exr";
		case EJoyImageFormats::ICNS : return ".icns";
	}
	return ".png";
} 
UTexture2D* UVictoryBPFunctionLibrary::Victory_LoadTexture2D_FromFile(const FString& FullFilePath,EJoyImageFormats ImageFormat, bool& IsValid,int32& Width, int32& Height)
{
	
	
	IsValid = false;
	UTexture2D* LoadedT2D = NULL;
	
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(GetJoyImageFormat(ImageFormat));
 
	//Load From File
	TArray<uint8> RawFileData;
	if (!FFileHelper::LoadFileToArray(RawFileData, *FullFilePath)) return NULL;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	  
	//Create T2D!
	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
	{ 
		const TArray<uint8>* UncompressedBGRA = NULL;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
		{
			LoadedT2D = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
			
			//Valid?
			if(!LoadedT2D) return NULL;
			//~~~~~~~~~~~~~~
			
			//Out!
			Width = ImageWrapper->GetWidth();
			Height = ImageWrapper->GetHeight();
			 
			//Copy!
			void* TextureData = LoadedT2D->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, UncompressedBGRA->GetData(), UncompressedBGRA->Num());
			LoadedT2D->PlatformData->Mips[0].BulkData.Unlock();

			//Update!
			LoadedT2D->UpdateResource();
		}
	}
	 
	// Success!
	IsValid = true;
	return LoadedT2D;
}
UTexture2D* UVictoryBPFunctionLibrary::Victory_LoadTexture2D_FromFile_Pixels(const FString& FullFilePath,EJoyImageFormats ImageFormat,bool& IsValid, int32& Width, int32& Height, TArray<FLinearColor>& OutPixels)
{
	//Clear any previous data
	OutPixels.Empty();
	
	IsValid = false;
	UTexture2D* LoadedT2D = NULL;
	
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(GetJoyImageFormat(ImageFormat));
 
	//Load From File
	TArray<uint8> RawFileData;
	if (!FFileHelper::LoadFileToArray(RawFileData, *FullFilePath)) return NULL;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	  
	//Create T2D!
	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
	{  
		const TArray<uint8>* UncompressedRGBA = NULL;
		if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, UncompressedRGBA))
		{
			LoadedT2D = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_R8G8B8A8);
			
			//Valid?
			if(!LoadedT2D) return NULL;
			//~~~~~~~~~~~~~~
			
			//Out!
			Width = ImageWrapper->GetWidth();
			Height = ImageWrapper->GetHeight();
			
			const TArray<uint8>& ByteArray = *UncompressedRGBA;
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			
			for(int32 v = 0; v < ByteArray.Num(); v+=4) 
			{ 
				if(!ByteArray.IsValidIndex(v+3)) 
				{ 
					break;
				}   
				     
				OutPixels.Add(
					FLinearColor(
						ByteArray[v],		//R
						ByteArray[v+1],		//G
						ByteArray[v+2],		//B
						ByteArray[v+3] 		//A
					)
				);
			}
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			   
			//Copy!
			void* TextureData = LoadedT2D->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, UncompressedRGBA->GetData(), UncompressedRGBA->Num());
			LoadedT2D->PlatformData->Mips[0].BulkData.Unlock();

			//Update!
			LoadedT2D->UpdateResource();
		}
	}
	 
	// Success!
	IsValid = true;
	return LoadedT2D;
	
}
bool UVictoryBPFunctionLibrary::Victory_Get_Pixel(const TArray<FLinearColor>& Pixels,int32 ImageHeight, int32 x, int32 y, FLinearColor& FoundColor)
{
	int32 Index = y * ImageHeight + x;
	if(!Pixels.IsValidIndex(Index))
	{
		return false;
	}
	 
	FoundColor = Pixels[Index];
	return true;
}


bool UVictoryBPFunctionLibrary::Victory_SavePixels(const FString& FullFilePath,int32 Width, int32 Height, const TArray<FLinearColor>& ImagePixels, FString& ErrorString)
{
	if(FullFilePath.Len() < 1) 
	{
		ErrorString = "No file path";
		return false;
	}
	//~~~~~~~~~~~~~~~~~
	
	//Ensure target directory exists, 
	//		_or can be created!_ <3 Rama
	FString NewAbsoluteFolderPath = FPaths::GetPath(FullFilePath);
	FPaths::NormalizeDirectoryName(NewAbsoluteFolderPath);
	if(!VCreateDirectory(NewAbsoluteFolderPath)) 
	{
		ErrorString = "Folder could not be created, check read/write permissions~ " + NewAbsoluteFolderPath;
		return false;
	}
	
	//Create FColor version
	TArray<FColor> ColorArray;
	for(const FLinearColor& Each : ImagePixels)
	{
		ColorArray.Add(Each.ToFColor(true));
	} 
	 
	if(ColorArray.Num() != Width * Height) 
	{
		ErrorString = "Error ~ height x width is not equal to the total pixel array length!";
		return false;
	}
	  
	//Remove any supplied file extension and/or add accurate one
	FString FinalFilename = FPaths::GetBaseFilename(FullFilePath, false) + ".png";  //false = dont remove path

	//~~~
	
	TArray<uint8> CompressedPNG;
	FImageUtils::CompressImageArray( 
		Width, 
		Height, 
		ColorArray, 
		CompressedPNG
	);
	    
	ErrorString = "Success! or if returning false, the saving of file to disk did not succeed for File IO reasons";
	return FFileHelper::SaveArrayToFile(CompressedPNG, *FinalFilename);
	 
	/*
	//Crashed for JPG, worked great for PNG
	//Maybe also works for BMP so could offer those two as save options?
	
	const int32 x = Width;
	const int32 y = Height;
	if(ColorArray.Num() != x * y) 
	{
		ErrorString = "Error ~ height x width is not equal to the total pixel array length!";
		return false;
	}
	
	//Image Wrapper Module
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	
	//Create Compressor
	IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(GetJoyImageFormat(ImageFormat));
	
	if(!ImageWrapper.IsValid()) 
	{
		ErrorString = "Error ~ Image wrapper could not be created!";
		return false;
	}
	//~~~~~~~~~~~~~~~~~~~~~~
	      
	if ( ! ImageWrapper->SetRaw(
			(void*)&ColorArray[0], 			//mem address of array start
			sizeof(FColor) * x * y, 			//total size
			x, y, 								//dimensions
			ERGBFormat::BGRA,				//LinearColor == RGBA 
			(sizeof(FColor) / 4) * 8			//Bits per pixel
	)) {
		ErrorString = "ImageWrapper::SetRaw() did not succeed";
		return false;
	}
	
	ErrorString = "Success! or if returning false, the saving of file to disk did not succeed for File IO reasons";
	return FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(), *FinalFilename);
	*/
}


class UAudioComponent* UVictoryBPFunctionLibrary::PlaySoundAttachedFromFile(const FString& FilePath, class USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, EAttachLocation::Type LocationType, bool bStopWhenAttachedToDestroyed, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundAttenuation* AttenuationSettings)
{	
	USoundWave* sw = GetSoundWaveFromFile(FilePath);

	if (!sw)
		return NULL;

	return UGameplayStatics::SpawnSoundAttached(sw, AttachToComponent, AttachPointName, Location, LocationType, bStopWhenAttachedToDestroyed, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
}

void UVictoryBPFunctionLibrary::PlaySoundAtLocationFromFile(UObject* WorldContextObject, const FString& FilePath, FVector Location, float VolumeMultiplier, float PitchMultiplier, float StartTime, class USoundAttenuation* AttenuationSettings)
{
	USoundWave* sw = GetSoundWaveFromFile(FilePath);

	if (!sw)
		return;

	UGameplayStatics::PlaySoundAtLocation(WorldContextObject, sw, Location, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
}

class USoundWave* UVictoryBPFunctionLibrary::GetSoundWaveFromFile(const FString& FilePath)
{
	USoundWave* sw = NewObject<USoundWave>(USoundWave::StaticClass());

	if (!sw)
		return NULL;

	//* If true the song was successfully loaded
	bool loaded = false;

	//* loaded song file (binary, encoded)
	TArray < uint8 > rawFile;

	loaded = FFileHelper::LoadFileToArray(rawFile, FilePath.GetCharArray().GetData());

	if (loaded)
	{
		FByteBulkData* bulkData = &sw->CompressedFormatData.GetFormat(TEXT("OGG"));

		bulkData->Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(bulkData->Realloc(rawFile.Num()), rawFile.GetData(), rawFile.Num());
		bulkData->Unlock();

		loaded = fillSoundWaveInfo(sw, &rawFile) == 0 ? true : false;
	}

	if (!loaded)
		return NULL;

	return sw;
}

int32 UVictoryBPFunctionLibrary::fillSoundWaveInfo(class USoundWave* sw, TArray<uint8>* rawFile)
{
    FSoundQualityInfo info;
    FVorbisAudioInfo vorbis_obj = FVorbisAudioInfo();
    if (!vorbis_obj.ReadCompressedInfo(rawFile->GetData(), rawFile->Num(), &info))
    {
        //Debug("Can't load header");
        return 1;
    }

	if(!sw) return 1;
	sw->SoundGroup = ESoundGroup::SOUNDGROUP_Default;
    sw->NumChannels = info.NumChannels;
    sw->Duration = info.Duration;
    sw->RawPCMDataSize = info.SampleDataSize;
    sw->SampleRate = info.SampleRate;

    return 0;
}

      
int32 UVictoryBPFunctionLibrary::findSource(class USoundWave* sw, class FSoundSource* out_audioSource)
{
	FAudioDevice* device = GEngine ? GEngine->GetMainAudioDevice() : NULL; //gently ask for the audio device

	FActiveSound* activeSound;
	FSoundSource* audioSource;
	FWaveInstance* sw_instance;
	if (!device)
	{
		activeSound = NULL;
		audioSource = NULL;
		out_audioSource = audioSource;
		return -1;
	}

	TArray<FActiveSound*> tmpActualSounds = device->GetActiveSounds();
	if (tmpActualSounds.Num())
	{
		for (auto activeSoundIt(tmpActualSounds.CreateIterator()); activeSoundIt; ++activeSoundIt)
		{
			activeSound = *activeSoundIt;
			for (auto WaveInstanceIt(activeSound->WaveInstances.CreateIterator()); WaveInstanceIt; ++WaveInstanceIt)
			{
				sw_instance = WaveInstanceIt.Value();
				if (sw_instance->WaveData->CompressedDataGuid == sw->CompressedDataGuid)
				{
					audioSource = device->WaveInstanceSourceMap.FindRef(sw_instance);
					out_audioSource = audioSource;
					return 0;
				}
			}
		}
	}

	audioSource = NULL;
	activeSound = NULL;
	out_audioSource = audioSource;
	return -2;
}


//~~~ Kris ~~~
 
bool UVictoryBPFunctionLibrary::Array_IsValidIndex(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp, int32 Index)
{
    // We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
    check(0);
    return false;
}

bool UVictoryBPFunctionLibrary::GenericArray_IsValidIndex(void* TargetArray, const UArrayProperty* ArrayProp, int32 Index)
{
    bool bResult = false;
    if (TargetArray)
    {
        FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

        UProperty* InnerProp = ArrayProp->Inner;
        bResult = ArrayHelper.IsValidIndex(Index);
    }
    
    return bResult;
}

float UVictoryBPFunctionLibrary::GetCreationTime(const AActor* Target)
{
    return (Target) ? Target->CreationTime : 0.0f;
}
 
float UVictoryBPFunctionLibrary::GetTimeAlive(const AActor* Target)
{
    return (Target) ? (Target->GetWorld()->GetTimeSeconds() - Target->CreationTime) : 0.0f;
}

bool UVictoryBPFunctionLibrary::CaptureComponent2D_Project(class USceneCaptureComponent2D* Target, FVector Location, FVector2D& OutPixelLocation)
{
    if ((Target == nullptr) || (Target->TextureTarget == nullptr))
    {
        return false;
    }
    
    const FTransform& Transform = Target->GetComponentToWorld();
    FMatrix ViewMatrix = Transform.ToInverseMatrixWithScale();
    FVector ViewLocation = Transform.GetTranslation();

    // swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
    ViewMatrix = ViewMatrix * FMatrix(
        FPlane(0,    0,    1,    0),
        FPlane(1,    0,    0,    0),
        FPlane(0,    1,    0,    0),
        FPlane(0,    0,    0,    1));

    const float FOV = Target->FOVAngle * (float)PI / 360.0f;

    FIntPoint CaptureSize(Target->TextureTarget->GetSurfaceWidth(), Target->TextureTarget->GetSurfaceHeight());
    
    float XAxisMultiplier;
    float YAxisMultiplier;

    if (CaptureSize.X > CaptureSize.Y)
    {
        // if the viewport is wider than it is tall
        XAxisMultiplier = 1.0f;
        YAxisMultiplier = CaptureSize.X / (float)CaptureSize.Y;
    }
    else
    {
        // if the viewport is taller than it is wide
        XAxisMultiplier = CaptureSize.Y / (float)CaptureSize.X;
        YAxisMultiplier = 1.0f;
    }

    FMatrix    ProjectionMatrix = FReversedZPerspectiveMatrix (
        FOV,
        FOV,
        XAxisMultiplier,
        YAxisMultiplier,
        GNearClippingPlane,
        GNearClippingPlane
        );

    FMatrix ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;

    FVector4 ScreenPoint = ViewProjectionMatrix.TransformFVector4(FVector4(Location,1));
    
    if (ScreenPoint.W > 0.0f)
    {
        float InvW = 1.0f / ScreenPoint.W;
        float Y = (GProjectionSignY > 0.0f) ? ScreenPoint.Y : 1.0f - ScreenPoint.Y;
        FIntRect ViewRect = FIntRect(0, 0, CaptureSize.X, CaptureSize.Y);
        OutPixelLocation = FVector2D(
            ViewRect.Min.X + (0.5f + ScreenPoint.X * 0.5f * InvW) * ViewRect.Width(),
            ViewRect.Min.Y + (0.5f - Y * 0.5f * InvW) * ViewRect.Height()
            );
        return true;
    }

    return false;
}    

bool UVictoryBPFunctionLibrary::Capture2D_Project(class ASceneCapture2D* Target, FVector Location, FVector2D& OutPixelLocation)
{
    return (Target) ? CaptureComponent2D_Project(Target->GetCaptureComponent2D(), Location, OutPixelLocation) : false;
}
 
UTextureRenderTarget2D* UVictoryBPFunctionLibrary::CreateTextureRenderTarget2D(int32 InSizeX, int32 InSizeY, FLinearColor ClearColor)
{ 
    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
    
    if (RenderTarget)
    {
        RenderTarget->AddToRoot();
        RenderTarget->ClearColor = ClearColor;
        RenderTarget->InitAutoFormat((InSizeX == 0) ? 256 : InSizeX, (InSizeY == 0) ? 256 : InSizeY);
    }
    
    return RenderTarget;
}

static IImageWrapperPtr GetImageWrapperByExtention(const FString InImagePath)
{
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    if (InImagePath.EndsWith(".png"))
    {
        return ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    }
    else if (InImagePath.EndsWith(".jpg") || InImagePath.EndsWith(".jpeg"))
    {
        return ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    }
    else if (InImagePath.EndsWith(".bmp"))
    {
        return ImageWrapperModule.CreateImageWrapper(EImageFormat::BMP);
    }
    else if (InImagePath.EndsWith(".ico"))
    {
        return ImageWrapperModule.CreateImageWrapper(EImageFormat::ICO);
    }
    else if (InImagePath.EndsWith(".exr"))
    {
        return ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);
    }
    else if (InImagePath.EndsWith(".icns"))
    {
        return ImageWrapperModule.CreateImageWrapper(EImageFormat::ICNS);
    }
    
    return nullptr;
}

bool UVictoryBPFunctionLibrary::CaptureComponent2D_SaveImage(class USceneCaptureComponent2D* Target, const FString ImagePath, const FLinearColor ClearColour)
{
	// Bad scene capture component! No render target! Stay! Stay! Ok, feed!... wait, where was I?
	if ((Target == nullptr) || (Target->TextureTarget == nullptr))
	{
		return false;
	}
	
	FRenderTarget* RenderTarget = Target->TextureTarget->GameThread_GetRenderTargetResource();
	if (RenderTarget == nullptr)
	{
		return false;
	}

	TArray<FColor> RawPixels;
	
	// Format not supported - use PF_B8G8R8A8.
	if (Target->TextureTarget->GetFormat() != PF_B8G8R8A8)
	{
		// TRACEWARN("Format not supported - use PF_B8G8R8A8.");
		return false;
	}

	if (!RenderTarget->ReadPixels(RawPixels))
	{
		return false;
	}

	// Convert to FColor.
	FColor ClearFColour = ClearColour.ToFColor(false); // FIXME - want sRGB or not?

	for (auto& Pixel : RawPixels)
	{
		// Switch Red/Blue changes.
		const uint8 PR = Pixel.R;
		const uint8 PB = Pixel.B;
		Pixel.R = PB;
		Pixel.B = PR;

		// Set alpha based on RGB values of ClearColour.
		Pixel.A = ((Pixel.R == ClearFColour.R) && (Pixel.R == ClearFColour.R) && (Pixel.R == ClearFColour.R)) ? 0 : 255;
	}
	
	IImageWrapperPtr ImageWrapper = GetImageWrapperByExtention(ImagePath);

	const int32 Width = Target->TextureTarget->SizeX;
	const int32 Height = Target->TextureTarget->SizeY;
	
	if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(&RawPixels[0], RawPixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::RGBA, 8))
	{
		FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(), *ImagePath);
		return true;
	}
	
	return false;
}

bool UVictoryBPFunctionLibrary::Capture2D_SaveImage(class ASceneCapture2D* Target, const FString ImagePath, const FLinearColor ClearColour)
{
	return (Target) ? CaptureComponent2D_SaveImage(Target->GetCaptureComponent2D(), ImagePath, ClearColour) : false;
}

UTexture2D* UVictoryBPFunctionLibrary::LoadTexture2D_FromFileByExtension(const FString& ImagePath, bool& IsValid, int32& OutWidth, int32& OutHeight)
{
	UTexture2D* Texture = nullptr;
	IsValid = false;

	// To avoid log spam, make sure it exists before doing anything else.
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ImagePath))
	{
		return nullptr;
	}

	TArray<uint8> CompressedData;
	if (!FFileHelper::LoadFileToArray(CompressedData, *ImagePath))
	{
		return nullptr;
	}
	
	IImageWrapperPtr ImageWrapper = GetImageWrapperByExtention(ImagePath);

	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
	{ 
		const TArray<uint8>* UncompressedRGBA = nullptr;
		
		if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, UncompressedRGBA))
		{
			Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_R8G8B8A8);
			
			if (Texture != nullptr)
			{
				IsValid = true;
				
				OutWidth = ImageWrapper->GetWidth();
				OutHeight = ImageWrapper->GetHeight();

				void* TextureData = Texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(TextureData, UncompressedRGBA->GetData(), UncompressedRGBA->Num());
				Texture->PlatformData->Mips[0].BulkData.Unlock();
				Texture->UpdateResource();
			}
		}
	}

	return Texture;
}

//~~~~~~~~~ END OF CONTRIBUTED BY KRIS ~~~~~~~~~~~
 


//~~~ Inspired by Sahkan ~~~
void UVictoryBPFunctionLibrary::Actor__GetAttachedActors(AActor* ParentActor,TArray<AActor*>& ActorsArray)
{
	if(!ParentActor) return;
	//~~~~~~~~~~~~
	
	ActorsArray.Empty(); 
	ParentActor->GetAttachedActors(ActorsArray);
}
 
void UVictoryBPFunctionLibrary::SetBloomIntensity(APostProcessVolume* PostProcessVolume,float Intensity)
{
	if(!PostProcessVolume) return;
	//~~~~~~~~~~~~~~~~
	
	PostProcessVolume->Settings.bOverride_BloomIntensity 	= true;
	PostProcessVolume->Settings.BloomIntensity 				= Intensity;
}


//~~~ Key To Truth ~~~
//.cpp
//Append different text strings with optional pins.
FString UVictoryBPFunctionLibrary::AppendMultiple(FString A, FString B)
{  
    FString Result = "";

	Result += A;
    Result += B;
	 
    return Result;
}

//~~~ Mhousse ~~~


//TESTING
static void TESTINGInternalDrawDebugCircle(const UWorld* InWorld, const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness=0)
{
	//this is how you can make cpp only internal functions!
	
}