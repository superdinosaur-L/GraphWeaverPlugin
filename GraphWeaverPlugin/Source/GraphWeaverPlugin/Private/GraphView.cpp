// Copyright 2026 RainButterfly. All Rights Reserved.

#include "GraphView.h"
#include "Engine/Engine.h"
#include "GraphNode.h"
#include "Macro.h"
#include "x86Intrinsics.h"
#include "Algo/Sort.h"


TArray<int32> Range(int32 MaxNum)
{
	TArray<int32> Value;
	for (int32 i = 0 ; i < MaxNum ; ++i)
		Value.Emplace(i);
	return Value;
}



void FNamesInputNodeMirror::operator=(const FNamesInputNode& Source)
{
	SelfName = Source.SelfName;
	ParentNames = Source.ParentNames;
	BroNames = Source.BroNames;
}

void FConstructConfig::operator=(const FConstructConfig& other)
{
	NamingOfRules = other.NamingOfRules;
	Precision = other.Precision;
}

FClanInfo_GraphWeaver::FClanInfo_GraphWeaver(FString& TargetKey, int32 Index, UGraphView* TargetView)
{
	int32 truncate = TargetView->ConstructConfig.Precision;
	ClanName = TargetKey.Left(truncate);
	Indices.Emplace(Index);
}


UGraphView::UGraphView(const FObjectInitializer& ObjectInitializer)
{
	RealNodesType = UNodeInfoBase_GraphWeaver::StaticClass();
	AddNodeInfo_Construct();
	RealNodes[0]->Activated = true;
	RealNodes[0].Get()->NamesInputNodeMirror.SelfName = "Root";
	RealNodes[0]->ExplicitName = "Root_Script";
	RealNodes[0]->IndexInRealNodes = 0;
	ErrorCodeForConstructView = NAGraphConstructErrorCode::None;
	FString RootClanName = "RTest";
	Clans.Emplace(RootClanName, 0, this);
	Owner = nullptr;
}


UGraphView::~UGraphView()
{
}

UGraphView::CheckResult UGraphView::CheckSameNode_Names(UGraphNode* Node)
{
	if (Node->NamesInput.SelfName == "Root")
	{
		ErrorCodeForConstructView = NAGraphConstructErrorCode::NameSameAsRoot;
		return HasError;
	}
	FString& NewSelfName = Node->NamesInput.SelfName;
	if (ConstructConfig.NamingOfRules == true)
	{
		for (auto& Clan : Clans)
		{
			if (Clan.ClanName == NewSelfName.Left(ConstructConfig.Precision))
			{
				for (int32 IndexInAll : Clan.Indices)
				{
					if (RealNodes[IndexInAll]->NamesInputNodeMirror.SelfName == NewSelfName)
					{
						RealNodes[IndexInAll]->SourceGraphNode = Node;
						Node->IndexInRealNodes = IndexInAll;
						if (WayToDealSameNode == NAWayToDealSameGraphNode::NothingToDo)[[likely]]
							return Repeat;
						if (WayToDealSameNode == NAWayToDealSameGraphNode::OnlyWarningSameNode)
						{
							GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Red,FString::Printf(
							TEXT("You have added two or more 'GraphNode' instances with the same 'SelfName' : %s .GraphViewName : %s"), *NewSelfName, *GraphViewName));
							UE_LOG(LogTemp, Error, TEXT("You have added two or more 'GraphNode' instances with the same 'SelfName' : %s, GraphViewName : %s"), *NewSelfName, *GraphViewName);
							return Repeat;
						}
						WAITING_MOD_LOG();
						return Undefined;
					}
				}
				return Non;
			}
		}
		return Non;
	}

	{
		for (auto& RecordedNode : RealNodes)
		{
			if (NewSelfName == RecordedNode->NamesInputNodeMirror.SelfName)
			{
				RecordedNode->SourceGraphNode = Node;
				Node->IndexInRealNodes = RecordedNode->IndexInRealNodes;
				if (WayToDealSameNode == NAWayToDealSameGraphNode::NothingToDo)[[likely]]
							return Repeat;
				if (WayToDealSameNode == NAWayToDealSameGraphNode::OnlyWarningSameNode)
				{
					GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Red,FString::Printf(
					TEXT("You have added two or more 'GraphNode' instances with the same 'SelfName' : %s .GraphViewName : %s"), *NewSelfName, *GraphViewName));
					UE_LOG(LogTemp, Error, TEXT("You have added two or more 'GraphNode' instances with the same 'SelfName' : %s, GraphViewName : %s"), *NewSelfName, *GraphViewName);
					return Repeat;
				}
				WAITING_MOD_LOG();
				return Undefined;
			}
		}
		return Non;
	}
}

