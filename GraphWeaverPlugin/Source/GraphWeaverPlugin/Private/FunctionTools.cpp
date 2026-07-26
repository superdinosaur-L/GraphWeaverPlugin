// Copyright 2026 RainButterfly. All Rights Reserved.

#include "FunctionTools.h"
#include "GraphNode.h"
#include "GraphView.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include <functional>

#include "x86Intrinsics.h"
#include "Algo/Sort.h"
#include "Misc/MemStack.h"

UFunctionTools_GraphWeaver::UFunctionTools_GraphWeaver()
{
}

UFunctionTools_GraphWeaver::~UFunctionTools_GraphWeaver()
{
}


TArray<int32> UFunctionTools_GraphWeaver::ObtainAllActivatedChildNodeInfo(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo)
{
	auto& Des = *NodeInfo;
	TArray<int32> rr;
	if (Des.IndexInRealNodes >= SourceGraphView->RealNodes.Num())[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllActivatedChildDescription"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllActivatedChildDescription"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName);
		return rr;
	}
	if (Des.IndexInRealNodes == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllActivatedChildDescription"), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllActivatedChildDescription"), *Des.ExplicitName);
		return rr;
	}
	if (!IsValid(SourceGraphView))[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("SourceGraphView is invalid.Func: ObtainAllActivatedChildDescription"));
		UE_LOG(LogTemp, Error, TEXT("SourceGraphView is invalid.Func: ObtainAllActivatedChildDescription"));
		return rr;
	}

	TArray<int32> ForkIndex;
	ForkIndex.Emplace(Des.IndexInRealNodes);
	int32 ForkArrived = 0;

	FMemMark Mar(FMemStack::Get());
	FMemStack& Stack = FMemStack::Get();

	uint8* RecordedHash = (uint8*)Stack.Alloc(SourceGraphView->RealNodes.Num() * sizeof(uint8), alignof(uint8));
	FMemory::Memzero(RecordedHash, SourceGraphView->RealNodes.Num() * sizeof(uint8));
	
	std::function<void()> f = [&]()
	{
		auto& NowDes = SourceGraphView->RealNodes[ForkIndex[ForkArrived]];
		for (auto& Child : NowDes->Family.Children)
		{
			if (RecordedHash[Child.IndexInRealNodes] == 1)
				continue ;
			RecordedHash[Child.IndexInRealNodes] = 1;
			ForkIndex.Emplace(Child.IndexInRealNodes);
			if (SourceGraphView->RealNodes[Child.IndexInRealNodes]->Activated == true)
				rr.Emplace(Child.IndexInRealNodes);
		}
	};
	while (ForkArrived < ForkIndex.Num())
	{
		f();
		ForkArrived++;
	}
	
	return rr;
}

TArray<int32> UFunctionTools_GraphWeaver::ObtainAllActivatedBroNodeInfo(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo)
{
	auto& Des = *NodeInfo;
	TArray<int32> rr;
	if (Des.IndexInRealNodes >= SourceGraphView->RealNodes.Num())[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllActivatedBroDescription"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllActivatedBroDescription"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName);
		return rr;
	}
	if (Des.IndexInRealNodes == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllActivatedBroDescription"), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllActivatedBroDescription"), *Des.ExplicitName);
		return rr;
	}
	if (!IsValid(SourceGraphView))[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("SourceGraphView is invalid.Func: ObtainAllActivatedBroDescription"));
		UE_LOG(LogTemp, Error, TEXT("SourceGraphView is invalid.Func: ObtainAllActivatedBroDescription"));
		return rr;
	}

	TArray<int32> ForkIndex;
	ForkIndex.Emplace(Des.IndexInRealNodes);
	int32 ForkArrived = 0;

	FMemMark Mark(FMemStack::Get());
	FMemStack& MemStack = FMemStack::Get();
	uint8* RecordedHash = static_cast<uint8*>(MemStack.Alloc(SourceGraphView->RealNodes.Num() * sizeof(uint8), 1));
	FMemory::Memzero(RecordedHash, SourceGraphView->RealNodes.Num() * sizeof(uint8));
	
	auto f = [&]()
	{
		auto& NowDes = SourceGraphView->RealNodes[ForkIndex[ForkArrived]];
		for (auto& Bro : NowDes->Family.Brothers)
		{
			if (RecordedHash[Bro.IndexInRealNodes] == 1)
				continue ;
			RecordedHash[Bro.IndexInRealNodes] = 1;
			ForkIndex.Emplace(Bro.IndexInRealNodes);
			if (SourceGraphView->RealNodes[Bro.IndexInRealNodes]->Activated == true)
				rr.Emplace(Bro.IndexInRealNodes);
		}
	};
	while (ForkArrived < ForkIndex.Num())
	{
		f();
		ForkArrived++;
	}
	return rr;
}

