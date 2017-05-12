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
	RootComponent = Mesh = ObjectInitializer.CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(this, "VictoryInstancedMesh");
} 

void AVictoryISM::BeginPlay() 
{  
	Super::BeginPlay();
	//~~~~~~~~~
	 

}