bool UGraphView::CheckIsNodeRemoved(UGraphNode* Node)
{
	for (auto& Name : NodeRemoved)
	{
		if (Name == Node->NamesInput.SelfName)
			return true;
	}
	return false;
}

void UGraphView::DealingWithParent_ChildRelationships(int32 ParentIndex, int32 ChildIndex)
{
	int32 NewElementIndex = RealNodes[ParentIndex].Get()->Family.Children.Emplace(ChildIndex, 1);
	RealNodes[ChildIndex].Get()->Family.Parents.Emplace(ParentIndex, RealNodes[ParentIndex].Get()->Family.Children.Num() - 1);
	RealNodes[ParentIndex].Get()->Family.Children[NewElementIndex].Ranking = RealNodes[ChildIndex].Get()->Family.Parents.Num() - 1;
}

void UGraphView::DealingWithBrothersRelationships(int32 BroIndex, int32 SelfIndex)
{
	int32 NewElementIndex = RealNodes[BroIndex].Get()->Family.Brothers.Emplace(SelfIndex, 1);
	RealNodes[SelfIndex].Get()->Family.Brothers.Emplace(BroIndex, RealNodes[BroIndex].Get()->Family.Brothers.Num() - 1);
	RealNodes[BroIndex].Get()->Family.Brothers[NewElementIndex].Ranking = RealNodes[SelfIndex].Get()->Family.Brothers.Num() - 1;
}

void UGraphView::RemoveDeletedRelationship(UGraphNode* Node)
{
	auto RealNode = RealNodes[Node->IndexInRealNodes];
	for (auto& RemovedNode : NodeRemoved)
	{
		for (int i = RealNode->NamesInputNodeMirror.ParentNames.Num() - 1; i >= 0; i--)
		{
			if (RemovedNode == RealNode->NamesInputNodeMirror.ParentNames[i])
				RealNode->NamesInputNodeMirror.ParentNames.RemoveAtSwap(i);
		}
		for (int i = RealNode->NamesInputNodeMirror.BroNames.Num() - 1; i >= 0; i--)
		{
			if (RemovedNode == RealNode->NamesInputNodeMirror.BroNames[i])
				RealNode->NamesInputNodeMirror.BroNames.RemoveAtSwap(i);
		}
	}
}