TArray<int32> UFunctionTools_GraphWeaver::ObtainDirectActivatedChildAndBroNodeInfo(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo)
{
	auto& Des = *NodeInfo;
	TArray<int32> rr;
	if (Des.IndexInRealNodes >= SourceGraphView->RealNodes.Num())[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainDirectActivatedChildAndBroDes"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainDirectActivatedChildAndBroDes"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName);
		return rr;
	}
	if (Des.IndexInRealNodes == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainDirectActivatedChildAndBroDes"), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainDirectActivatedChildAndBroDes"), *Des.ExplicitName);
		return rr;
	}
	if (!IsValid(SourceGraphView))[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("SourceGraphView is invalid.Func: ObtainDirectActivatedChildAndBroDes"));
		UE_LOG(LogTemp, Error, TEXT("SourceGraphView is invalid.Func: ObtainDirectActivatedChildAndBroDes"));
		return rr;
	}

	for (auto& Bro : Des.Family.Brothers)
	{
		if (SourceGraphView->RealNodes[Bro.IndexInRealNodes]->Activated == true)
			rr.Emplace(Bro.IndexInRealNodes);
	}
	for (auto& Child : Des.Family.Children)
	{
		if (SourceGraphView->RealNodes[Child.IndexInRealNodes]->Activated == true)
			rr.Emplace(Child.IndexInRealNodes);
	}
	
	return rr;
}

TArray<int32> UFunctionTools_GraphWeaver::ObtainAllChildNodeInfo(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo)
{
	auto& Des = *NodeInfo;
	TArray<int32> rr;
	if (Des.IndexInRealNodes >= SourceGraphView->RealNodes.Num())[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllChildDes"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllChildDes"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName);
		return rr;
	}
	if (Des.IndexInRealNodes == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllChildDes"), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllChildDes"), *Des.ExplicitName);
		return rr;
	}
	if (!IsValid(SourceGraphView))[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("SourceGraphView is invalid.Func: ObtainAllChildDes"));
		UE_LOG(LogTemp, Error, TEXT("SourceGraphView is invalid.Func: ObtainAllChildDes"));
		return rr;
	}

	TArray<int32> ForkIndex;
	int32 IndexArrived = 0;
	ForkIndex.Emplace(Des.IndexInRealNodes);

	FMemMark Mar(FMemStack::Get());
	FMemStack& Stack = FMemStack::Get();

	uint8* RecordedHash = (uint8*)Stack.Alloc(SourceGraphView->RealNodes.Num() * sizeof(uint8), alignof(uint8));
	FMemory::Memzero(RecordedHash, SourceGraphView->RealNodes.Num() * sizeof(uint8));
	
	auto f = [&]()
	{
		auto& NowDes = SourceGraphView->RealNodes[ForkIndex[IndexArrived]];
		for (auto& Child : NowDes->Family.Children)
		{
			if (RecordedHash[Child.IndexInRealNodes] == 1)
				continue ;
			ForkIndex.Emplace(Child.IndexInRealNodes);
			rr.Emplace(Child.IndexInRealNodes);
			RecordedHash[Child.IndexInRealNodes] = 1;
		}
	};

	while (IndexArrived < ForkIndex.Num())
	{
		f();
		IndexArrived++;
	}
	return rr;
}

