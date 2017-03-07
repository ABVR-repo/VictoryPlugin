/*

	By Rama

*/
#include "VictoryISM.h"
#include "VictoryBPLibraryPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// VictoryISM

AVictoryISM::AVictoryISM(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = Mesh = ObjectInitializer.CreateDefaultSubobject<UInstancedStaticMeshComponent>(this, "VictoryInstancedMesh");
} 

void AVictoryISM::BeginPlay() 
{  
	Super::BeginPlay();
	//~~~~~~~~~
	 

}