UGraphView::CheckResult UGraphView::NamesConstructWay(UGraphNode* TargetNode)
{
	
	FString TargetSelfName = TargetNode->NamesInput.SelfName;
	auto& Mirror = RealNodes[TargetNode->IndexInRealNodes]->NamesInputNodeMirror;

	if (Mirror.ParentNames.Num() == 0)
	{
		DealingWithParent_ChildRelationships(0, TargetNode->IndexInRealNodes);
	}
	
	//有添加父节点数组但是为空
	for (auto& Handle : Mirror.ParentNames)
	{
		if (Handle.Len() == 0)
		{
			DealingWithParent_ChildRelationships(0, TargetNode->IndexInRealNodes);
			Mirror.ParentNames.RemoveSingleSwap(Handle);
			break ;
		}
	}
	//处理完父节点为Root的特殊情况

	//不允许兄弟节点里面出现Brother包含Root的情况.不希望出现乱伦理的事情
	for (auto& BroName : Mirror.BroNames)
	{
		if (BroName.Len() == 0 || BroName == "Root")
		{
			ErrorCodeForConstructView = NAGraphConstructErrorCode::BroNameSameAsRoot;
			return HasError;
		}
	}
	
	//有规则的命名,取代在AlreadyRecorded里面进行暴力搜索,提升速度.只能搜索到已经被记录的节点
	if (ConstructConfig.NamingOfRules)
	{
		bool FindSelfClan = false;

		for (int32 p_i = Mirror.ParentNames.Num() - 1; p_i >= 0; --p_i)
		{
			uint8 Find = 0;
			for (auto& Clan : Clans)
			{
				if (Mirror.ParentNames[p_i].Left(ConstructConfig.Precision) == Clan.ClanName)
				{
					for (int32 IndexInAll : Clan.Indices)
					{
						if (RealNodes[IndexInAll]->NamesInputNodeMirror.SelfName == Mirror.ParentNames[p_i])
						{
							DealingWithParent_ChildRelationships(IndexInAll, TargetNode->IndexInRealNodes);
							Mirror.ParentNames.RemoveAtSwap(p_i);
							Find = 1;
							break ;
						}
					}
				}
				if (Find == 1)
					break ;
			}
		}

		for (int32 b_i = Mirror.BroNames.Num() - 1; b_i >= 0; --b_i)
		{
			uint8 Find = 0;
			for (auto& Clan : Clans)
			{
				if (Mirror.BroNames[b_i].Left(ConstructConfig.Precision) == Clan.ClanName)
				{
					for (int32 IndexInAll : Clan.Indices)
					{
						if (RealNodes[IndexInAll]->NamesInputNodeMirror.SelfName == Mirror.BroNames[b_i])
						{
							DealingWithBrothersRelationships(IndexInAll, TargetNode->IndexInRealNodes);
							Mirror.BroNames.RemoveAtSwap(b_i);
							Find = 1;
							break;
						}
					}
					if (Find == 1)
						break ;
				}
			}
		}

		for (auto& Clan : Clans)
		{
			if (Clan.ClanName == TargetSelfName.Left(ConstructConfig.Precision))
			{
				FindSelfClan = true;
				Clan.Indices.Emplace(TargetNode->IndexInRealNodes);
				break ;
			}
		}
		
		
		//处理连接情况的第一种情况，寻找自己的可能子节点。此时TargetNode扮演的角色是A2
		for (int ChildIndex = WillVerticalAwakeNode.Num() - 1; ChildIndex >= 0; --ChildIndex)
		{
			auto Child = RealNodes[WillVerticalAwakeNode[ChildIndex]];
			for (FString& ParentName : Child->NamesInputNodeMirror.ParentNames)
			{
				if (ParentName == TargetSelfName)
				{
					DealingWithParent_ChildRelationships(TargetNode->IndexInRealNodes, WillVerticalAwakeNode[ChildIndex]);
					Child->NamesInputNodeMirror.ParentNames.RemoveSingleSwap(ParentName);
					if (Child->NamesInputNodeMirror.ParentNames.Num() == 0)
						WillVerticalAwakeNode.RemoveAtSwap(ChildIndex);
					break ;
				}
			}
		}
		
		//处理连接的第一种情况，A1指向兄弟A2。寻找潜在的兄弟节点
		for (int BroIndex = WillHorizontalAwakeNode.Num() - 1; BroIndex >= 0; --BroIndex)
		{
			auto Bro = RealNodes[WillHorizontalAwakeNode[BroIndex]];
			for (FString& BroName : Bro->NamesInputNodeMirror.BroNames)
			{
				if (BroName == TargetSelfName)
				{
					DealingWithBrothersRelationships(TargetNode->IndexInRealNodes, WillHorizontalAwakeNode[BroIndex]);
					Bro->NamesInputNodeMirror.BroNames.RemoveSingleSwap(BroName);
					if (Bro->NamesInputNodeMirror.BroNames.Num() == 0)
						WillHorizontalAwakeNode.RemoveAtSwap(BroIndex);
					break ;
				}
			}
		}
		
		//自己的父节点没有完全找完，需要加入到纵向等待序列
		if (Mirror.ParentNames.Num() != 0)
			WillVerticalAwakeNode.Emplace(TargetNode->IndexInRealNodes);

		//自己的兄弟节点还没有找完，需要加入横向等待队列
		if (Mirror.BroNames.Num() != 0)
			WillHorizontalAwakeNode.Emplace(TargetNode->IndexInRealNodes);

		
		if (!FindSelfClan)
			Clans.Emplace(Mirror.SelfName, TargetNode->IndexInRealNodes, this);
		
		return Non;
	}//if (NamesConstructConfig.NamingOfRules)

	auto SelfDes = RealNodes[TargetNode->IndexInRealNodes].Get();
	//不开启NamingOfRules，但是运行速度可能会降低
	//直接进行暴力寻找，因为缺失信息太多，貌似只能暴力查找
	for (auto& PerDes : RealNodes)
	{
		for (int i = SelfDes->NamesInputNodeMirror.ParentNames.Num() - 1; i >= 0; --i)
		{
			if (PerDes->NamesInputNodeMirror.SelfName == SelfDes->NamesInputNodeMirror.ParentNames[i])
			{
				DealingWithParent_ChildRelationships(PerDes->IndexInRealNodes, TargetNode->IndexInRealNodes);
				SelfDes->NamesInputNodeMirror.ParentNames.RemoveAtSwap(i);
				break ;
			}
		}
	}
	for (auto& PerDes : RealNodes)
	{
		for (int i = SelfDes->NamesInputNodeMirror.BroNames.Num() - 1; i >= 0; --i)
		{
			if (PerDes->NamesInputNodeMirror.SelfName == SelfDes->NamesInputNodeMirror.BroNames[i])
			{
				DealingWithBrothersRelationships(PerDes->IndexInRealNodes, TargetNode->IndexInRealNodes);
				SelfDes->NamesInputNodeMirror.BroNames.RemoveAtSwap(i);
				break ;
			}
		}
	}
	//已记录的Parent和Bro已经寻找完了，开始寻找等待的Parent和Bro

	

	//处理连接情况的第一种情况，寻找自己的可能子节点。此时TargetNode扮演的角色是A2
	for (int ChildIndex = WillVerticalAwakeNode.Num() - 1; ChildIndex >= 0; --ChildIndex)
	{
		auto Child = RealNodes[WillVerticalAwakeNode[ChildIndex]].Get();
		for (auto& ParentName : Child->NamesInputNodeMirror.ParentNames)
		{
			if (ParentName == TargetSelfName)
			{
				DealingWithParent_ChildRelationships(TargetNode->IndexInRealNodes, Child->IndexInRealNodes);
				Child->NamesInputNodeMirror.ParentNames.RemoveSingleSwap(TargetSelfName);
				if (Child->NamesInputNodeMirror.ParentNames.Num() == 0)
					WillVerticalAwakeNode.RemoveAtSwap(ChildIndex);
				break ;
			}
		}
	}

	for (int BroIndex = WillHorizontalAwakeNode.Num() - 1 ; BroIndex >= 0 ; --BroIndex)
	{
		auto Bro = RealNodes[WillHorizontalAwakeNode[BroIndex]].Get();
		for (auto& BroName : Bro->NamesInputNodeMirror.BroNames)
		{
			if (BroName == TargetSelfName)
			{
				DealingWithBrothersRelationships(TargetNode->IndexInRealNodes, Bro->IndexInRealNodes);
				Bro->NamesInputNodeMirror.BroNames.RemoveSingleSwap(TargetSelfName);
				if (Bro->NamesInputNodeMirror.BroNames.Num() == 0)
					WillHorizontalAwakeNode.RemoveAtSwap(BroIndex);
				break ;
			}
		}
	}

	//自己的父节点没有完全找完，需要加入到纵向等待序列
	if (RealNodes[TargetNode->IndexInRealNodes]->NamesInputNodeMirror.ParentNames.Num() != 0)
		WillVerticalAwakeNode.Emplace(TargetNode->IndexInRealNodes);

	//自己的兄弟节点还没有找完，需要加入横向等待队列
	if (RealNodes[TargetNode->IndexInRealNodes]->NamesInputNodeMirror.BroNames.Num() != 0)
		WillHorizontalAwakeNode.Emplace(TargetNode->IndexInRealNodes);

	return Non;
}