void UFunctionTools_GraphWeaver::FixupRanking(UGraphView* DisorderedView)
{
	auto& RealNodes = DisorderedView->RealNodes;
	
	for (auto& Des : RealNodes)
	{
		//Parent
		{
			for (int32 i = 0 ; i < Des->Family.Parents.Num(); i++)
			{
				for (auto& Child : RealNodes[Des->Family.Parents[i].IndexInRealNodes]->Family.Children)
				{
					if (Child.IndexInRealNodes == Des->IndexInRealNodes)
					{
						Child.Ranking = i;
						break ;
					}
				}
			}
		}
		//Bro
		{
			for (int32 i = 0 ; i < Des->Family.Brothers.Num(); i++)
			{
				for (auto& Bro : RealNodes[Des->Family.Brothers[i].IndexInRealNodes]->Family.Brothers)
				{
					if (Bro.IndexInRealNodes == Des->IndexInRealNodes)
					{
						Bro.Ranking = i;
						break ;
					}
				}
			}
		}
		//Child
		{
			for (int32 i = 0 ; i < Des->Family.Children.Num(); i++)
			{
				for (auto& Parent : RealNodes[Des->Family.Children[i].IndexInRealNodes]->Family.Parents)
				{
					if (Parent.IndexInRealNodes == Des->IndexInRealNodes)
					{
						Parent.Ranking = i;
						break ;
					}
				}
			}
		}
	}
}

