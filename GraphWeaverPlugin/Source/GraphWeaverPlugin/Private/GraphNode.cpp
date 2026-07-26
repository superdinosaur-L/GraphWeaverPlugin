// Copyright 2026 RainButterfly. All Rights Reserved.

#include "GraphNode.h"
#include "FunctionTools.h"
#include "GraphView.h"
#include "Macro.h"
#include "Engine/Engine.h"

UGraphNode::UGraphNode()
{
	Outer = nullptr;
	IndexInRealNodes = -1;
	ExplicitName = "None";
	SourceGraphView = nullptr;
}

UGraphNode::~UGraphNode()
{
}





bool UGraphNode::ProcessInformAuto(UGraphView* InGraph)
{
	//每次当调用SpawnGraphNode并且勾选AutoBuild的时候必然会执行该函数(InGraph会由SpawnGraphNode::ExpandNode自行检查)
	//执行该函数的时候IndexInRecorded必定是-1
	if (InGraph)[[likely]]
	{
		InGraph->AddNewNodeIntelligent(this);
	}
	else
	{
		//SpawnGraphNode::ValidateNodeDuringCompilation
		//实际上下面的报错大概率不会发生,主要由上面的函数和GetBlueprint()->Status = BS_Dirty来确保TargetView正确
		UE_LOG(LogTemp, Error, TEXT("UGraphNode::ProcessInformAuto: InGraph == nullptr"));
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("UGraphNode::ProcessInformAuto: InGraph == nullptr"));
	}
	return true;
}

UNodeInfoBase_GraphWeaver* UGraphNode::ObtainSelfNodeInfo()
{
	if (IndexInRealNodes == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("UGraphNode::ObtainSelfNodeInfo:GraphNode's IndexInRealNodes == -1."));
		UE_LOG(LogTemp, Error, TEXT("UGraphNode::ObtainSelfNodeInfo:GraphNode's IndexInRealNodes == -1."))
		return nullptr;
	}
	return SourceGraphView->RealNodes[IndexInRealNodes];
}

TArray<int32> UGraphNode::ObtainAllActivatedChildNodeInfo()
{
	return UFunctionTools_GraphWeaver::ObtainAllActivatedChildNodeInfo(SourceGraphView, ObtainSelfNodeInfo());
}

TArray<int32> UGraphNode::ObtainAllActivatedBroNodeInfo()
{
	return UFunctionTools_GraphWeaver::ObtainAllActivatedBroNodeInfo(SourceGraphView, ObtainSelfNodeInfo());
}

TArray<int32> UGraphNode::ObtainDirectActivatedChildAndBroNodeInfo()
{
	return UFunctionTools_GraphWeaver::ObtainDirectActivatedChildAndBroNodeInfo(SourceGraphView, ObtainSelfNodeInfo());
}

TArray<int32> UGraphNode::ObtainAllChildNodeInfo()
{
	return UFunctionTools_GraphWeaver::ObtainAllChildNodeInfo(SourceGraphView, ObtainSelfNodeInfo());
}

void UGraphNode::RemoveSelfInfo(bool RemoveChild, bool ReorderRanking)
{
	if (IndexInRealNodes == -1)[[unlikely]]
		return ;
	if (SourceGraphView == nullptr)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("UGraphNode::RemoveSelfInfo:SourceGraphView == nullptr"));
		UE_LOG(LogTemp, Error, TEXT("UGraphNode::RemoveSelfInfo:SourceGraphView == nullptr"));
		EMPTY_LOG_GW();
		return ;
	}
	UFunctionTools_GraphWeaver::RemoveNodes(SourceGraphView, ObtainSelfNodeInfo(), RemoveChild, ReorderRanking);
}

