void UGraphView::AllocateGraphViewSize(int32 Size)
{
	if (RealNodes.Num() == 1)
	{
		RealNodes.Reserve(Size + 1);
	}
}

void UGraphView::LogByErrorCode(UGraphNode* NewNode)
{
	switch (ErrorCodeForConstructView)
	{
	case NAGraphConstructErrorCode::NameSameAsRoot:
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("SelfName cannot be empty or Same As Root. ExplicitName: %s  , SelfName: %s"), *NewNode->ExplicitName, *NewNode->NamesInput.SelfName));
			UE_LOG(LogTemp, Error, TEXT("SelfName cannot be empty or Same As Root. ExplicitName: %s , SelfName: %s"),
				*NewNode->ExplicitName,  *NewNode->NamesInput.SelfName);
		}
		break ;
	case NAGraphConstructErrorCode::BroNameSameAsRoot:
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
				TEXT("Brother nodes cannot contain a Root node.ExplicitName: %s , SelfName: %s"), *NewNode->ExplicitName, *NewNode->NamesInput.SelfName));
			UE_LOG(LogTemp, Error, TEXT("Brother nodes cannot contain a Root node.ExplicitName: %s , SelfName: %s"), *NewNode->ExplicitName, *NewNode->NamesInput.SelfName);
		}
		break ;
	default:
		WAITING_MOD_LOG();
	}
}