void UFunctionTools_GraphWeaver::RemoveNodes(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo, bool RemoveChildren, bool ReorderRanking)
{
	auto& Des = *NodeInfo;
	auto& RealNodes = SourceGraphView->RealNodes;
	if (RemoveChildren)
	{
		TArray<int32> IndexToRemove;
		IndexToRemove.Emplace(Des.IndexInRealNodes);
		TArray<int32> ChildIndex = ObtainAllChildNodeInfo(SourceGraphView, NodeInfo);
		for (int32 Child : ChildIndex)
			IndexToRemove.Emplace(Child);
		
		Algo::Sort(IndexToRemove, [](int32 A, int32 B) {
			return A < B;  // 升序, 小数字在前
		});
		//上面收集信息
		//假设收集到了IndexToRemove = {1, 3, 5, 6, 9}  (0作为根不能被删除)
	
		int32 NodesNum = SourceGraphView->RealNodes.Num();

		//创建标记点
		FMemMark Mark(FMemStack::Get());
		// === 使用 FMemStack 分配临时内存 ===
		FMemStack& MemStack = FMemStack::Get();

		// DeletedHash[i] = 1 表示节点 i 被删除
		uint8* DeletedHash = (uint8*)MemStack.Alloc(NodesNum * sizeof(uint8), alignof(uint8));
		FMemory::Memzero(DeletedHash, NodesNum * sizeof(uint8));

		//数组用于哈希 ->  DeleteNum[i]表示从第一个元素到当前元素的上一个元素,期间一共有多少个元素被删除.即[0, x)
		uint64* DeleteNum = (uint64*)MemStack.Alloc(NodesNum * sizeof(uint64), alignof(uint64));
		DeleteNum[NodesNum - 1] = IndexToRemove.Num();

		int32 IndexArrived = IndexToRemove.Num() - 1;
	
		for (int32 Num = NodesNum - 1; Num >= 0; --Num)
		{
			if (IndexArrived == -1)
			{
				DeleteNum[Num] = 0;
				continue ;
			}
			if (Num != NodesNum - 1)
				DeleteNum[Num] = DeleteNum[Num + 1];
			
			if (Num == IndexToRemove[IndexArrived])
			{
				DeletedHash[Num] = 1;
				DeleteNum[Num]--;
				IndexArrived--;
				if (RealNodes[Num]->SourceGraphNode != nullptr)
					RealNodes[Num]->SourceGraphNode->IndexInRealNodes = -1;
				SourceGraphView->AddRemovedNodeName(RealNodes[Num]);
				//RealNodes.RemoveAt(Num);
			}
		}
	//0 1 2 3 4 5
	//  x   x   x
	//0 0 1 1 2 2 -> DeleteNum
		auto f = [&](TArray<FKinInfo_GraphWeaver>& Relationship)
		{
			for (int32 i = Relationship.Num() - 1; i >= 0; i--)
			{
				if (i >= 1 && HasSSE41())
				{
					_mm_prefetch((char*)DeletedHash[Relationship[i - 1].IndexInRealNodes], _MM_HINT_T0);
					_mm_prefetch((char*)DeleteNum[Relationship[i - 1].IndexInRealNodes], _MM_HINT_T0);
				}
				if (DeletedHash[Relationship[i].IndexInRealNodes] == 1)
				{
					Relationship.RemoveAt(i);
					continue ;
				}
				Relationship[i].IndexInRealNodes = Relationship[i].IndexInRealNodes - DeleteNum[Relationship[i].IndexInRealNodes];
			}
		};
		
		for (auto& PerDes : RealNodes)
		{
			if (DeletedHash[PerDes->IndexInRealNodes] == 1)
			{
				for (auto& Pair : PerDes->Family.Brothers)
				{
					if (DeletedHash[Pair.IndexInRealNodes] == 1)
						continue ;
					auto Bro = RealNodes[Pair.IndexInRealNodes];
					Bro->NamesInputNodeMirror.BroNames.Emplace(PerDes->NamesInputNodeMirror.SelfName);
					SourceGraphView->WillHorizontalAwakeNode.Emplace(Bro->IndexInRealNodes - DeleteNum[Bro->IndexInRealNodes]);
				}
				continue ;
			}
			PerDes->IndexInRealNodes = PerDes->IndexInRealNodes - DeleteNum[PerDes->IndexInRealNodes];
			if (PerDes->SourceGraphNode != nullptr)
				PerDes->SourceGraphNode->IndexInRealNodes = PerDes->IndexInRealNodes;
			f(PerDes->Family.Children);
			f(PerDes->Family.Parents);
			f(PerDes->Family.Brothers);
		}
		IndexArrived = IndexToRemove.Num() - 1;
		for (int i = RealNodes.Num() - 1; i >= 0 && IndexArrived >= 0; i--)
		{
			if (i == IndexToRemove[IndexArrived])
			{
				RealNodes.RemoveAt(i);
				IndexArrived--;
			}
		}
		if (ReorderRanking)
			FixupRanking(SourceGraphView);
		//修复SourceGraphView::Clans::Indices的错误指向
		if (SourceGraphView->ConstructConfig.NamingOfRules == true)
		{
			for (auto& Clan : SourceGraphView->Clans)
			{
				for (int32 i = Clan.Indices.Num() - 1; i >= 0; i--)
				{
					if (DeletedHash[Clan.Indices[i]] == 1)
					{
						Clan.Indices.RemoveAt(i);
						continue ;
					}
					Clan.Indices[i] -= DeleteNum[Clan.Indices[i]];
				}
			}
		}
		//修复WillVer和WillHor
		for (int32 i = SourceGraphView->WillVerticalAwakeNode.Num() - 1; i >= 0; i--)
		{
			if (i >= 1 && HasSSE41())
			{
				_mm_prefetch((char*)&DeletedHash[SourceGraphView->WillVerticalAwakeNode[i - 1]], _MM_HINT_T0);
				_mm_prefetch((char*)&DeleteNum[SourceGraphView->WillVerticalAwakeNode[i - 1]], _MM_HINT_T0);
			}
			if (DeletedHash[SourceGraphView->WillVerticalAwakeNode[i]] == 1)
			{
				SourceGraphView->WillVerticalAwakeNode.RemoveAtSwap(i);
				continue ;
			}
			SourceGraphView->WillVerticalAwakeNode[i] -= DeleteNum[SourceGraphView->WillVerticalAwakeNode[i]];
		}
		for (int32 i = SourceGraphView->WillHorizontalAwakeNode.Num() - 1; i >= 0; i--)
		{
			if (i >= 1 && HasSSE41())
			{
				_mm_prefetch((char*)&DeletedHash[SourceGraphView->WillHorizontalAwakeNode[i - 1]], _MM_HINT_T0);
				_mm_prefetch((char*)&DeleteNum[SourceGraphView->WillHorizontalAwakeNode[i - 1]], _MM_HINT_T0);
			}
			if (DeletedHash[SourceGraphView->WillHorizontalAwakeNode[i]] == 1)
			{
				SourceGraphView->WillHorizontalAwakeNode.RemoveAtSwap(i);
				continue ;
			}
			SourceGraphView->WillHorizontalAwakeNode[i] -= DeleteNum[SourceGraphView->WillHorizontalAwakeNode[i]];
		}
		return ;
	}//if(RemoveChildren)

	//if(!RemoveChildren)
	{
		//强制开启RecordedRanking,否则下一次调用该函数且RemoveChild为false的时候该函数会错误运行甚至报错
		//if(RecordedRanking)
		if (!SourceGraphView->ValidateRankingConsistencyLight(TArray<int32>{}))
			FixupRanking(SourceGraphView);
		//修复WillVer和WillHor
		for (int32 i = 0 ; i < SourceGraphView->WillVerticalAwakeNode.Num() ; i++)
		{
			if (SourceGraphView->WillVerticalAwakeNode[i] > Des.IndexInRealNodes)
			{
				SourceGraphView->WillVerticalAwakeNode[i]--;
				continue ;
			}
			if (SourceGraphView->WillVerticalAwakeNode[i] == Des.IndexInRealNodes)
			{
				SourceGraphView->WillVerticalAwakeNode.RemoveAtSwap(i);
				i--;
			}
		}
		for (int32 i = 0 ; i < SourceGraphView->WillHorizontalAwakeNode.Num() ; i++)
		{
			if (SourceGraphView->WillHorizontalAwakeNode[i] > Des.IndexInRealNodes)
			{
				SourceGraphView->WillHorizontalAwakeNode[i]--;
				continue ;
			}
			if (SourceGraphView->WillHorizontalAwakeNode[i] == Des.IndexInRealNodes)
			{
				SourceGraphView->WillHorizontalAwakeNode.RemoveAtSwap(i);
				i--;
			}
		}
		{
			for (auto& Pair : Des.Family.Parents)
			{
				auto Parent = RealNodes[Pair.IndexInRealNodes].Get();
				if (Pair.Ranking == (Parent->Family.Children.Num() - 1))
				{
					Parent->Family.Children.RemoveAt(Pair.Ranking);
					continue ;
				}
				for (int32 i = Pair.Ranking + 1 ; i < Parent->Family.Children.Num() ; i++)
				{
					RealNodes[Parent->Family.Children[i].IndexInRealNodes]->Family.Parents[Parent->Family.Children[i].Ranking].Ranking--;
				}
				Parent->Family.Children.RemoveAt(Pair.Ranking);
			}
			for (auto& Pair : Des.Family.Children)
			{
				auto Child = RealNodes[Pair.IndexInRealNodes].Get();
				Child->NamesInputNodeMirror.ParentNames.Emplace(Des.NamesInputNodeMirror.SelfName);
				if (Pair.IndexInRealNodes > Des.IndexInRealNodes)
					SourceGraphView->WillVerticalAwakeNode.Emplace(Pair.IndexInRealNodes - 1);
				else
					SourceGraphView->WillVerticalAwakeNode.Emplace(Pair.IndexInRealNodes);
				if (Pair.Ranking == Child->Family.Parents.Num() - 1)
				{
					Child->Family.Parents.RemoveAt(Pair.Ranking);
					continue ;
				}
				for (int32 i = Pair.Ranking + 1 ; i < Child->Family.Parents.Num() ; i++)
				{
					RealNodes[Child->Family.Parents[i].IndexInRealNodes]->Family.Children[Child->Family.Parents[i].Ranking].Ranking--;
				}
				Child->Family.Parents.RemoveAt(Pair.Ranking);
			}
			for (auto& Pair : Des.Family.Brothers)
			{
				auto Bro = RealNodes[Pair.IndexInRealNodes].Get();
				Bro->NamesInputNodeMirror.BroNames.Emplace(Des.NamesInputNodeMirror.SelfName);
				if (Pair.IndexInRealNodes > Des.IndexInRealNodes)
					SourceGraphView->WillHorizontalAwakeNode.Emplace(Pair.IndexInRealNodes - 1);
				else
					SourceGraphView->WillHorizontalAwakeNode.Emplace(Pair.IndexInRealNodes);
				if (Pair.Ranking == Bro->Family.Brothers.Num() - 1)
				{
					Bro->Family.Brothers.RemoveAt(Pair.Ranking);
					continue ;
				}
				for (int32 i = Pair.Ranking + 1 ; i < Bro->Family.Brothers.Num() ; i++)
				{
					RealNodes[Bro->Family.Brothers[i].IndexInRealNodes]->Family.Brothers[Bro->Family.Brothers[i].Ranking].Ranking--;
				}
				Bro->Family.Brothers.RemoveAt(Pair.Ranking);
			}
			//上面三个for循环只是处理了和被移除节点相关节点的相关Family的Ranking,不处理IndexInRealNodes
			if (Des.SourceGraphNode != nullptr)
				Des.SourceGraphNode->IndexInRealNodes = -1;

			for (int32 i = Des.IndexInRealNodes + 1 ; i < RealNodes.Num() ; i++)
			{
				RealNodes[i]->IndexInRealNodes--;
				if (RealNodes[i]->SourceGraphNode != nullptr)
					RealNodes[i]->SourceGraphNode->IndexInRealNodes--;
				for (auto& Parent : RealNodes[i]->Family.Parents)
				{
					if (Parent.IndexInRealNodes >= Des.IndexInRealNodes && Parent.IndexInRealNodes < i)
						RealNodes[Parent.IndexInRealNodes + 1]->Family.Children[Parent.Ranking].IndexInRealNodes--;
					else
						RealNodes[Parent.IndexInRealNodes]->Family.Children[Parent.Ranking].IndexInRealNodes--;
				}
				for (auto& Child : RealNodes[i]->Family.Children)
				{
					if (Child.IndexInRealNodes >= Des.IndexInRealNodes && Child.IndexInRealNodes < i)
						RealNodes[Child.IndexInRealNodes + 1]->Family.Parents[Child.Ranking].IndexInRealNodes--;
					else
						RealNodes[Child.IndexInRealNodes]->Family.Parents[Child.Ranking].IndexInRealNodes--;
				}
				for (auto& Bro : RealNodes[i]->Family.Brothers)
				{
					if (Bro.IndexInRealNodes >= Des.IndexInRealNodes && Bro.IndexInRealNodes < i)
						RealNodes[Bro.IndexInRealNodes + 1]->Family.Brothers[Bro.Ranking].IndexInRealNodes--;
					else
						RealNodes[Bro.IndexInRealNodes]->Family.Brothers[Bro.Ranking].IndexInRealNodes--;
				}
			}
			//修复SourceGraphView::Clans::Indexs的错误指向
			if (SourceGraphView->ConstructConfig.NamingOfRules == true)
			{
				for (auto& Clan : SourceGraphView->Clans)
				{
					for (int32 i = Clan.Indices.Num() - 1; i >= 0; i--)
					{
						if (Clan.Indices[i] == Des.IndexInRealNodes)
						{
							Clan.Indices.RemoveAt(i);
							continue ;
						}
						if (Clan.Indices[i] > Des.IndexInRealNodes)
							Clan.Indices[i]--;
					}
				}
			}
			SourceGraphView->AddRemovedNodeName(NodeInfo);
			RealNodes.RemoveAt(Des.IndexInRealNodes);
		}
	}
}