bool UGraphView::AddNewNodeIntelligent(UGraphNode* NewNode)
{
	if (ErrorCodeForConstructView == NAGraphConstructErrorCode::None)[[likely]]
	{
		auto CheckEnumResult = Non;
		CheckEnumResult = CheckSameNode_Names(NewNode);
		if (CheckEnumResult == Repeat)
			return true;
		if (CheckEnumResult == HasError)[[unlikely]]
			goto Error;
		if (CheckEnumResult == Undefined)[[unlikely]]
		{
			WAITING_MOD_LOG();
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Magenta,
				TEXT("Undefined case encountered in UGraphView::AddNewNodeIntelligent."));
			return false;
		}

		if (NodeRemoved.Num() > 0)
		{
			if (CheckIsNodeRemoved(NewNode))
				return true;
		}
		int32 IndexInRecorded = AddNodeInfo();
		RealNodes[IndexInRecorded].Get()->IndexInRealNodes = IndexInRecorded;
		RealNodes[IndexInRecorded].Get()->SourceGraphNode = NewNode;
		//_GraphNode标记在FunctionTools.cpp::SpawnNodeAndSetBasicProperty被添加
		RealNodes[IndexInRecorded].Get()->ExplicitName = NewNode->ExplicitName.LeftChop(FString("_GraphNode").Len()) + "_Script";
		NewNode->IndexInRealNodes = IndexInRecorded;
		RealNodes[IndexInRecorded]->NamesInputNodeMirror = NewNode->NamesInput;
		
		CheckEnumResult = NamesConstructWay(NewNode);
		if (CheckEnumResult == Non)
			return true;
		if (CheckEnumResult == HasError)
			goto Error;
		
		WAITING_MOD_LOG();
		return false;
	}

	Error:
	LogByErrorCode(NewNode);
	return false;
}

bool UGraphView::ReAddNode(UGraphNode* Node)
{
#if 0
	for (int i : WillVerticalAwakeNode)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Orange, FString::Printf(TEXT("VerIndex: %d"), i));
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Orange,
			FString::Printf(TEXT("VerName: %s"), *RealNodes[i]->NamesInputNodeMirror.SelfName));
		UE_LOG(LogTemp, Error, TEXT("VerIndex: %d"), i);
		UE_LOG(LogTemp, Error, TEXT("VerName: %s"), *RealNodes[i]->NamesInputNodeMirror.SelfName);
		EMPTY_LOG_GW();
	}
	for (int i : WillHorizontalAwakeNode)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Silver, FString::Printf(TEXT("HorIndex: %d"), i));
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Silver,
			FString::Printf(TEXT("HorName: %s"), *RealNodes[i]->NamesInputNodeMirror.SelfName));
		UE_LOG(LogTemp, Error, TEXT("HorIndex: %d"), i);
		UE_LOG(LogTemp, Error, TEXT("HorName: %s"), *RealNodes[i]->NamesInputNodeMirror.SelfName);
		EMPTY_LOG_GW();
	}