UGraphView* UFunctionTools_GraphWeaver::GetViewAndIndexFromNode(UGraphNode* Target, int32& Index)
{
	Index = Target->IndexInRealNodes;
	return Target->SourceGraphView;
}






void UFunctionForUK2Node::NonFunction()
{
	
}

TArray<int32> UFunctionForUK2Node::GetEmptyIntArray()
{
	return TArray<int32>{};
}

FString UFunctionForUK2Node::GetClassCPPNameFromDefaultObject(const FString& InName)
{
	FString rr = "U";
	int32 i = 0;
	for ( ; i < InName.Len(); i++)
	{
		if (i < DefaultObjectNamePrefix)
			continue;
		rr += InName[i];
	}
	return rr;
}

UGraphView* UFunctionForUK2Node::SpawnViewAndSetBasicProperty(UObject* Outer, FString GraphViewName,
                                                              NAWayToDealSameGraphNode::EWayToDealSameGraphNode WayToDealSameNode, UClass* RealNodesType, UClass* DCWrapperType,
                                                              int32 AllocateSize, FConstructConfig ConstructConfig)
{
	auto View = NewObject<UGraphView>(Outer, UGraphView::StaticClass());
	RealRunView::Get().AddView(View);
	View->Owner = Outer;
	View->GraphViewName = GraphViewName;
	View->WayToDealSameNode = WayToDealSameNode;
	View->RealNodesType = RealNodesType;
	View->AllocateGraphViewSize(AllocateSize);
	View->ConstructConfig = ConstructConfig;
	return View;
}