#endif
	FString& NodeName = Node->NamesInput.SelfName;
	if (NodeName == "Root")
	{
		ErrorCodeForConstructView = NAGraphConstructErrorCode::NameSameAsRoot;
		LogByErrorCode(Node);
		return false;
	}
	for (int32 i = 0 ; i < NodeRemoved.Num() ; i++)
	{
		if (NodeRemoved[i] == NodeName)
		{
			int32 IndexInRecorded = AddNodeInfo();
			RealNodes[IndexInRecorded].Get()->IndexInRealNodes = IndexInRecorded;
			RealNodes[IndexInRecorded].Get()->SourceGraphNode = Node;
			//_GraphNode标记在FunctionTools.cpp::SpawnNodeAndSetBasicProperty被添加
			RealNodes[IndexInRecorded].Get()->ExplicitName = Node->ExplicitName.LeftChop(FString("_GraphNode").Len()) + "_Script";
			Node->IndexInRealNodes = IndexInRecorded;
			RealNodes[IndexInRecorded]->NamesInputNodeMirror = Node->NamesInput;
			auto& Mirror = RealNodes[IndexInRecorded]->NamesInputNodeMirror;
			//处理特殊情况
			if (Mirror.ParentNames.Num() == 0)
				DealingWithParent_ChildRelationships(0, IndexInRecorded);
			for (int32 j = 0; j < Mirror.ParentNames.Num(); j++)
			{
				if (Mirror.ParentNames[j].Len() == 0)
				{
					DealingWithParent_ChildRelationships(0, IndexInRecorded);
					Mirror.ParentNames.RemoveAtSwap(j);
					break ;
				}
			}
			for (auto& BroName : Mirror.BroNames)
			{
				if (BroName == "Root" || BroName.Len() == 0)
				{
					ErrorCodeForConstructView = NAGraphConstructErrorCode::BroNameSameAsRoot;
					LogByErrorCode(Node);
					return false ;
				}
			}
			
			if (ConstructConfig.NamingOfRules)
			{
				for (int32 j = Mirror.ParentNames.Num() - 1 ; j >= 0 ; j--)
				{
					uint8 Find = 0;
					for (auto& Clan : Clans)
					{
						if (Mirror.ParentNames[j].Left(ConstructConfig.Precision) == Clan.ClanName)
						{
							for (int32 II = 0 ; II < Clan.Indices.Num() ; II++)
							{
								if (HasSSE41() && II < Clan.Indices.Num() - 1)
								{
									_mm_prefetch((char*)&RealNodes[Clan.Indices[II + 1]], _MM_HINT_T0);
								}
								if (RealNodes[Clan.Indices[II]]->NamesInputNodeMirror.SelfName == Mirror.ParentNames[j])
								{
									DealingWithParent_ChildRelationships(Clan.Indices[II], IndexInRecorded);
									Mirror.ParentNames.RemoveAtSwap(j);
									Find = 1;
									break ;
								}
							}
							if (Find)
								break ;
						}
					}
				}
				for (int32 j = WillHorizontalAwakeNode.Num() - 1 ; j >= 0 ; j--)
				{
					if (j >= 1 && HasSSE41())
						_mm_prefetch((char*)&RealNodes[WillHorizontalAwakeNode[j - 1]], _MM_HINT_T0);
					auto Bro = RealNodes[WillHorizontalAwakeNode[j]];
					for (int32 k = Bro->NamesInputNodeMirror.BroNames.Num() - 1 ; k >= 0 ; k--)
					{
						auto& BroName = Bro->NamesInputNodeMirror.BroNames[k];
						if (BroName == NodeName)
						{
							DealingWithBrothersRelationships(IndexInRecorded, WillHorizontalAwakeNode[j]);
							Bro->NamesInputNodeMirror.BroNames.RemoveAtSwap(k);
							Mirror.BroNames.RemoveSingleSwap(Bro->NamesInputNodeMirror.SelfName);
							if (Bro->NamesInputNodeMirror.BroNames.Num() == 0)
								WillHorizontalAwakeNode.RemoveAtSwap(j);
							break ;
						}
					}
				}
				for (int32 j = WillVerticalAwakeNode.Num() - 1 ; j >= 0 ; j--)
				{
					if (j >= 1 && HasSSE41())
						_mm_prefetch((char*)RealNodes[WillVerticalAwakeNode[j - 1]], _MM_HINT_T0);
					auto Child = RealNodes[WillVerticalAwakeNode[j]];
					for (int k = Child->NamesInputNodeMirror.ParentNames.Num() - 1 ; k >= 0 ; k--)
					{
						auto& ParentName = Child->NamesInputNodeMirror.ParentNames[k];
						if (ParentName == NodeName)
						{
							DealingWithParent_ChildRelationships(IndexInRecorded, WillVerticalAwakeNode[j]);
							Child->NamesInputNodeMirror.ParentNames.RemoveAtSwap(k);
							if (Child->NamesInputNodeMirror.ParentNames.Num() == 0)
								WillVerticalAwakeNode.RemoveAtSwap(j);
							break ;
						}
					}
				}
				for (auto& Clan : Clans)
				{
					if (Clan.ClanName == NodeName.Left(ConstructConfig.Precision))
					{
						Clan.Indices.Emplace(IndexInRecorded);
						break ;
					}
				}
				if (Mirror.ParentNames.Num() != 0)
					WillVerticalAwakeNode.Emplace(IndexInRecorded);
				if (Mirror.BroNames.Num() != 0)
					WillHorizontalAwakeNode.Emplace(IndexInRecorded);
			}
			else
			{
				for (auto PerNode : RealNodes)
				{
					auto& PerNodeMirror = PerNode->NamesInputNodeMirror;
					for (int32 j = Mirror.ParentNames.Num() - 1 ; j >= 0 ; j--)
					{
						if (PerNodeMirror.SelfName == Mirror.ParentNames[j])
						{
							DealingWithParent_ChildRelationships(PerNode->IndexInRealNodes, IndexInRecorded);
							Mirror.ParentNames.RemoveAtSwap(j);
							break ;
						}
					}
				}
				for (int32 j = WillVerticalAwakeNode.Num() - 1 ; j >= 0 ; j--)
				{
					if (j >= 1 && HasSSE41())
						_mm_prefetch((char*)&RealNodes[WillVerticalAwakeNode[j - 1]], _MM_HINT_T0);
					auto Child = RealNodes[WillVerticalAwakeNode[j]];
					for (int32 k = Child->NamesInputNodeMirror.ParentNames.Num() - 1 ; k >= 0 ; k--)
					{
						if (Child->NamesInputNodeMirror.ParentNames[k] == NodeName)
						{
							DealingWithParent_ChildRelationships(IndexInRecorded, WillVerticalAwakeNode[j]);
							Child->NamesInputNodeMirror.ParentNames.RemoveAtSwap(k);
							if (Child->NamesInputNodeMirror.ParentNames.Num() == 0)
								WillVerticalAwakeNode.RemoveAtSwap(j);
							break ;
						}
					}
				}
				for (int32 j = WillHorizontalAwakeNode.Num() - 1 ; j >= 0 ; j--)
				{
					if (j >= 1 && HasSSE41())
						_mm_prefetch((char*)&RealNodes[WillHorizontalAwakeNode[j - 1]], _MM_HINT_T0);
					auto Bro = RealNodes[WillHorizontalAwakeNode[j]];
					for (int32 k = Bro->NamesInputNodeMirror.BroNames.Num() - 1 ; k >= 0 ; k--)
					{
						if (Bro->NamesInputNodeMirror.BroNames[k] == NodeName)
						{
							DealingWithBrothersRelationships(IndexInRecorded, WillHorizontalAwakeNode[j]);
							Mirror.BroNames.RemoveSingleSwap(Bro->NamesInputNodeMirror.SelfName);
							Bro->NamesInputNodeMirror.BroNames.RemoveAtSwap(k);
							if (Bro->NamesInputNodeMirror.BroNames.Num() == 0)
								WillHorizontalAwakeNode.RemoveAtSwap(j);
							break ;
						}
					}
				}
				if (Mirror.ParentNames.Num() != 0)
					WillVerticalAwakeNode.Emplace(IndexInRecorded);
				if (Mirror.BroNames.Num() != 0)
					WillHorizontalAwakeNode.Emplace(IndexInRecorded);
			}
			NodeRemoved.RemoveAtSwap(i);
			return true;
		}
	}
	auto Result = CheckSameNode_Names(Node);
	if (Result == Repeat || Result == Non)[[likely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow,
			FString::Printf(TEXT("You should add node into view by GraphNode::ProcessInformAuto instead of ReAddNode. \n "
					  "NodeName: %s"), *NodeName));
		AddNewNodeIntelligent(Node);
		return true;
	}
	return false;
}