UGraphNode* UFunctionForUK2Node::CallProcessInformAuto(UGraphNode* Target, bool Call)
{
	if (!Call)[[unlikely]]
		return Target;
	Target->ProcessInformAuto(Target->SourceGraphView);
	return Target;
}

UGraphNode* UFunctionForUK2Node::SpawnNodeAndSetBasicProperty(UObject* Outer, FNamesInputNode& NamesInput, FString ViewName)
{
	auto Node = NewObject<UGraphNode>(Outer, UGraphNode::StaticClass());
	Node->ExplicitName = Outer->GetName() + "_GraphNode";
	Node->NamesInput = NamesInput;
	const auto& Array = RealRunView::Get().GetArray();
	for (auto View : Array)
	{
		if (View->GraphViewName == ViewName)
		{
			Node->SourceGraphView = View;
			break ;
		}
	}
	return Node;
}


/*
bool UFunctionTools::CanBePlacedInLevel(const UClass* Class)
{
	if (!Class)
		return false;
    
	// 1. 必须是AActor或其子类（只有Actor能放在关卡中）
	if (!Class->IsChildOf(AActor::StaticClass()))
		return false;
    
	// 2. 不能是抽象类（无法实例化）
	if (Class->HasAnyClassFlags(CLASS_Abstract))
		return false;
    
	// 3. 不能明确标记为不可放置
	if (Class->HasAnyClassFlags(CLASS_NotPlaceable))
		return false;
    
	// 4. 关键：满足"拖拽蓝图类型"要求
	//    这样会自动排除纯C++类，即使它们技术上可放置
	if (!Cast<UBlueprintGeneratedClass>(Class))
		return false;
    
	return true;
}
*/