void UGraphView::SetWayToDealSameNode(TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> Way)
{
	WayToDealSameNode = Way;
}

bool UGraphView::ValidateRankingConsistency_Inner(UGraphView* GraphView, TArray<int32>& IgnoredNodeIndices)
{
	if (GraphView == nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("UGraphView::ValidateRankingConsistency::GraphView Is Invalid"));
		return false;
	}
	bool result = true;
	Algo::Sort(IgnoredNodeIndices, [](int32 A, int32 B) {
			return A < B;  // 升序, 小数字在前
		});
	auto& RealNodes = GraphView->RealNodes;
	int32 ForkArrived = 0;
	for (auto& Node : RealNodes)
	{
		if (IgnoredNodeIndices.Num() > 0 && ForkArrived < IgnoredNodeIndices.Num() && IgnoredNodeIndices[ForkArrived] == Node->IndexInRealNodes)
		{
			ForkArrived++;
			continue ;
		}
		for (auto& Parent : Node->Family.Parents)
		{
			if (RealNodes[Parent.IndexInRealNodes]->Family.Children.Num() > Parent.Ranking
				&& RealNodes[Parent.IndexInRealNodes]->Family.Children[Parent.Ranking].IndexInRealNodes == Node->IndexInRealNodes)
				continue ;
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				FString::Printf(TEXT("GraphWeaver:ValidateRankingConsistency::As child in parent ranking is wrong, child index: %d, parent index: %d"), Node->IndexInRealNodes, Parent.IndexInRealNodes));
			UE_LOG(LogTemp, Error, TEXT("GraphWeaver:ValidateRankingConsistency::As child in parent ranking is wrong, child index: %d, parent index: %d"), Node->IndexInRealNodes, Parent.IndexInRealNodes);
			EMPTY_LOG_GW();
			result = false;
		}
		for (auto& Bro : Node->Family.Brothers)
		{
			if (RealNodes[Bro.IndexInRealNodes]->Family.Brothers.Num() > Bro.Ranking
				&& RealNodes[Bro.IndexInRealNodes]->Family.Brothers[Bro.Ranking].IndexInRealNodes == Node->IndexInRealNodes)
				continue ;
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				FString::Printf(TEXT("GraphWeaver:ValidateRankingConsistency::As bro in bro ranking is wrong, self index: %d, bro index: %d"), Node->IndexInRealNodes, Bro.IndexInRealNodes));
			UE_LOG(LogTemp, Error, TEXT("GraphWeaver:ValidateRankingConsistency::As bro in bro ranking is wrong, self index: %d, bro index: %d"), Node->IndexInRealNodes, Bro.IndexInRealNodes);
			EMPTY_LOG_GW();
			result = false;
		}
		for (auto& Child : Node->Family.Children)
		{
			if (RealNodes[Child.IndexInRealNodes]->Family.Parents.Num() > Child.Ranking
				&& RealNodes[Child.IndexInRealNodes]->Family.Parents[Child.Ranking].IndexInRealNodes == Node->IndexInRealNodes)
				continue ;

			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				FString::Printf(TEXT("GraphWeaver:ValidateRankingConsistency::As parent in child ranking is wrong, parent index: %d, child index: %d"), Node->IndexInRealNodes, Child.IndexInRealNodes));
			UE_LOG(LogTemp, Error, TEXT("GraphWeaver:ValidateRankingConsistency::As parent in child ranking is wrong, parent index: %d, child index: %d"), Node->IndexInRealNodes, Child.IndexInRealNodes);
			EMPTY_LOG_GW();
			result = false;
		}
	}
	return result;
}

bool UGraphView::ValidateRankingConsistency(TArray<int32>&& IgnoredNodeIndices)
{
	return ValidateRankingConsistency_Inner(this, IgnoredNodeIndices);
}

bool UGraphView::ValidateRankingConsistency(TArray<int32>& IgnoredNodeIndices)
{
	return ValidateRankingConsistency_Inner(this, IgnoredNodeIndices);
}

bool UGraphView::ValidateRankingConsistencyLight_Inner(UGraphView* GraphView, TArray<int32>& IgnoredNodeIndices)
{
	Algo::Sort(IgnoredNodeIndices, [](int32 A, int32 B) {
			return A < B;  // 升序, 小数字在前
		});
	auto& RealNodes = GraphView->RealNodes;
	int32 ForkArrived = 0;
	for (auto& Node : RealNodes)
	{
		if (IgnoredNodeIndices.Num() > 0 && ForkArrived < IgnoredNodeIndices.Num() && IgnoredNodeIndices[ForkArrived] == Node->IndexInRealNodes)
		{
			ForkArrived++;
			continue ;
		}
		for (auto& Parent : Node->Family.Parents)
		{
			if (RealNodes[Parent.IndexInRealNodes]->Family.Children.Num() > Parent.Ranking
				&& RealNodes[Parent.IndexInRealNodes]->Family.Children[Parent.Ranking].IndexInRealNodes == Node->IndexInRealNodes)
				continue ;
			return false;
		}
		for (auto& Bro : Node->Family.Brothers)
		{
			if (RealNodes[Bro.IndexInRealNodes]->Family.Brothers.Num() > Bro.Ranking
				&& RealNodes[Bro.IndexInRealNodes]->Family.Brothers[Bro.Ranking].IndexInRealNodes == Node->IndexInRealNodes)
				continue ;
			return false;
		}
		for (auto& Child : Node->Family.Children)
		{
			if (RealNodes[Child.IndexInRealNodes]->Family.Parents.Num() > Child.Ranking
				&& RealNodes[Child.IndexInRealNodes]->Family.Parents[Child.Ranking].IndexInRealNodes == Node->IndexInRealNodes)
				continue ;
			return false;
		}
	}
	return true;
}

bool UGraphView::ValidateRankingConsistencyLight(TArray<int32>&& IgnoredNodeIndices)
{
	return ValidateRankingConsistencyLight_Inner(this, IgnoredNodeIndices);
}

bool UGraphView::ValidateRankingConsistencyLight(TArray<int32>& IgnoredNodeIndices)
{
	return ValidateRankingConsistencyLight_Inner(this, IgnoredNodeIndices);
}

void UGraphView::AddRemovedNodeName(TObjectPtr<UNodeInfoBase_GraphWeaver> Node)
{
	NodeRemoved.Emplace(Node->NamesInputNodeMirror.SelfName);
}

FDCBasicInfoForWholeView UGraphView::GetBasicDCForWholeView()
{
	FDCBasicInfoForWholeView Result;
	Result.Clans = Clans;
	Result.WillVerticalAwakeNode = WillVerticalAwakeNode;
	Result.WillHorizontalAwakeNode = WillHorizontalAwakeNode;
	Result.NodeRemoved = NodeRemoved;
	return Result;
}

void UGraphView::ResetBasicInformFromBasicDC(FDCBasicInfoForWholeView& BasicDC)
{
	Clans = BasicDC.Clans;
	WillVerticalAwakeNode = BasicDC.WillVerticalAwakeNode;
	WillHorizontalAwakeNode = BasicDC.WillHorizontalAwakeNode;
	NodeRemoved = BasicDC.NodeRemoved;
}
















