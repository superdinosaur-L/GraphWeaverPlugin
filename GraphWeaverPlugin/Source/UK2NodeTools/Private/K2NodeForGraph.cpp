// Copyright 2026 RainButterfly. All Rights Reserved.

#include "K2NodeForGraph.h"
#include "GraphNode.h"
#include "Engine/Engine.h"
#include <functional>
#include "Math/RandomStream.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "FunctionTools.h"
#include "GraphView.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "KismetCompiler.h"
#include "K2Node_MakeStruct.h"
#include "Editor.h"  // 用于GEditor
#include "GUIDClass.h"
#include "EdGraph/EdGraphPin.h"

FString UK2Node_SpawnGraphView::GenerateRandomString(int32 Length)
{
	const FString ValidChars = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    
	// 创建随机数生成器
	FRandomStream RandomStream;
	RandomStream.GenerateNewSeed(); // 生成新的随机种子
    
	FString Result;
	Result.Reserve(Length); // 预分配内存提高性能
    
	for (int32 i = 0; i < Length; ++i)
	{
		// 从有效字符中随机选择一个
		int32 RandomIndex = RandomStream.RandHelper(ValidChars.Len());
		Result.AppendChar(ValidChars[RandomIndex]);
	}
    
	return Result;
}

std::suspend_never FEditorLinkerTask::promise_type::initial_suspend() noexcept
{
	return {};
}

std::suspend_never FEditorLinkerTask::promise_type::final_suspend() noexcept
{
	return {};
}

FEditorLinkerTask FEditorLinkerTask::promise_type::get_return_object() noexcept
{
	return {};
}

void FEditorLinkerTask::promise_type::return_void() noexcept
{
	
}

void FEditorLinkerTask::promise_type::unhandled_exception() noexcept
{
	UE_LOG(LogTemp, Error, TEXT("Coroutine exception occurred."));
	WAITING_MOD_LOG_UK2NODE();
}



bool FPollAwaiter::IsConditionMet(std::shared_ptr<PollState> State)
{
	UK2Node_SpawnGraphView* View = State->WeakOwner.Get();
	if (!View || !IsValid(View))
		return false;

	UBlueprint* Blueprint = View->GetBlueprint();
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("等待蓝图初始化..."));
		return false;
	}

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (auto Graph : AllGraphs)
	{
		if (Graph->Nodes.Contains(View))
			return true;
	}
	
	return false;
}

void FPollAwaiter::StartPolling(std::shared_ptr<PollState> State, float ElapsedTime)
{
	//如果执行完了寻找任务或者还没有在Blueprint->Nodes里面找到目标View，目标View就直接被删除了，此时不需要再执行协程函数了
	if (!State->bIsActive || !State->WeakOwner.IsValid())
		return ;

	//更新剩余时间
	State->ElapsedTime = ElapsedTime;

	//如果在Nodes里找到了目标View或者直接用户电脑运行太慢，超过了最大运行上限时间，就直接恢复协程执行最后一个阶段
	if (IsConditionMet(State) || State->ElapsedTime >= State->TimeoutSeconds)
	{
		State->bIsActive = false;
		AsyncTask(ENamedThreads::GameThread,
			[State]()
			{
				//检查Handle是否还有效，Handle控制的任务是否已经完成
				if (State->Handle && !State->Handle.done())
					State->Handle.resume();
			});
		return ;
	}

	//使用编辑器全局定时器
	if (UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr ; EditorWorld)
	{
		FTimerManager& TimerManager = EditorWorld->GetTimerManager();
		//设置定时器
		TimerManager.SetTimer(State->TimerHandle,
			[State, NextElapsedTime = ElapsedTime + 0.1f]() mutable 
			{
				AsyncTask(ENamedThreads::GameThread,
					[State, NextElapsedTime]()
					{
						StartPolling(State, NextElapsedTime);
					});
			},
			0.1f,
			false
			);
	}
}

bool FPollAwaiter::await_ready() noexcept
{
	return TimeoutSeconds <= 0.f;
}

void FPollAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
{
	// 创建共享状态对象，延续生命周期
	auto SharedState = std::make_shared<PollState>();
	SharedState->Handle = handle;
	SharedState->TimeoutSeconds = TimeoutSeconds;
	SharedState->WeakOwner = WeakOwner;
	SharedState->bIsActive = bIsActive.load();
    	
	StartPolling(SharedState, 0.0f);
}

void FPollAwaiter::await_resume() noexcept
{
}



// 添加节点（线程安全）
void AllGraphViewArray::AddView_OSS(UK2Node_SpawnGraphView* View)
{
	if (!IsValid(View))[[unlikely]]  // 安全检查
		return ;
	int32 Index = Views.Emplace(View);
	View->IndexInViewArray = Index;
}

void AllGraphViewArray::RemoveView_OSS(UK2Node_SpawnGraphView* View)
{
	if (View->IndexInViewArray == Views.Num() - 1)
	{
		Views.RemoveAt(Views.Num() - 1);
		return ;
	}
	Views[Views.Num() - 1]->IndexInViewArray = View->IndexInViewArray;
	Views.RemoveAtSwap(View->IndexInViewArray);
}

UK2Node_SpawnGraphView* AllGraphViewArray::FindViewByCommonName(const FString& Name) const
{
	for (auto View : Views)
	{
		if (IsValid(View.Get()) && View->GetHiddenViewNamePin()->DefaultValue == Name)
		{
			return View.Get();
		}
	}
    
	return nullptr;
}

void AllGraphViewArray::UpdateAllViewIndex()
{
	for (int32 Index = 0 ; Index < Views.Num(); ++Index)
	{
		Views[Index]->IndexInViewArray = Index;
	}
}


#define LLOCTEXT_NAMESPACE "GraphKN"


bool UK2Node_SpawnGraphView::CheckSelfIsValid()
{
	//由于AboutToDie只有在Destroy里面会被设置为true，所以不需要检查
	return !(NameSameAsOtherView == 1 || BeCopied == 1);
}

UEdGraphPin* UK2Node_SpawnGraphView::GetNamesConfigPin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName.ToString() == PinNameHelper.ConstructConfig && Pin->Direction == EGPD_Input)
			return Pin;
	}
	return nullptr;
}


UEdGraphPin* UK2Node_SpawnGraphView::GetReturnValuePin() const
{
	for (auto Pin: Pins)
	{
		if (Pin->PinName.ToString() == PinNameHelper.ReturnValue && Pin->Direction == EGPD_Output)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphView::GetExplicitViewNamePin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName.ToString() == PinNameHelper.ExplicitViewName && Pin->Direction == EGPD_Input)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphView::GetHiddenViewNamePin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName.ToString() == PinNameHelper.HiddenViewName)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphView::GetWrapperTypePin() const
{
	for (auto Pin : Pins)
		if (Pin->PinName.ToString() == PinNameHelper.DCWrapperType)
			return Pin;
	return nullptr;
}

TArray<UK2Node_SpawnGraphNode*> UK2Node_SpawnGraphView::GetRealSpawnNodes()
{
	TArray<UK2Node_SpawnGraphNode*> rr;
	rr.Reserve(ChildNodeIndex.Num());
	auto& Nodes = AllGraphNodeArray::Get().GetNodes();
	for (int32 ChildIndex : ChildNodeIndex)
	{
		rr.Emplace(Nodes[ChildIndex].Get());
	}
	return rr;
}

TArray<UK2Node_GetDCStruct*> UK2Node_SpawnGraphView::GetRealGetDCNodes()
{
	TArray<UK2Node_GetDCStruct*> rr;
	rr.Reserve(GetDCStructIndex.Num());
	for (auto Index : GetDCStructIndex)
	{
		rr.Emplace(AllGetDCStructArray::Get().GetArray()[Index].Get());
	}
	return rr;
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphView::GetDefaultPins(bool IncludeSubPins)
{
	TArray<UEdGraphPin*> rr;
	rr.Reserve(DefaultPinsName.Num());
	for (auto Pin : Pins)
	{
		for (auto& PinName : DefaultPinsName)
		{
			if (Pin->PinName.ToString() == PinName)
			{
				rr.Emplace(Pin);
				if (IncludeSubPins)
				{
					for (auto SubPin : Pin->SubPins)
						rr.Emplace(SubPin);
				}
				break ;
			}
		}
	}
	return rr;
}


void UK2Node_SpawnGraphView::UpdateHiddenViewNameForThisNode()
{
	FString& Name = GetExplicitViewNamePin()->DefaultValue;
	if (Name.Len() == 0)
	{
		FString PreStr;
		uint8 Find = 0;
		do
		{
			PreStr  = "AutoGeneration_" + GenerateRandomString(5);
			Find = 0;
			for (auto& View : AllGraphViewArray::Get().GetAllViews())
			{
				if (View.Get() != this && View->GetHiddenViewNamePin()->DefaultValue == PreStr)
				{
					Find = 1;
					break ;
				}
			}
		}while (Find == 1);
		GetHiddenViewNamePin()->DefaultValue = PreStr;
		return ;
	}
	GetHiddenViewNamePin()->DefaultValue = "Manually_" + Name;
}


TArray<UK2Node_SpawnGraphNode*> UK2Node_SpawnGraphView::FindWaitChildNodeByCommonName_OSS()
{
	TArray<UK2Node_SpawnGraphNode*> rr;
	if (GetExplicitViewNamePin()->DefaultValue.IsEmpty())
		return rr;
	for (int32 ChildIndex = AllWaitNodeArray::Get().GetAllNodes().Num() - 1 ; ChildIndex >= 0; --ChildIndex)
	{
		auto Child = AllWaitNodeArray::Get().GetAllNodes()[ChildIndex];
		if (Child->IndexValueOfGetViewWay != 0)//  != Name
			continue ;

		if (Child->GetExplicitViewNamePin()->DefaultValue == GetExplicitViewNamePin()->DefaultValue)
		{
			ChildNodeIndex.Emplace(Child->IndexInAllNodeArray);
			Child->IndexOfSourceGraphView = IndexInViewArray;
			AllWaitNodeArray::Get().RemoveNode_OSS(ChildIndex);
			Child->BuildSecondPins();
			Child->FixUpSecondPins();
			Child->GetBlueprint()->Status = BS_Dirty;
			Child->GetGraph()->NotifyNodeChanged(Child.Get());
			rr.Emplace(Child.Get());
		}
	}
	return rr;
}

TArray<UK2Node_GetDCStruct*> UK2Node_SpawnGraphView::FindWaitChildDCByCommonName_OSS()
{
	TArray<UK2Node_GetDCStruct*> rr;
	//不允许DCStruct通过去匹配HiddenName来建立连接
	if (GetExplicitViewNamePin()->DefaultValue.Len() == 0)
		return rr;
	auto& DCArray = AllWaitDCStructArray::Get().GetArray();
	for (int32 Index = DCArray.Num() - 1 ; Index >= 0 ; --Index)
	{
		auto DC = DCArray[Index];
		if (DC->IndexValueOfGetViewWay != 0)// != Name
			continue ;

		if (DC->GetViewNamePin()->DefaultValue == GetExplicitViewNamePin()->DefaultValue)
		{
			rr.Emplace(DC.Get());
			DC->GetDCWrapperPin()->DefaultObject = GetWrapperTypePin()->DefaultObject;
			DC->BuildReturnPinsByDCWrapperPins();
			GetDCStructIndex.Emplace(DC->IndexInArray);
			DC->IndexOfSourceGraphView = IndexInViewArray;
			AllWaitDCStructArray::Get().RemoveDC_OSS(DC.Get());
			DC->GetBlueprint()->Status = BS_Dirty;
			DC->GetGraph()->NotifyNodeChanged(DC.Get());
		}
	}
	return rr;
}


TArray<UK2Node_SpawnGraphNode*> UK2Node_SpawnGraphView::FindWaitNodeChildByLink()
{
	TArray<UK2Node_SpawnGraphNode*> rr;
	for (int32 ChildIndex = AllWaitNodeArray::Get().GetAllNodes().Num() - 1 ; ChildIndex >= 0; --ChildIndex)
	{
		auto Child = AllWaitNodeArray::Get().GetAllNodes()[ChildIndex];
		if (UEdGraphPin* ViewInPin = Child->GetViewInPin(); ViewInPin)
		{
			UBlueprint* ViewOuterBlueprint = Cast<UBlueprint>(ViewInPin->DefaultObject);
			if (ViewOuterBlueprint == GetBlueprint())
			{
				ChildNodeIndex.Emplace(Child->IndexInAllNodeArray);
				Child->IndexOfSourceGraphView = IndexInViewArray;
				Child->GetHiddenViewNamePin()->DefaultValue = GetHiddenViewNamePin()->DefaultValue;
				AllWaitNodeArray::Get().RemoveNode_OSS(ChildIndex);
				rr.Emplace(Child.Get());
				Child->BuildSecondPins();
				Child->FixUpSecondPins();
				Child->GetBlueprint()->Status = BS_Dirty;
				Child->GetGraph()->NotifyNodeChanged(Child.Get());
			}
		}
	}
	return rr;
}

TArray<UK2Node_GetDCStruct*> UK2Node_SpawnGraphView::FindWaitDCChildByLink()
{
	TArray<UK2Node_GetDCStruct*> rr;
	for (int32 DCIndex = AllWaitDCStructArray::Get().GetArray().Num() - 1 ; DCIndex >= 0; --DCIndex)
	{
		auto DC = AllWaitDCStructArray::Get().GetArray()[DCIndex];
		if (UEdGraphPin* ViewInPin = DC->GetViewInPin())
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(ViewInPin->DefaultObject);
			if (Blueprint == GetBlueprint())
			{
				DC->Modify();
				rr.Emplace(DC.Get());
				GetDCStructIndex.Emplace(DC->IndexInArray);
				DC->IndexOfSourceGraphView = IndexInViewArray;
				AllWaitDCStructArray::Get().RemoveDC_OSS(DC.Get());
				DC->UpdateDCWrapperPinValueByGraphView();
				DC->BuildReturnPinsByDCWrapperPins();
				DC->GetBlueprint()->Status = BS_Dirty;
				DC->GetGraph()->NotifyNodeChanged(DC.Get());
			}
		}
	}
	return rr;
}


TArray<UK2Node_SpawnGraphNode*> UK2Node_SpawnGraphView::EmptyChildNode_Name()
{
	TArray<UK2Node_SpawnGraphNode*> rr;
	for (int32 ChildIndex = ChildNodeIndex.Num() - 1; ChildIndex >= 0; --ChildIndex)
	{
		auto Child = AllGraphNodeArray::Get().GetNodes()[ChildNodeIndex[ChildIndex]].Get();
		if (Child->IndexValueOfGetViewWay != 0)//  != Name
			continue ;
		//Name
		{
			{
				Child->Modify();
				Child->UpdateDefaultPins();
				Child->UpdateFirstPins();
				Child->UpdateSecondPins();
				Child->BreakLinkedToAllPins();
				Child->ErrorMsg.Reset();
				Child->Pins.Reset();
				Child->AllocateDefaultPins();
				Child->FixUpDefaultPins();
				Child->BuildFirstPins();
				Child->FixUpFirstPins();
				Child->GetHiddenViewNamePin()->DefaultValue.Reset();
				Child->IndexOfSourceGraphView = -1;
				AllWaitNodeArray::Get().AddNewNode_OSS(Child);
				Child->GetBlueprint()->Status = BS_Dirty;
				Child->GetGraph()->NotifyNodeChanged(Child);
			}
			ChildNodeIndex.RemoveAt(ChildIndex);
			rr.Emplace(Child);
		}
	}
	return rr;
}

void UK2Node_SpawnGraphView::EmptyChildDC_Name()
{
	for (int32 DCIndex = GetDCStructIndex.Num() - 1; DCIndex >= 0; --DCIndex)
	{
		auto DC = AllGetDCStructArray::Get().GetArray()[DCIndex];
		if (DC->IndexValueOfGetViewWay != 0)// != Name
			continue ;
		DC->Modify();
		DC->UpdateDefaultPins();
		DC->UpdateFirstPins();
		DC->UpdateGraphViewPin();
		DC->BreakAllLinkedTo();
		DC->Pins.Reset();
		DC->ErrorMsg.Reset();
		DC->AllocateDefaultPins();
		DC->FixupDefaultPins();
		DC->BuildFirstPins();
		DC->FixupFirstPins();
		DC->BuildGraphViewPin();
		DC->FixupGraphViewPin();
		DC->IndexOfSourceGraphView = -1;
		AllWaitDCStructArray::Get().AddNewElem_OSS(DC.Get());
		DC->GetDCWrapperPin()->DefaultObject = nullptr;
		DC->GetBlueprint()->Status = BS_Dirty;
		DC->GetGraph()->NotifyNodeChanged(DC.Get());
		GetDCStructIndex.RemoveAt(DCIndex);
	}
}

void UK2Node_SpawnGraphView::EmptyAllChild()
{
	auto& NodeManager = AllGraphNodeArray::Get();
	for (int32 NodeIndex = ChildNodeIndex.Num() - 1; NodeIndex >= 0; --NodeIndex)
	{
		auto Node = NodeManager.GetNodes()[ChildNodeIndex[NodeIndex]];
		if (Node->IndexValueOfGetViewWay == 0)//Name
		{
			Node->Modify();
			Node->UpdateDefaultPins();
			Node->UpdateFirstPins();
			Node->UpdateSecondPins();
			Node->BreakLinkedToAllPins();
			Node->ErrorMsg.Reset();
			Node->Pins.Reset();
			Node->AllocateDefaultPins();
			Node->FixUpDefaultPins();
			Node->BuildFirstPins();
			Node->FixUpFirstPins();
			Node->GetHiddenViewNamePin()->DefaultValue.Reset();
			AllWaitNodeArray::Get().AddNewNode_OSS(Node.Get());
			Node->IndexOfSourceGraphView = -1;
			Node->GetBlueprint()->Status = BS_Dirty;
			Node->GetGraph()->NotifyNodeChanged(Node.Get());
			continue ;
		}
		if (Node->IndexValueOfGetViewWay == 1)//Link
		{
			Node->Modify();
			Node->UpdateDefaultPins();
			Node->UpdateFirstPins();
			Node->UpdateSecondPins();
			Node->BreakLinkedToAllPins();
			Node->ErrorMsg.Reset();
			Node->Pins.Reset();
			Node->AllocateDefaultPins();
			Node->FixUpDefaultPins();
			Node->BuildFirstPins();
			Node->FixUpFirstPins();
			Node->IndexOfSourceGraphView = -1;//放在UpdateSourceView之前来简化流程
			Node->UpdateSourceView();
			if (Node->IndexOfSourceGraphView != -1)
			{
				Node->BuildSecondPins();
				Node->FixUpSecondPins();
			}
			Node->GetBlueprint()->Status = BS_Dirty;
			Node->GetGraph()->NotifyNodeChanged(Node.Get());
			continue ;
		}
		WAITING_MOD_LOG_UK2NODE();
	}
	ChildNodeIndex.Reset();

	auto& DCStructManager = AllGetDCStructArray::Get();
	for (int32 DCStructIndex = GetDCStructIndex.Num() - 1; DCStructIndex >= 0; --DCStructIndex)
	{
		auto DCStruct = DCStructManager.GetArray()[DCStructIndex].Get();
		if (DCStruct->IndexValueOfGetViewWay == 0)//Name
		{
			DCStruct->Modify();
			DCStruct->UpdateDefaultPins();
			DCStruct->UpdateFirstPins();
			DCStruct->UpdateGraphViewPin();
			DCStruct->BreakAllLinkedTo();
			DCStruct->Pins.Reset();
			DCStruct->ErrorMsg.Reset();
			DCStruct->AllocateDefaultPins();
			DCStruct->FixupDefaultPins();
			DCStruct->BuildFirstPins();
			DCStruct->FixupFirstPins();
			DCStruct->BuildGraphViewPin();
			DCStruct->FixupGraphViewPin();
			DCStruct->GetDCWrapperPin()->DefaultObject = nullptr;
			DCStruct->IndexOfSourceGraphView = -1;
			AllWaitDCStructArray::Get().AddNewElem_OSS(DCStruct);
			DCStruct->GetBlueprint()->Status = BS_Dirty;
			DCStruct->GetGraph()->NotifyNodeChanged(DCStruct);
			continue ;
		}
		if (DCStruct->IndexValueOfGetViewWay == 1)//Link
		{
			DCStruct->Modify();
			DCStruct->UpdateDefaultPins();
			DCStruct->UpdateFirstPins();
			DCStruct->UpdateGraphViewPin();
			DCStruct->BreakAllLinkedTo();
			DCStruct->Pins.Reset();
			DCStruct->ErrorMsg.Reset();
			DCStruct->AllocateDefaultPins();
			DCStruct->FixupDefaultPins();
			DCStruct->BuildFirstPins();
			DCStruct->FixupFirstPins();
			DCStruct->BuildGraphViewPin();
			DCStruct->FixupGraphViewPin();
			DCStruct->IndexOfSourceGraphView = -1;//减少UpdateSourceGraphView里面的操作步骤
			DCStruct->UpdateSourceView();
			DCStruct->UpdateDCWrapperPinValueByGraphView();
			DCStruct->BuildReturnPinsByDCWrapperPins();
			DCStruct->GetBlueprint()->Status = BS_Dirty;
			DCStruct->GetGraph()->NotifyNodeChanged(DCStruct);
			continue ;
		}
		WAITING_MOD_LOG_UK2NODE();
	}
}

FEditorLinkerTask UK2Node_SpawnGraphView::DelayLinkChild_Link()
{
	double WaitTime = MaxWaitTime;
	if (WaitTime <= 0.2f)
		WaitTime = 0.2f;
	//由于放置View的一瞬间，蓝图的Nodes里面还没有记录该节点，需要等待记录之后再执行
	FPollAwaiter WaitViewJoinInBlueprint(WaitTime, this);
	co_await WaitViewJoinInBlueprint;
	// 延迟结束后，安全执行查找
	FindWaitNodeChildByLink();
	FindWaitDCChildByLink();
	co_return;
}


void UK2Node_SpawnGraphView::AllocateDefaultPins()
{
	auto K2Schema = GetDefault<UEdGraphSchema_K2>();
	DefaultPinsName.Empty();
	
	auto ExecPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	DefaultPinsName.Emplace(ExecPin->PinName.ToString());
	
	auto ThenPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	DefaultPinsName.Emplace(ThenPin->PinName.ToString());

	UEdGraphPin* ViewNamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, *PinNameHelper.ExplicitViewName);
	DefaultPinsName.Emplace(ViewNamePin->PinName.ToString());
	ViewNamePin->bNotConnectable = true;
	
	UEnum* DealSameNodeEnum = StaticEnum<NAWayToDealSameGraphNode::EWayToDealSameGraphNode>();
	UEdGraphPin* DealSameNodePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, DealSameNodeEnum, *PinNameHelper.WayToDealSameNode);
	DealSameNodePin->DefaultValue = DealSameNodeEnum->GetNameStringByIndex(IndexValueOfWayToDealSameNode);
	DefaultPinsName.Emplace(DealSameNodePin->PinName.ToString());

	auto RealNodesType = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Class, UNodeInfoBase_GraphWeaver::StaticClass(),
		*PinNameHelper.RealNodesType);
	RealNodesType->DefaultObject = UNodeInfoBase_GraphWeaver::StaticClass();
	DefaultPinsName.Emplace(RealNodesType->PinName.ToString());

	auto DCWrapper = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Class, UDCWrapper_GraphWeaver::StaticClass(),
		*PinNameHelper.DCWrapperType);
	DCWrapper->DefaultObject = UDCWrapperForDefaultStruct_GraphWeaver::StaticClass();
	DefaultPinsName.Emplace(DCWrapper->PinName.ToString());
	
	//创建一个隐藏的Pin来真正保存当前蓝图节点的GraphViewName
	UEdGraphPin* HiddenViewNamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, *PinNameHelper.HiddenViewName);
	DefaultPinsName.Emplace(HiddenViewNamePin->PinName.ToString());
	HiddenViewNamePin->DefaultValue = "";
	HiddenViewNamePin->bHidden = true;

	UEdGraphPin* AllocateSizePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, *PinNameHelper.RealNodesReserveSize);
	K2Schema->SetPinAutogeneratedDefaultValue(AllocateSizePin, TEXT("0"));
	DefaultPinsName.Emplace(AllocateSizePin->PinName.ToString());
	
	auto ReturnValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, UGraphView::StaticClass(),
		*PinNameHelper.ReturnValue);
	DefaultPinsName.Emplace(ReturnValuePin->PinName.ToString());

	auto ConfigPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FConstructConfig::StaticStruct(), *PinNameHelper.ConstructConfig);
	DefaultPinsName.Emplace(ConfigPin->PinName.ToString());
	
	if (ENodeAdvancedPins::NoPins == AdvancedPinDisplay)
	{
		AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
	}
}

void UK2Node_SpawnGraphView::PostReconstructNode()
{
	Super::PostReconstructNode();
}

void UK2Node_SpawnGraphView::PostPasteNode()
{
	Message_Error(TEXT("Creating a new 'SpawnGraphView' by copy-pasting is not allowed."));
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		OldExplicitName = GetExplicitViewNamePin()->DefaultValue;
		OldWrapperType = GetWrapperTypePin()->DefaultObject;
		return ;
	}
	BeCopied = 1;
}

void UK2Node_SpawnGraphView::ReconstructNode()
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		TArray<UEdGraphPin*> OldPins(Pins);
		Pins.Reset();
		ErrorMsg.Empty();
		AllocateDefaultPins();
		RestoreSplitPins(OldPins);
		RewireOldPinsToNewPins(OldPins, Pins, {});//只复刻不构建K2Node逻辑
		GetGraph()->NotifyNodeChanged(this);
		OldExplicitName = GetExplicitViewNamePin()->DefaultValue;
		OldWrapperType = GetWrapperTypePin()->DefaultObject;
		return ;
	}
	if (!CheckSelfIsValid())
	{
		return ;
	}
	
	TArray<UEdGraphPin*> OldPins(Pins);
	Modify();
	ErrorMsg.Reset();
	Pins.Reset();
	AllocateDefaultPins();
	RestoreSplitPins(OldPins);
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	RewireOldPinsToNewPins(OldPins, Pins, &NewPinsToOldPins);
	PostReconstructNode();
	if (HasAnyFlags(RF_Transactional))
	{
		AllGraphViewArray::Get().AddView_OSS(this);
	}
	GetGraph()->NotifyNodeChanged(this);
	
	//我真棒！💪✨ (๑•̀ㅂ•́)و✧ (◍•ᴗ•◍)❤
}

void UK2Node_SpawnGraphView::PostPlacedNewNode()
{
	UserGuid = GetDefault<UGraphWeaverPerUserGuid>()->UserGuid;
	DelayLinkChild_Link();
	AllGraphViewArray::Get().AddView_OSS(this);
	UpdateHiddenViewNameForThisNode();
	//此时已经有Blueprint的父亲关系了，只是Blueprint的Nodes还没有把新创建的节点添加进来
}


FText UK2Node_SpawnGraphView::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("PreGraphKN" ,"Spawn", "SpawnGraphView");
}

FSlateIcon UK2Node_SpawnGraphView::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.SpawnActor_16x");
	return Icon;
}

FText UK2Node_SpawnGraphView::GetTooltipText() const
{
	return NSLOCTEXT("PreGraphKN", "NodeToolTip",
		"Nodes specifically designed for generating GraphView.\n "
		"Please do not create a new 'SpawnGraphView' by copying and pasting from another 'SpawnGraphView'. \n "
		"If you assign a non-empty string to 'GraphViewName', ensure it is unique and does not match the 'GraphViewName' \n"
		" of any previously created 'SpawnGraphView'.\n "
		"Warning! Do NOT use 'Ctrl + Z' to undo operations for this blueprint node. Since this blueprint involves communication between different blueprints, \n "
		"using this undo method will most likely cause chaos in the entire plugin functionality and result in malfunctions.");
}

void UK2Node_SpawnGraphView::DestroyNode()
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)//不是自己创建的,没有权力销毁
		return ;
	if (!CheckSelfIsValid())
	{      
		Super::DestroyNode();
		return ;
	}
	AboutToDie = true;
	AllGraphViewArray::Get().RemoveView_OSS(this);
	//更新被交换的SpawnGraphView的子项的IndexOfSourceGraphView
	if (IndexInViewArray != AllGraphViewArray::Get().GetAllViews().Num())
	{
		auto SwapedView = AllGraphViewArray::Get().GetAllViews()[IndexInViewArray];
		for (auto ChildIndex : SwapedView->ChildNodeIndex)
		{
			AllGraphNodeArray::Get().GetNodes()[ChildIndex]->IndexOfSourceGraphView = IndexInViewArray;
		}
		for (auto GetDCIndex : SwapedView->GetDCStructIndex)
		{
			AllGetDCStructArray::Get().GetArray()[GetDCIndex]->IndexOfSourceGraphView = IndexInViewArray;
		}
	}
	for (auto Child : GetRealSpawnNodes())
	{
		Child->Modify();
		Child->UpdateDefaultPins();
		Child->UpdateFirstPins();
		Child->UpdateSecondPins();
		Child->BreakLinkedToAllPins();
		Child->ErrorMsg.Reset();
		Child->Pins.Reset();
		Child->AllocateDefaultPins();
		Child->FixUpDefaultPins();
		Child->BuildFirstPins();
		Child->FixUpFirstPins();
		Child->IndexOfSourceGraphView = -1;
		if (Child->IndexValueOfGetViewWay == 1)//Link
		{
			//利用上面的修改为-1,可以取消UpdateSourceView里面最前面两行操作SourceGraphView的操作来节省性能
			Child->UpdateSourceView();
			if (Child->IndexOfSourceGraphView != -1)
			{
				Child->BuildSecondPins();
				Child->FixUpSecondPins();
				Child->GetBlueprint()->Status = BS_Dirty;
				Child->GetGraph()->NotifyNodeChanged(Child);
			}
		}
		else if (Child->IndexValueOfGetViewWay == 0)//Name
		{
			AllWaitNodeArray::Get().AddNewNode_OSS(Child);
			Child->GetHiddenViewNamePin()->DefaultValue.Reset();
			Child->GetBlueprint()->Status = BS_Dirty;
			Child->GetGraph()->NotifyNodeChanged(Child);
		}
		else
			WAITING_MOD_LOG_UK2NODE();
	}
	for (auto DC : GetRealGetDCNodes())
	{
		DC->Modify();
		DC->GetDCWrapperPin()->DefaultObject = nullptr;
		DC->UpdateDefaultPins();
		DC->UpdateFirstPins();
		DC->UpdateGraphViewPin();
		DC->BreakAllLinkedTo();
		DC->Pins.Reset();
		DC->AllocateDefaultPins();
		DC->FixupDefaultPins();
		DC->BuildFirstPins();
		DC->FixupFirstPins();
		DC->BuildGraphViewPin();
		DC->FixupGraphViewPin();
		DC->IndexOfSourceGraphView = -1;
		if (DC->IndexValueOfGetViewWay == 1)// == Link
		{
			DC->UpdateSourceView();
			DC->UpdateDCWrapperPinValueByGraphView();
			DC->BuildReturnPinsByDCWrapperPins();
		}
		else if (DC->IndexValueOfGetViewWay == 0)// == Name
		{
			AllWaitDCStructArray::Get().AddNewElem_OSS(DC);
		}
		else
			WAITING_MOD_LOG_UK2NODE();
		DC->GetBlueprint()->Status = BS_Dirty;
		DC->GetGraph()->NotifyNodeChanged(DC);
	}
	
	Super::DestroyNode();
}

void UK2Node_SpawnGraphView::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (BeCopied == 1)
	{
		MessageLog.Error(TEXT("@@: Creating a new 'SpawnGraphView' by copy-pasting is not allowed."), this);
	}
	if (NameSameAsOtherView == 1)
	{
		FString ErrorMessage = "@@: The 'GraphViewName' of the current node is or has been identical to that of another node.\n "
						 "The current node should be deleted directly and replaced with a newly created 'SpawnGraphView' blueprint node.";
		MessageLog.Error(*ErrorMessage, this);
	}
}

void UK2Node_SpawnGraphView::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	const UEdGraphSchema_K2* SchemaHelper = GetDefault<UEdGraphSchema_K2>();
	if (UEdGraphPin* GraphViewNamePin = GetExplicitViewNamePin())
	{
		SchemaHelper->ConstructBasicPinTooltip(*GraphViewNamePin,
			NSLOCTEXT("PreGraphKN", "GraphViewPinToolTip", "The name you want to give this GraphView. If you don't specify a name, one will be generated automatically."),
			GraphViewNamePin->PinToolTip);
	}

	if (UEdGraphPin* NamingOfRulesPin = FindPin(*(PinNameHelper.ConstructConfig + PinNameHelper.ConstructConfig_NamingOfRules), EGPD_Input))
		SchemaHelper->ConstructBasicPinTooltip(*NamingOfRulesPin,
			NSLOCTEXT("PreGraphKN", "NamingOfRulesPinTooltip", "If you want to use the Names method for construction but do not wish to follow a structured \n"
													  "naming convention, set this option to false to reduce memory usage; otherwise, set it to true. "
													  "\nImproper configuration may significantly degrade construction performance. "
													  "\nNote: This option only takes effect when using the Names construction method."),
													  NamingOfRulesPin->PinToolTip);

	if (UEdGraphPin* PrecisionPin = FindPin(*(PinNameHelper.ConstructConfig + PinNameHelper.ConstructConfig_Precision), EGPD_Input))
		SchemaHelper->ConstructBasicPinTooltip(*PrecisionPin,
		NSLOCTEXT("PreGraphKN", "PrecisionPinTooltip", "How many characters at the beginning of a name define a clan (family).\n"
													"For example:\n"
													"If you have names like AA2, AA3, BB2, BB3, then the Precision value should be 2.\n"
													"If you have AA1 and AB2, the Precision can be either 1 (using just A) or 2 (using AA and AB).\n"
													"Note: This option only takes effect when the construction method is set to Names and NamingOfRules is true.\n"
													"If this value exceeds the length of a given name, the entire name will be used.\n"
													"If the value is less than 1, it defaults to 1."),
		PrecisionPin->PinToolTip);

	if (UEdGraphPin* AllocatePin = FindPinChecked(*PinNameHelper.RealNodesReserveSize))
		SchemaHelper->ConstructBasicPinTooltip(*AllocatePin,
		NSLOCTEXT("PreGraphKN", "AllocatePinToolTip", "The approximate number of 'GraphNode's you intend to include in this 'GraphView'. \n "
												   "For example, if you plan to add 10 'GraphNode's (excluding the root node inherent to 'GraphView') to this 'GraphView', \n "
													"you can specify 10 or a larger capacity. This allows the graph construction process to be slightly faster."),
													AllocatePin->PinToolTip);

	if (UEdGraphPin* WayToDealSameNodePin = FindPinChecked(*PinNameHelper.WayToDealSameNode))
		SchemaHelper->ConstructBasicPinTooltip(*WayToDealSameNodePin,
			NSLOCTEXT("PreGraphKn", "WayToDealSameNodePinToolTip", "How do you want to be notified when a 'GraphNode' is added to the 'GraphView' multiple times?"),
													WayToDealSameNodePin->PinToolTip);
	
	Super::GetPinHoverText(Pin, HoverTextOut);
}




FText UK2Node_SpawnGraphView::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

void UK2Node_SpawnGraphView::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UK2Node_SpawnGraphView::ShouldShowNodeProperties() const
{
	return true;
}


void UK2Node_SpawnGraphView::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		//应该恢复原值
		if (Pin == GetExplicitViewNamePin())
			Pin->DefaultValue = OldExplicitName;
		
		if (Pin == GetWrapperTypePin())
			Pin->DefaultObject = OldWrapperType;
		
		if (Pin == FindPinChecked(PinNameHelper.WayToDealSameNode))
		{
			UEnum* WayToDealSameNodeEnum = StaticEnum<NAWayToDealSameGraphNode::EWayToDealSameGraphNode>();
			if (IndexValueOfWayToDealSameNode >= 0)
				Pin->DefaultValue = WayToDealSameNodeEnum->GetNameStringByIndex(IndexValueOfWayToDealSameNode);
			else
				Pin->DefaultValue = WayToDealSameNodeEnum->GetNameStringByIndex(0);
		}
		return ;
	}
	if (!CheckSelfIsValid())
		return ;
	if (Pin == GetExplicitViewNamePin())
	{
		if (Pin->DefaultValue.Len() > 0)
		{
			for (auto& ElView : AllGraphViewArray::Get().GetAllViews())
			{
				if (ElView.Get() != this)[[likely]]
				{
					if (ElView->GetExplicitViewNamePin()->DefaultValue == Pin->DefaultValue)[[unlikely]]
					{
						NameSameAsOtherView = 1;
						GetHiddenViewNamePin()->DefaultValue.Reset();
						EmptyAllChild();
						AllGraphViewArray::Get().RemoveView_OSS(this);
						if (IndexInViewArray != AllGraphViewArray::Get().GetAllViews().Num())
						{
							auto SwapedView = AllGraphViewArray::Get().GetAllViews()[IndexInViewArray].Get();
							for (auto PerChildNodeIndex : SwapedView->ChildNodeIndex)
							{
								AllGraphNodeArray::Get().GetNodes()[PerChildNodeIndex]->IndexOfSourceGraphView = IndexInViewArray;
							}
							for (auto GetDCStructChildIndex : SwapedView->GetDCStructIndex)
							{
								AllGetDCStructArray::Get().GetArray()[GetDCStructChildIndex]->IndexOfSourceGraphView = IndexInViewArray;
							}
						}
						return ;
					}
				}
			}
		}
		
		UpdateHiddenViewNameForThisNode();
		
		EmptyChildNode_Name();
		EmptyChildDC_Name();
		
		FindWaitChildNodeByCommonName_OSS();
		for (auto Child : GetRealSpawnNodes())
		{
			Child->GetHiddenViewNamePin()->DefaultValue = GetHiddenViewNamePin()->DefaultValue;
			Child->GetBlueprint()->Status = EBlueprintStatus::BS_Dirty;
			Child->GetGraph()->NotifyNodeChanged(Child);
		}
		
		//DC
		FindWaitChildDCByCommonName_OSS();
		return ;
	}
	if (Pin == GetWrapperTypePin())
	{
		for (auto DC : GetRealGetDCNodes())
		{
			DC->Modify();
			DC->UpdateDefaultPins();
			DC->UpdateFirstPins();
			DC->UpdateGraphViewPin();
			DC->Pins.Reset();
			DC->ErrorMsg.Reset();
			DC->AllocateDefaultPins();
			DC->FixupDefaultPins();
			DC->BuildFirstPins();
			DC->FixupFirstPins();
			DC->BuildGraphViewPin();
			DC->FixupGraphViewPin();
			DC->UpdateDCWrapperPinValueByGraphView();
			DC->BuildReturnPinsByDCWrapperPins();
			DC->GetBlueprint()->Status = BS_Dirty;
			DC->GetGraph()->NotifyNodeChanged(DC);
		}
		return ;
	}
	if (Pin == FindPinChecked(*PinNameHelper.WayToDealSameNode))
	{
		Modify();
		IndexValueOfWayToDealSameNode = StaticEnum<NAWayToDealSameGraphNode::EWayToDealSameGraphNode>()->GetIndexByNameString(Pin->DefaultValue);
		GetGraph()->NotifyNodeChanged(this);
	}
}



void UK2Node_SpawnGraphView::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	if (!CheckSelfIsValid())[[unlikely]]
	{
		UK2Node_CallFunction* Non = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		Non->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, NonFunction), UFunctionForUK2Node::StaticClass());
		Non->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *Non->GetExecPin());
		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *Non->GetThenPin());

		BreakAllNodeLinks();
		return ;
	}

#if 0
	{
		UK2Node_CallFunction* NonFun = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		NonFun->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, NonFunction), UFunctionForUK2Node::StaticClass());
		NonFun->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *NonFun->GetExecPin());
		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *NonFun->GetThenPin());
		BreakAllNodeLinks();
		return ;
	}
#endif
	UK2Node_Self* self = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
	self->AllocateDefaultPins();

	UK2Node_CallFunction* SpawnViewAndSet = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	SpawnViewAndSet->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, SpawnViewAndSetBasicProperty), UFunctionForUK2Node::StaticClass());
	SpawnViewAndSet->AllocateDefaultPins();

	self->FindPinChecked(TEXT("Self"))->MakeLinkTo(SpawnViewAndSet->FindPinChecked(TEXT("Outer")));

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *SpawnViewAndSet->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetHiddenViewNamePin(), *SpawnViewAndSet->FindPinChecked(TEXT("GraphViewName")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.WayToDealSameNode), *SpawnViewAndSet->FindPinChecked(TEXT("WayToDealSameNode")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.RealNodesType), *SpawnViewAndSet->FindPinChecked(TEXT("RealNodesType")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.DCWrapperType), *SpawnViewAndSet->FindPinChecked(TEXT("DCWrapperType")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.RealNodesReserveSize), *SpawnViewAndSet->FindPinChecked(TEXT("AllocateSize")));
	auto ConstructConfigPin = FindPinChecked(PinNameHelper.ConstructConfig);
	if (ConstructConfigPin->SubPins.Num() > 0)
	{
		UK2Node_MakeStruct* ConfigStruct = CompilerContext.SpawnIntermediateNode<UK2Node_MakeStruct>(this, SourceGraph);
		ConfigStruct->StructType = FConstructConfig::StaticStruct();
		ConfigStruct->bMadeAfterOverridePinRemoval = true;
		ConfigStruct->AllocateDefaultPins();
		CompilerContext.MovePinLinksToIntermediate(
			*FindPinChecked(PinNameHelper.ConstructConfig + "_" + PinNameHelper.ConstructConfig_NamingOfRules),
			*ConfigStruct->FindPinChecked(PinNameHelper.ConstructConfig_NamingOfRules));
		CompilerContext.MovePinLinksToIntermediate(
			*FindPinChecked(PinNameHelper.ConstructConfig + "_" + PinNameHelper.ConstructConfig_Precision),
			*ConfigStruct->FindPinChecked(PinNameHelper.ConstructConfig_Precision));
		ConfigStruct->FindPinChecked(TEXT("ConstructConfig"))->MakeLinkTo(SpawnViewAndSet->FindPinChecked(TEXT("ConstructConfig")));
	}
	else
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.ConstructConfig), *SpawnViewAndSet->FindPinChecked(TEXT("ConstructConfig")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.ReturnValue), *SpawnViewAndSet->GetReturnValuePin());
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *SpawnViewAndSet->GetThenPin());
	BreakAllNodeLinks();
}


void UK2Node_SpawnGraphView::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
}

#undef LLOCTEXT_NAMESPACE









#define LLOCTEXT_NAMESPACE "GraphNodeKN"


bool UK2Node_SpawnGraphNode::CheckViewIsValid(UK2Node_SpawnGraphView* View)
{
	return !(View->NameSameAsOtherView == 1 || View->BeCopied == 1 || View->AboutToDie);
}

UEdGraphPin* UK2Node_SpawnGraphNode::GetExplicitViewNamePin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName.ToString() == PinNameHelper.SourceViewName)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphNode::GetHiddenViewNamePin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName.ToString() == PinNameHelper.SourceViewHiddenName)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphNode::GetViewInPin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName.ToString() == PinNameHelper.SourceViewInBlueprint)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphNode::GetFindViewWayPin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName.ToString() == PinNameHelper.GetViewWay)
			return Pin;
	}
	return nullptr;
}

UK2Node_SpawnGraphView* UK2Node_SpawnGraphNode::GetRealSpawnView()
{
	return AllGraphViewArray::Get().GetAllViews()[IndexOfSourceGraphView].Get();
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphNode::GetDefaultPins()
{
	TArray<UEdGraphPin*> rr;
	for (auto& Name : PinsDefault)
	{
		for (auto Pin : Pins)
		{
			if (Pin->PinName.ToString() == Name)
			{
				rr.Emplace(Pin);
				break ;
			}
		}
	}
	return rr;
}

void UK2Node_SpawnGraphNode::UpdateDefaultPins()
{
	PinsDefault_Ptr = GetDefaultPins();
}

void UK2Node_SpawnGraphNode::FixUpDefaultPins()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	RewireOldPinsToNewPins(PinsDefault_Ptr, Pins, &NewPinsToOldPins);
}

void UK2Node_SpawnGraphNode::BuildFirstPins()
{
	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			PinsFirst_Name.Empty();
			UEdGraphPin* ViewNamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, *PinNameHelper.SourceViewName);
			ViewNamePin->bNotConnectable = true;
			PinsFirst_Name.Emplace(ViewNamePin->PinName.ToString());
		}
		break ;
	case 1://Link
		{
			PinsFirst_Link.Empty();
			UEdGraphPin* ViewFamilyPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object,
				UBlueprint::StaticClass(), *PinNameHelper.SourceViewInBlueprint);
			ViewFamilyPin->bNotConnectable = true;
			PinsFirst_Link.Emplace(ViewFamilyPin->PinName.ToString());
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphNode::GetPinsFirst(bool IncludeSubPins)
{
	TArray<UEdGraphPin*> rr;

	auto f = [&](const TArray<FString>& Array)
	{
		for (auto& Name : Array)
		{
			for (auto Pin : Pins)
			{
				if (Pin->PinName.ToString() == Name)
				{
					rr.Emplace(Pin);
					break ;
				}
			}
		}
		if (IncludeSubPins)
		{
			int32 Num = Array.Num();
			for (int32 Index = 0 ; Index < Num ; ++Index)
			{
				for (auto SubPin : rr[Index]->SubPins)
					rr.Emplace(SubPin);
			}
		}
	};

	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			f(PinsFirst_Name);
		}
		break ;
	case 1:
		{
			f(PinsFirst_Link);
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	
	return rr;
}

void UK2Node_SpawnGraphNode::UpdateFirstPins()
{
	
	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			PinsFirst_Name_Ptr = GetPinsFirst();
		}
		break ;
	case 1://Link
		{
			PinsFirst_Link_Ptr = GetPinsFirst();
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_SpawnGraphNode::FixUpFirstPins()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	switch (IndexValueOfGetViewWay)
	{
	case 0:
		{
			RestoreSplitPins(PinsFirst_Name_Ptr);
			RewireOldPinsToNewPins(PinsFirst_Name_Ptr, Pins, &NewPinsToOldPins);
			break ;
		}
	case 1:
		{
			RestoreSplitPins(PinsFirst_Link_Ptr);
			RewireOldPinsToNewPins(PinsFirst_Link_Ptr, Pins, &NewPinsToOldPins);
			break ;
		}
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_SpawnGraphNode::UpdateSourceView()
{
	if (IndexOfSourceGraphView != -1)
		AllGraphViewArray::Get().GetAllViews()[IndexOfSourceGraphView]->ChildNodeIndex.RemoveSingleSwap(IndexInAllNodeArray);
	IndexOfSourceGraphView = -1;

	uint8 Find = 0;
	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			if (GetExplicitViewNamePin()->DefaultValue.IsEmpty())
				break ;
			
			for (auto View : AllGraphViewArray::Get().GetAllViews())
			{
				if (View->GetExplicitViewNamePin()->DefaultValue == GetExplicitViewNamePin()->DefaultValue)
				{
					if (!CheckViewIsValid(View.Get()))[[unlikely]]
						continue ;
					IndexOfSourceGraphView = View->IndexInViewArray;
					View->ChildNodeIndex.Emplace(IndexInAllNodeArray);
					GetHiddenViewNamePin()->DefaultValue = View->GetHiddenViewNamePin()->DefaultValue;
					if (IndexInWaitNodeArray != -1)
						AllWaitNodeArray::Get().RemoveNode_OSS(this);
					Find = 1;
					break ;
				}
			}
		}
		break ;
	case 1:
		{
			if (UBlueprint* Family = Cast<UBlueprint>(GetViewInPin()->DefaultObject) ; Family)
			{
				TArray<UEdGraph*> AllGraphs;
				Family->GetAllGraphs(AllGraphs);
				for (auto Graph : AllGraphs)
				{
					for (auto Node : Graph->Nodes)
					{
						if (UK2Node_SpawnGraphView* TargetView = Cast<UK2Node_SpawnGraphView>(Node) ; TargetView)
						{
							if (!CheckViewIsValid(TargetView))[[unlikely]]
								continue ;
							TargetView->ChildNodeIndex.Emplace(IndexInAllNodeArray);
							IndexOfSourceGraphView = TargetView->IndexInViewArray;
							GetHiddenViewNamePin()->DefaultValue = TargetView->GetHiddenViewNamePin()->DefaultValue;
							if (IndexInWaitNodeArray != -1)
								AllWaitNodeArray::Get().RemoveNode_OSS(this);
							Find = 1;
							break ;
						}
					}
					if (Find)
						break ;
				}
			}
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	if (!Find)
	{
		if (IndexInWaitNodeArray == -1)
		{
			AllWaitNodeArray::Get().AddNewNode_OSS(this);
		}
		GetHiddenViewNamePin()->DefaultValue.Reset();
	}
}

void UK2Node_SpawnGraphNode::BuildSecondPins()
{
	PinsSecond_NamesConfig.Empty();
	UEdGraphPin* NamesInputPin =
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FNamesInputNode::StaticStruct(), *PinNameHelper.NamesInput);
	PinsSecond_NamesConfig.Emplace(NamesInputPin->PinName.ToString());
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphNode::GetPinsSecond()
{
	TArray<UEdGraphPin*> rr;
	std::function<void(TArray<FString>&)> f = [&](TArray<FString>& Array)
	{
		for (auto& Name : Array)
		{
			for (auto Pin : Pins)
			{
				if (Pin->PinName.ToString() == Name)
				{
					rr.Emplace(Pin);
					break ;
				}
			}
		}
		int32 NumPins = rr.Num();
		for (int32 Index = 0 ; Index < NumPins ; ++Index)
		{
			for (auto SubPin : rr[Index]->SubPins)
				rr.Emplace(SubPin);
		}
	};


	f(PinsSecond_NamesConfig);
	
	return rr;
}

void UK2Node_SpawnGraphNode::UpdateSecondPins()
{
	PinsSecond_NamesConfig_Ptr = GetPinsSecond();
}

void UK2Node_SpawnGraphNode::FixUpSecondPins()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	
	RestoreSplitPins(PinsSecond_NamesConfig_Ptr);
	RewireOldPinsToNewPins(PinsSecond_NamesConfig_Ptr, Pins, &NewPinsToOldPins);
}

void UK2Node_SpawnGraphNode::BreakLinkedToSecondPins()
{
	for (auto Pin : GetPinsSecond())
	{
		for (auto LinkedPin : Pin->LinkedTo)
		{
			LinkedPin->LinkedTo.RemoveSingle(Pin);
			LinkedPin->GetOwningNode()->GetGraph()->NotifyNodeChanged(LinkedPin->GetOwningNode());
		}
	}
}

void UK2Node_SpawnGraphNode::BreakLinkedToAllPins()
{
	for (auto Pin : Pins)
	{
		for (auto LinkedPin : Pin->LinkedTo)
		{
			LinkedPin->LinkedTo.RemoveSingle(Pin);
			LinkedPin->GetOwningNode()->GetGraph()->NotifyNodeChanged(LinkedPin->GetOwningNode());
		}
	}
}

void UK2Node_SpawnGraphNode::AllocateDefaultPins()
{
	PinsDefault.Empty();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	
	UEdGraphPin* ExecPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	PinsDefault.Emplace(ExecPin->PinName.ToString());

	UEdGraphPin* ThenPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	PinsDefault.Emplace(ThenPin->PinName.ToString());

	UEnum* GetViewWay = StaticEnum<NAGetGraphViewWay::EGetGraphViewWay>();
	UEdGraphPin* GetViewWayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, GetViewWay, *PinNameHelper.GetViewWay);
	PinsDefault.Emplace(GetViewWayPin->PinName.ToString());
	GetViewWayPin->DefaultValue = GetViewWay->GetNameStringByIndex(IndexValueOfGetViewWay);
	GetViewWayPin->bNotConnectable = true;

	UEdGraphPin* AutoBuildPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, *PinNameHelper.AutoBuild);
	PinsDefault.Emplace(AutoBuildPin->PinName.ToString());
	AutoBuildPin->bHidden = true;
	Schema->SetPinAutogeneratedDefaultValue(AutoBuildPin, AutoBuild ? TEXT("true") : TEXT("false"));

	UEdGraphPin* GraphNodePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object,
		UGraphNode::StaticClass(), *PinNameHelper.ReturnValue);
	PinsDefault.Emplace(GraphNodePin->PinName.ToString());

	UEdGraphPin* RealViewNameHiddenPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, *PinNameHelper.SourceViewHiddenName);
	PinsDefault.Emplace(RealViewNameHiddenPin->PinName.ToString());
	RealViewNameHiddenPin->bHidden = true;
}


void UK2Node_SpawnGraphNode::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		if (Pin->PinName.ToString() == *PinNameHelper.GetViewWay)
		{
			UEnum* E = StaticEnum<NAGetGraphViewWay::EGetGraphViewWay>();
			Pin->DefaultValue = E->GetNameStringByIndex(IndexValueOfGetViewWay);
		}
		if (Pin == GetExplicitViewNamePin())
			Pin->DefaultValue = OldExplicitViewName;
		
		if (Pin == GetViewInPin())
			Pin->DefaultObject = OldViewIn;
		return ;
	}
	if (Pin->PinName.ToString() == *PinNameHelper.GetViewWay)
	{
		Modify();
		UpdateDefaultPins();
		UpdateFirstPins();
		if (IndexOfSourceGraphView != -1)
			UpdateSecondPins();
		BreakLinkedToAllPins();
		{
			IndexValueOfGetViewWay = StaticEnum<NAGetGraphViewWay::EGetGraphViewWay>()->GetIndexByNameString(Pin->DefaultValue);
		}
		Pins.Reset();
		ErrorMsg.Reset();
		AllocateDefaultPins();
		FixUpDefaultPins();
		BuildFirstPins();
		FixUpFirstPins();
		UpdateSourceView();
		if (IndexOfSourceGraphView != -1)
		{
			BuildSecondPins();
			FixUpSecondPins();
		}
		GetGraph()->NotifyNodeChanged(this);
		return ;
	}
	if (Pin == GetExplicitViewNamePin() || Pin == GetViewInPin())
	{
		Modify();
		UpdateDefaultPins();
		UpdateFirstPins();
		if (IndexOfSourceGraphView != -1)
			UpdateSecondPins();
		BreakLinkedToAllPins();
		Pins.Reset();
		ErrorMsg.Reset();
		AllocateDefaultPins();
		FixUpDefaultPins();
		BuildFirstPins();
		FixUpFirstPins();
		UpdateSourceView();
		if (IndexOfSourceGraphView != -1)
		{
			BuildSecondPins();
			FixUpSecondPins();
		}
		GetGraph()->NotifyNodeChanged(this);
		return ;
	}
}

void UK2Node_SpawnGraphNode::DestroyNode()
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
		return ;
	if (IndexInWaitNodeArray != -1)
	{
		AllWaitNodeArray::Get().RemoveNode_OSS(this);
	}
 	int32 OldLastIndex = AllGraphNodeArray::Get().RemoveNode_OSS(this);
	if (IndexOfSourceGraphView != -1)
	{
		GetRealSpawnView()->ChildNodeIndex.RemoveSingleSwap(IndexInAllNodeArray);
	}
	bool SwapedNodeHasParent = false;
	if (OldLastIndex != -1 && AllGraphNodeArray::Get().GetNodes()[IndexInAllNodeArray].Get()->IndexInWaitNodeArray == -1)
		SwapedNodeHasParent = true;
	if (SwapedNodeHasParent)
	{
		auto View =
			AllGraphViewArray::Get().GetAllViews()[AllGraphNodeArray::Get().GetNodes()[IndexInAllNodeArray].Get()->IndexOfSourceGraphView];
		for (auto& ChildIndex : View->ChildNodeIndex)
		{
			if (ChildIndex == OldLastIndex)
			{
				ChildIndex = IndexInAllNodeArray;
				break ;
			}
		}
	}
	Super::DestroyNode();
}

void UK2Node_SpawnGraphNode::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (IndexOfSourceGraphView == -1)[[unlikely]]
	{
		UK2Node_CallFunction* NonFun = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		NonFun->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, NonFunction), UFunctionForUK2Node::StaticClass());
		NonFun->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *NonFun->GetExecPin());
		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *NonFun->GetThenPin());
		BreakAllNodeLinks();
		return ;
	}
#if 1
	UK2Node_CallFunction* SpawnNodeAndSetBasicProperty = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	SpawnNodeAndSetBasicProperty->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, SpawnNodeAndSetBasicProperty), UFunctionForUK2Node::StaticClass());
	SpawnNodeAndSetBasicProperty->AllocateDefaultPins();
	
	UK2Node_Self* Self = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
	Self->AllocateDefaultPins();

	UK2Node_CallFunction* CallMakeGraph = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallMakeGraph->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, CallProcessInformAuto), UFunctionForUK2Node::StaticClass());
	CallMakeGraph->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *SpawnNodeAndSetBasicProperty->GetExecPin());
	if (FindPinChecked(PinNameHelper.NamesInput)->SubPins.Num() > 0)
	{
		UK2Node_MakeStruct* NamesInputStruct = CompilerContext.SpawnIntermediateNode<UK2Node_MakeStruct>(this, SourceGraph);
		NamesInputStruct->StructType = FNamesInputNode::StaticStruct();
		NamesInputStruct->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.NamesInput_SelfName),
			*NamesInputStruct->FindPinChecked(PinNameHelper.NamesInput + "_" + PinNameHelper.NamesInput_SelfName));
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.NamesInput_ParentNames),
			*NamesInputStruct->FindPinChecked(PinNameHelper.NamesInput + "_" + PinNameHelper.NamesInput_ParentNames));
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.NamesInput_BroNames),
			*NamesInputStruct->FindPinChecked(PinNameHelper.NamesInput + "_" + PinNameHelper.NamesInput_BroNames));
		NamesInputStruct->FindPinChecked(TEXT("NamesInputNode"))->MakeLinkTo(SpawnNodeAndSetBasicProperty->FindPinChecked(TEXT("NamesInput")));
	}
	else
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.NamesInput), *SpawnNodeAndSetBasicProperty->FindPinChecked(TEXT("NamesInput")));

	Self->FindPinChecked(TEXT("Self"))->MakeLinkTo(SpawnNodeAndSetBasicProperty->FindPinChecked(TEXT("Outer")));
	SpawnNodeAndSetBasicProperty->GetThenPin()->MakeLinkTo(CallMakeGraph->GetExecPin());
	SpawnNodeAndSetBasicProperty->GetReturnValuePin()->MakeLinkTo(CallMakeGraph->FindPinChecked(TEXT("Target")));
	CompilerContext.MovePinLinksToIntermediate(*GetHiddenViewNamePin(), *SpawnNodeAndSetBasicProperty->FindPinChecked(TEXT("ViewName")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.AutoBuild), *CallMakeGraph->FindPinChecked(TEXT("Call")));
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *CallMakeGraph->GetThenPin());
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.ReturnValue), *SpawnNodeAndSetBasicProperty->GetReturnValuePin());

	BreakAllNodeLinks();
#endif
}

void UK2Node_SpawnGraphNode::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == FindPinChecked(PinNameHelper.AutoBuild)->PinName)
	{
		// 查找 AutoBuild 引脚并更新其默认值
		if (UEdGraphPin* AutoBuildPin = FindPinChecked(PinNameHelper.AutoBuild))
		{
			AutoBuildPin->DefaultValue = AutoBuild ? TEXT("true") : TEXT("false");
		}
	}
}

void UK2Node_SpawnGraphNode::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (IndexOfSourceGraphView == -1)
	{
		// 抛出编译错误，@@会被替换为节点引用，可在编辑器中高亮
		MessageLog.Error(
			*NSLOCTEXT("SourceGraphViewMissing", "SourceGraphViewError","@@: 'SourceGraphView' cannot be null. Please specify a valid GraphView object.").ToString(),
			this  // 传入节点指针用于错误定位
		);
	}
}

void UK2Node_SpawnGraphNode::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (UEdGraphPin* NamesPin = FindPin(*PinNameHelper.NamesInput) ; NamesPin)
	{
		Schema->ConstructBasicPinTooltip(*NamesPin,
		NSLOCTEXT("NodePinText", "NamesInput", "For nodes that are siblings of each other, you only need to declare the sibling relationship in one direction—not both.\n"
										 "For example, if 'A1' and 'B1' are siblings, it is sufficient for either 'A1' to declare 'B1' as a sibling or for 'B1' to declare 'A1' as a sibling.\n"
										 "Do not have both 'A1' and 'B1' declare each other as siblings simultaneously, as this will cause relationship ambiguity or conflicts."),
			NamesPin->PinToolTip);
	}
	
	if (UEdGraphPin* NamesBroNamesPin = FindPin(*(PinNameHelper.NamesInput + "_BroNames")); NamesBroNamesPin)
	{
		Schema->ConstructBasicPinTooltip(*NamesBroNamesPin,
		NSLOCTEXT("NodePinText", "NamesInput", "For nodes that are siblings of each other, you only need to declare the sibling relationship in one direction—not both.\n"
										 "For example, if 'A1' and 'B1' are siblings, it is sufficient for either 'A1' to declare 'B1' as a sibling or for 'B1' to declare 'A1' as a sibling.\n"
										 "Do not have both 'A1' and 'B1' declare each other as siblings simultaneously, as this will cause relationship ambiguity or conflicts."),
			NamesBroNamesPin->PinToolTip);
	}

	if (UEdGraphPin* GetViewWayPin = FindPinChecked(*PinNameHelper.GetViewWay))
	{
		Schema->ConstructBasicPinTooltip(*GetViewWayPin,
		NSLOCTEXT("NodePinText", "GetViewWay", "The method used to locate the 'SpawnGraphView'.\n "
															"Note: If the value is set to 'Link', it will only find the first 'SpawnGraphView' in the corresponding Blueprint. \n"
															"Therefore, if your Blueprint contains multiple 'SpawnGraphView' nodes, please switch to using the 'Name' method for identification instead."),
			GetViewWayPin->PinToolTip);
	}
	
	Super::GetPinHoverText(Pin, HoverTextOut);
}

void UK2Node_SpawnGraphNode::PostPlacedNewNode()
{
	UserGuid = GetDefault<UGraphWeaverPerUserGuid>()->UserGuid;
	AllGraphNodeArray::Get().AddNewNode_OSS(this);
	AllWaitNodeArray::Get().AddNewNode_OSS(this);
	BuildFirstPins();
}


void UK2Node_SpawnGraphNode::PostPasteNode()
{
	BeCopied = 1;
	if (auto Pin = FindPin(PinNameHelper.SourceViewName))
		OldExplicitViewName = Pin->DefaultValue;
	if (auto Pin = FindPin(PinNameHelper.SourceViewInBlueprint))
		OldViewIn = Pin->DefaultObject;
	
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		IndexValueOfGetViewWay = -1;
		GetHiddenViewNamePin()->DefaultValue.Reset();
		return ;
	}
	AllGraphNodeArray::Get().AddNewNode_OSS(this);
	if (IndexOfSourceGraphView != -1)
		GetRealSpawnView()->ChildNodeIndex.Emplace(IndexInAllNodeArray);
	else
	{
		AllWaitNodeArray::Get().AddNewNode_OSS(this);
	}
}


void UK2Node_SpawnGraphNode::ReconstructNode()
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		TArray<UEdGraphPin*> OldPins(Pins);
		bool HasSecondPin = false;
		if (GetPinsSecond().Num() > 0)
			HasSecondPin = true;
		Pins.Reset();
		ErrorMsg.Reset();
		AllocateDefaultPins();
		BuildFirstPins();
		if (HasSecondPin)
			BuildSecondPins();
		RestoreSplitPins(OldPins);
		RewireOldPinsToNewPins(OldPins, Pins, {});
		if (auto Pin = GetExplicitViewNamePin())
			OldExplicitViewName = Pin->DefaultValue;
		if (auto Pin = GetViewInPin())
			OldViewIn = Pin->DefaultObject;
		return ;
	}
	if (BeCopied == 0)
	{
		TArray<UEdGraphPin*> OldPins(Pins);
		Modify();
		ErrorMsg.Reset();
		Pins.Reset();
		AllocateDefaultPins();
		BuildFirstPins();
		BuildSecondPins();
		RestoreSplitPins(OldPins);
		TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
		RewireOldPinsToNewPins(OldPins, Pins, &NewPinsToOldPins);
		GetGraph()->NotifyNodeChanged(this);
		PostReconstructNode();

		//实际上只有本体在打开引擎或者复制粘贴一个新的节点的时候才会执行这个函数
		if (HasAnyFlags(RF_Transactional))
		{
			AllGraphNodeArray::Get().AddNewNode_OSS(this);
			AllWaitNodeArray::Get().AddNewNode_OSS(this);
		}
		return ;
	}
	if (BeCopied == 1)
	{
		PostReconstructNode();
	}
}

void UK2Node_SpawnGraphNode::PostLoad()
{
	Super::PostLoad();
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
		return ;
	if (HasAnyFlags(RF_Transient))
		return ;
	IndexOfSourceGraphView = -1;
	UpdateSourceView();
	if (IndexOfSourceGraphView != -1)
	{
		GetGraph()->NotifyNodeChanged(this);
		return ;
	}
	{
		UpdateDefaultPins();
		UpdateFirstPins();
		Pins.Reset();
		AllocateDefaultPins();
		FixUpDefaultPins();
		BuildFirstPins();
		FixUpFirstPins();
		GetBlueprint()->Status = BS_Dirty;
		GetGraph()->NotifyNodeChanged(this);
	}
}

void UK2Node_SpawnGraphNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_SpawnGraphNode::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

bool UK2Node_SpawnGraphNode::ShouldShowNodeProperties() const
{
	return true;
}

FText UK2Node_SpawnGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("PreGraphNodeKN", "NodeTitleKey", "SpawnGraphNode");
}


FSlateIcon UK2Node_SpawnGraphNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.SpawnActor_16x");
	return Icon;
}

FText UK2Node_SpawnGraphNode::GetTooltipText() const
{
	return NSLOCTEXT("PreGraphNodeKN", "GetTooltipText",
	"Nodes specifically designed for generating GraphView\n "
	"Warning! Do NOT use 'Ctrl + Z' to undo operations for this blueprint node. Since this blueprint involves communication between different blueprints, \n"
	"using this undo method will most likely cause chaos in the entire plugin functionality and result in malfunctions.");
}






#undef LLOCTEXT_NAMESPACE




#define LLOCTEXT_NAMESPACE "DCStructKN"


AllWaitNodeArray::AllWaitNodeArray()
{
	WaitNodeArray.Reserve(10);
}

AllWaitNodeArray::~AllWaitNodeArray()
{
	WaitNodeArray.Empty();
}

int32 AllGraphNodeArray::AddNewNode_OSS(UK2Node_SpawnGraphNode* NewNode)
{
	int32 Index = Nodes.Emplace(NewNode);
	NewNode->IndexInAllNodeArray = Index;
	return Index;
}

void AllGraphNodeArray::UpdateAllNodeIndex(int32 StartIndex)
{
	int32 Index = -1;
	auto& Array = Get().GetNodes();
	if (StartIndex == -1)
	{
		for (auto Node : Array)
		{
			Index++;
			Node.Get()->IndexInAllNodeArray = Index;
		}
		return ;
	}

	for ( Index = StartIndex; Index < Array.Num(); ++Index)
	{
		Array[Index].Get()->IndexInAllNodeArray = Index;
	}
}


AllGraphNodeArray::AllGraphNodeArray()
{
	Nodes.Empty();
}

bool UK2Node_GetDCStruct::CheckViewIsValid(UK2Node_SpawnGraphView* View)
{
	return !(View->NameSameAsOtherView == 1 || View->BeCopied == 1 || View->AboutToDie);
}

void UK2Node_GetDCStruct::UpdateSourceView()
{
	if (IndexOfSourceGraphView != -1)
		AllGraphViewArray::Get().GetAllViews()[IndexOfSourceGraphView]->GetDCStructIndex.RemoveSingleSwap(IndexInArray);
	IndexOfSourceGraphView = -1;
	uint8 Find = 0;
	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			if (GetViewNamePin()->DefaultValue.Len() == 0)
				break ;
		
			for (auto View : AllGraphViewArray::Get().GetAllViews())
			{
				if (View->GetExplicitViewNamePin()->DefaultValue == GetViewNamePin()->DefaultValue && CheckViewIsValid(View.Get()))
				{
					View->GetDCStructIndex.Emplace(IndexInArray);
					IndexOfSourceGraphView = View->IndexInViewArray;
					Find = 1;
					if (IndexInWaitArray != -1)
						AllWaitDCStructArray::Get().RemoveDC_OSS(this);
					break ;
				}
			}
		}
		break ;
	case 1://Link
		{
			UBlueprint* Blueprint = StaticCast<UBlueprint*>(GetViewInPin()->DefaultObject);
			if (Blueprint != nullptr)
			{
				TArray<UEdGraph*> Graphs;
				Blueprint->GetAllGraphs(Graphs);
				for (auto Graph : Graphs)
				{
					for (auto Node : Graph->Nodes)
					{
						if (auto View = Cast<UK2Node_SpawnGraphView>(Node))
						{
							if (!CheckViewIsValid(View))
								continue ;
							View->GetDCStructIndex.Emplace(IndexInArray);
							IndexOfSourceGraphView = View->IndexInViewArray;
							Find = 1;
							if (IndexInWaitArray != -1)
								AllWaitDCStructArray::Get().RemoveDC_OSS(this);
							break ;
						}
					}
					if (Find == 1)
						break ;
				}
			}
		}
		break;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	if (Find == 0 && IndexInWaitArray == -1)
		AllWaitDCStructArray::Get().AddNewElem_OSS(this);
}

void UK2Node_GetDCStruct::BuildFirstPins()
{
	switch (IndexValueOfGetViewWay)
	{
	case 0:
		{
			FirstPinsName_NameWay.Empty();
			auto ViewName = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, *PinNameHelper.GraphViewName);
			ViewName->bNotConnectable = true;
			FirstPinsName_NameWay.Emplace(ViewName->PinName.ToString());
		}
		break ;
	case 1:
		{
			FirstPinsName_LinkWay.Empty();
			auto ViewIn = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UBlueprint::StaticClass(), *PinNameHelper.GraphViewIn);
			ViewIn->bNotConnectable = true;
			FirstPinsName_LinkWay.Emplace(ViewIn->PinName.ToString());
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_GetDCStruct::BuildReturnPinsByDCWrapperPins()
{
	UClass* WrapperClassType = static_cast<UClass*>(GetDCWrapperPin()->DefaultObject);
	if (WrapperClassType == nullptr)
		return ;
	FCreatePinParams Params;
	Params.ContainerType = EPinContainerType::Array;
	
#if 0
	//检查是否是蓝图类型.如果是,需要另作处理
	if (WrapperClassType->IsChildOf(UDCWrapperForBlueprint::StaticClass()))
	{
		UScriptStruct* StructType = const_cast<UDCWrapperForBlueprint*>(GetDefault<UDCWrapperForBlueprint>(WrapperClassType))->DCStructType;
		if (StructType == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("StructType Is NULL.File: %s, Line: %d"), *FString(__FILE__), __LINE__)
			EMPTY_LOG_UK2NODE();
			return;
		}
		CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, StructType, *PinNameHelper.ReturnValue, Params);
		return ;
	}
#endif
	
	UScriptStruct* StructType = const_cast<UDCWrapper_GraphWeaver*>(GetDefault<UDCWrapper_GraphWeaver>(WrapperClassType))->GetStructType();
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, StructType, *PinNameHelper.ReturnValue, Params);
}

void UK2Node_GetDCStruct::BuildGraphViewPin()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UGraphView::StaticClass(), *PinNameHelper.GraphView);
}

TArray<UEdGraphPin*> UK2Node_GetDCStruct::GetDefaultPins()
{
	TArray<UEdGraphPin*> rr;
	rr.Reserve(DefaultPins.Num());
	for (auto Pin : Pins)
	{
		for (auto PinName : DefaultPinNames)
		{
			if (Pin->PinName.ToString() == PinName)
			{
				rr.Emplace(Pin);
				break ;
			}
		}
	}
	return rr;
}

TArray<UEdGraphPin*> UK2Node_GetDCStruct::GetFirstPins()
{
	TArray<UEdGraphPin*> rr;
	auto f = [&rr, this](TArray<FString> PinNames)
	{
		for (auto& PinName : PinNames)
		{
			for (auto Pin : Pins)
			{
				if (Pin->PinName.ToString() == PinName)
				{
					rr.Emplace(Pin);
					break ;
				}
			}
		}
	};
	
	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			f(FirstPinsName_NameWay);
		}
		break ;
	case 1:
		{
			f(FirstPinsName_LinkWay);
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	return rr;
}

TArray<UEdGraphPin*> UK2Node_GetDCStruct::GetDefaultAndFirstPins()
{
	TArray<UEdGraphPin*> rr;
	auto DefaultPins_Pre = GetDefaultPins();
	auto FirstPins_Pre = GetFirstPins();
	rr.Reserve(DefaultPins_Pre.Num() + FirstPins_Pre.Num());
	for (auto Pin : DefaultPins_Pre)
		rr.Emplace(Pin);
	for (auto Pin : FirstPins_Pre)
		rr.Emplace(Pin);
	return rr;
}

UEdGraphPin* UK2Node_GetDCStruct::GetGraphViewPin()
{
	for (auto Pin : Pins)
		if (Pin->PinName.ToString() == PinNameHelper.GraphView)
			return Pin;
	return nullptr;
}


void UK2Node_GetDCStruct::UpdateDefaultPins()
{
	DefaultPins = GetDefaultPins();
}

void UK2Node_GetDCStruct::UpdateFirstPins()
{
	switch (IndexValueOfGetViewWay)
	{
	case 0:
		{
			FirstPins_NameWay = GetFirstPins();
		}
		break ;
	case 1:
		{
			FirstPins_LinkWay = GetFirstPins();
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_GetDCStruct::UpdateGraphViewPin()
{
	GraphViewPin = GetGraphViewPin();
}

void UK2Node_GetDCStruct::FixupDefaultPins()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	RestoreSplitPins(DefaultPins);
	RewireOldPinsToNewPins(DefaultPins, Pins, &NewPinsToOldPins);
}

void UK2Node_GetDCStruct::FixupFirstPins()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	switch (IndexValueOfGetViewWay)
	{
	case 0:
		{
			RestoreSplitPins(FirstPins_NameWay);
			RewireOldPinsToNewPins(FirstPins_NameWay, Pins, &NewPinsToOldPins);
		}
		break ;
	case 1:
		{
			RestoreSplitPins(FirstPins_LinkWay);
			RewireOldPinsToNewPins(FirstPins_LinkWay, Pins, &NewPinsToOldPins);
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_GetDCStruct::FixupGraphViewPin()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	TArray<UEdGraphPin*> Pin{GraphViewPin};
	RewireOldPinsToNewPins(Pin, Pins, &NewPinsToOldPins);
}

void UK2Node_GetDCStruct::UpdateDCWrapperPinValueByGraphView()
{
	if (IndexOfSourceGraphView == -1)
	{
		GetDCWrapperPin()->DefaultObject = nullptr;
		return ;
	}
	{
		auto View = AllGraphViewArray::Get().GetAllViews()[IndexOfSourceGraphView];
		GetDCWrapperPin()->DefaultObject = View->GetWrapperTypePin()->DefaultObject;
	}
	//有时候有返回值Pin,但是DCWrapper值为空.这个时候需要重建蓝图节点以删除返回值Pin.通常是GraphView不小心选错了WrapperType导致的(可以选择WrapperType的值为空)
	//CheckReconsOrNot:
#if 0
	if (GetDCWrapperPin()->DefaultObject == nullptr)
	{
		Modify();
		UpdateDefaultPins();
		UpdateFirstPins();
		UpdateGraphViewPin();
		Pins.Reset();
		AllocateDefaultPins();
		FixupDefaultPins();
		BuildFirstPins();
		FixupFirstPins();
		BuildGraphViewPin();
		FixupGraphViewPin();
		GetBlueprint()->Status = BS_Dirty;
		GetGraph()->NotifyNodeChanged(this);
	}
#endif
}

UEdGraphPin* UK2Node_GetDCStruct::GetViewNamePin() const
{
	for (auto Pin : Pins)
		if (Pin->PinName.ToString() == PinNameHelper.GraphViewName)
			return Pin;
	return nullptr;
}

UEdGraphPin* UK2Node_GetDCStruct::GetViewInPin() const
{
	for (auto Pin : Pins)
		if (Pin->PinName.ToString() == PinNameHelper.GraphViewIn)
			return Pin;
	return nullptr;
}

UEdGraphPin* UK2Node_GetDCStruct::GetReturnValuePin() const
{
	for(auto Pin : Pins)
		if (Pin->PinName.ToString() == PinNameHelper.ReturnValue)
			return Pin;
	return nullptr;
}

UEdGraphPin* UK2Node_GetDCStruct::GetFindViewWayPin() const
{
	for (auto Pin : Pins)
		if (Pin->PinName.ToString() == PinNameHelper.GetViewWay)
			return Pin;
	return nullptr;
}

UEdGraphPin* UK2Node_GetDCStruct::GetDCWrapperPin() const
{
	for (auto Pin : Pins)
		if (Pin->PinName.ToString() == PinNameHelper.DCWrapper)
			return Pin;
	return nullptr;
}

void UK2Node_GetDCStruct::AllocateDefaultPins()
{
	DefaultPinNames.Empty();
	
	UEdGraphPin* ExecPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	DefaultPinNames.Emplace(ExecPin->PinName.ToString());
	
	auto ThenPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	DefaultPinNames.Emplace(ThenPin->PinName.ToString());

	auto DCWrapperType = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Class, UDCWrapper_GraphWeaver::StaticClass(), *PinNameHelper.DCWrapper);
	DefaultPinNames.Emplace(DCWrapperType->PinName.ToString());
	DCWrapperType->bHidden = true;
	
	UEnum* EFindViewWay = StaticEnum<NAGetGraphViewWay::EGetGraphViewWay>();
	auto FindViewWay = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, EFindViewWay, *PinNameHelper.GetViewWay);
	FindViewWay->bNotConnectable = true;
	FindViewWay->DefaultValue = EFindViewWay->GetNameStringByIndex(IndexValueOfGetViewWay);
	DefaultPinNames.Emplace(FindViewWay->PinName.ToString());
}

void UK2Node_GetDCStruct::PostPlacedNewNode()
{
	UserGuid = GetDefault<UGraphWeaverPerUserGuid>()->UserGuid;
	AllGetDCStructArray::Get().AddNewElem_OSS(this);
	AllWaitDCStructArray::Get().AddNewElem_OSS(this);
	BuildFirstPins();
	BuildGraphViewPin();
}

void UK2Node_GetDCStruct::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		if (Pin == GetFindViewWayPin())
		{
			UEnum* E = StaticEnum<NAGetGraphViewWay::EGetGraphViewWay>();
			Pin->DefaultValue = E->GetNameStringByIndex(IndexValueOfGetViewWay);
		}
		if (Pin == GetViewInPin())
			Pin->DefaultObject = OldViewIn;
		if (Pin == GetViewNamePin())
			Pin->DefaultValue = OldViewName;
		return ;
	}
	if (Pin == GetFindViewWayPin())
	{
		Modify();
		UpdateDefaultPins();
		UpdateFirstPins();
		UpdateGraphViewPin();
		BreakAllLinkedTo();
		IndexValueOfGetViewWay = StaticEnum<NAGetGraphViewWay::EGetGraphViewWay>()->GetIndexByNameString(Pin->DefaultValue);
		Pins.Reset();
		AllocateDefaultPins();
		FixupDefaultPins();
		BuildFirstPins();
		FixupFirstPins();
		BuildGraphViewPin();
		FixupGraphViewPin();
		UpdateSourceView();
		UpdateDCWrapperPinValueByGraphView();
		BuildReturnPinsByDCWrapperPins();
		GetGraph()->NotifyNodeChanged(this);
		return ;
	}
	if (Pin == GetViewNamePin() || Pin == GetViewInPin())
	{
		Modify();
		UpdateDefaultPins();
		UpdateFirstPins();
		UpdateGraphViewPin();
		BreakAllLinkedTo();
		Pins.Reset();
		ErrorMsg.Reset();
		AllocateDefaultPins();
		FixupDefaultPins();
		BuildFirstPins();
		FixupFirstPins();
		BuildGraphViewPin();
		FixupGraphViewPin();
		UpdateSourceView();
		UpdateDCWrapperPinValueByGraphView();
		BuildReturnPinsByDCWrapperPins();
		GetGraph()->NotifyNodeChanged(this);
		return ;
	}
}

void UK2Node_GetDCStruct::ReconstructNode()
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		UpdateDefaultPins();
		UpdateFirstPins();
		UpdateGraphViewPin();
		auto ResultPin = GetReturnValuePin();
		TArray<UEdGraphPin*> ReturnValue{ResultPin};
		bool HasReturnValue = ResultPin != nullptr;
		Pins.Reset();
		AllocateDefaultPins();
		FixupDefaultPins();
		BuildFirstPins();
		FixupFirstPins();
		BuildGraphViewPin();
		FixupGraphViewPin();
		if (HasReturnValue)
		{
			BuildReturnPinsByDCWrapperPins();
			RewireOldPinsToNewPins(ReturnValue, Pins, {});
			GetGraph()->NotifyNodeChanged(this);
		}
		if (auto Pin = GetViewNamePin())
			OldViewName = Pin->DefaultValue;
		if (auto Pin = GetViewInPin())
			OldViewIn = Pin->DefaultObject;
		return ;
	}
	if (BeCopied)
		return ;
	StartUpEngine = true;
	
	if (HasAnyFlags(RF_Transactional))
	{
		AllGetDCStructArray::Get().AddNewElem_OSS(this);
		AllWaitDCStructArray::Get().AddNewElem_OSS(this);
	}

	TArray<UEdGraphPin*> PinSecond;
	auto ResultPin = GetReturnValuePin();
	if (ResultPin != nullptr)
		PinSecond.Emplace(ResultPin);
	TArray<UEdGraphPin*> OldPins{PinSecond};
	UpdateDefaultPins();
	UpdateFirstPins();
	UpdateGraphViewPin();
	Pins.Reset();
	ErrorMsg.Reset();
	AllocateDefaultPins();
	FixupDefaultPins();
	BuildFirstPins();
	FixupFirstPins();
	BuildGraphViewPin();
	FixupGraphViewPin();
	if (ResultPin != nullptr)
	{
		BuildReturnPinsByDCWrapperPins();
		TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
		RewireOldPinsToNewPins(OldPins, Pins, &NewPinsToOldPins);
	}
}

void UK2Node_GetDCStruct::PostLoad()
{
	Super::PostLoad();
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
		return ;
	if (HasAnyFlags(RF_Transient) && StartUpEngine)
	{
		//让ForEachLoop等 接收数组的节点断开，重新匹配
		BreakAllNodeLinks();
		return ;
	}
	if (HasAnyFlags(RF_Transient))//直接从源蓝图进行拷贝就可以了
		return ;

	Modify();
	TArray<UEdGraphPin*> PinSecond;
	PinSecond.Emplace(GetReturnValuePin());
	TArray<UEdGraphPin*> OldPins{PinSecond};
	UpdateDefaultPins();
	UpdateFirstPins();
	UpdateGraphViewPin();
	Pins.Reset();
	AllocateDefaultPins();
	FixupDefaultPins();
	BuildFirstPins();
	FixupFirstPins();
	BuildGraphViewPin();
	FixupGraphViewPin();
	IndexOfSourceGraphView = -1;
	UpdateSourceView();
	UpdateDCWrapperPinValueByGraphView();
	BuildReturnPinsByDCWrapperPins();
	if (IndexOfSourceGraphView != -1)
		RewireOldPinsToNewPins(OldPins, Pins, {});
	
	//强制在本地更新之后让备份更新ExpandNode的状态
	StartUpEngine = false;
	GetBlueprint()->Status = BS_Dirty;
	GetGraph()->NotifyNodeChanged(this);
}

void UK2Node_GetDCStruct::PostPasteNode()
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
	{
		if (auto Pin = GetViewNamePin())
			OldViewName = Pin->DefaultValue;
		if (auto Pin = GetViewInPin())
			OldViewIn = Pin->DefaultObject;
		IndexValueOfGetViewWay = -1;
		return ;
	}
	BeCopied = true;
	AllGetDCStructArray::Get().AddNewElem_OSS(this);
	if(IndexOfSourceGraphView != -1)
		AllGraphViewArray::Get().GetAllViews()[IndexOfSourceGraphView]->GetDCStructIndex.Emplace(IndexInArray);
	else
		AllWaitDCStructArray::Get().AddNewElem_OSS(this);
}

void UK2Node_GetDCStruct::DestroyNode()
{
	if (UserGuid != GetDefault<UGraphWeaverPerUserGuid>()->UserGuid)
		return ;
	AllGetDCStructArray::Get().RemoveNode_OSS(this);
	if (IndexInWaitArray != -1)
		AllWaitDCStructArray::Get().RemoveDC_OSS(this);
	else
		AllGraphViewArray::Get().GetAllViews()[IndexOfSourceGraphView]->GetDCStructIndex.RemoveSingleSwap(IndexInArray);

	if (IndexInArray != AllGetDCStructArray::Get().GetArray().Num())
	{
		auto SwapedDC = AllGetDCStructArray::Get().GetArray()[IndexInArray];
		if (SwapedDC->IndexInWaitArray == -1)
		{
			auto View = AllGraphViewArray::Get().GetAllViews()[SwapedDC->IndexOfSourceGraphView];
			for (auto& DCChild : View->GetDCStructIndex)
			{
				if (DCChild == AllGetDCStructArray::Get().GetArray().Num())
				{
					DCChild = IndexInArray;
					break ;
				}
			}
		}
	}
	Super::DestroyNode();
}

void UK2Node_GetDCStruct::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (GetDCWrapperPin()->DefaultObject == nullptr)
	{
		MessageLog.Error(
			*NSLOCTEXT("DCWrapperValueInvalid", "GetDCStructError","@@: 'SpawnGraphView' is not specified, or the 'DCWrapperType' of the specified 'SpawnGraphView' is empty.").ToString(),
			this  // 传入节点指针用于错误定位
		);
	}
}

void UK2Node_GetDCStruct::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	if (GetDCWrapperPin()->DefaultObject == nullptr || StartUpEngine)
	{
		Error:
		UK2Node_CallFunction* NonFun = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		NonFun->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, NonFunction), UFunctionForUK2Node::StaticClass());
		NonFun->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *NonFun->GetExecPin());
		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *NonFun->GetThenPin());
		BreakAllNodeLinks();
		return ;
	}

	UClass* WrapperType = static_cast<UClass*>(GetDCWrapperPin()->DefaultObject);
	FString GetDCStructFuncName = "GetDCStruct_" + UFunctionForUK2Node::GetClassCPPNameFromDefaultObject(WrapperType->GetDefaultObjectName().ToString());
	UFunction* Function = WrapperType->FindFunctionByName(*GetDCStructFuncName);
	if (Function == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Function '%s' not found."), *GetDCStructFuncName);
		goto Error;
	}

	FString ViewName;
	for (TFieldIterator<FObjectPropertyBase> It(Function) ; It ; ++It)
	{
		FObjectPropertyBase* Property = *It;
		if (Property->PropertyClass && Property->PropertyClass->IsChildOf(UGraphView::StaticClass()))
		{
			ViewName = Property->GetName();
			break ;
		}
	}

	if (ViewName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Function '%s' is missing a required parameter. Expected a UGraphView-derived UObject pointer."), 
			*GetDCStructFuncName);
		goto Error;
	}
	
	UK2Node_CallFunction* GetDCStruct = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetDCStruct->FunctionReference.SetExternalMember(*GetDCStructFuncName, static_cast<UClass*>(GetDCWrapperPin()->DefaultObject));
	GetDCStruct->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *GetDCStruct->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *GetDCStruct->GetThenPin());
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(*PinNameHelper.GraphView), *GetDCStruct->FindPinChecked(ViewName));
	CompilerContext.MovePinLinksToIntermediate(*GetReturnValuePin(), *GetDCStruct->GetReturnValuePin());

	BreakAllNodeLinks();
}

void UK2Node_GetDCStruct::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_GetDCStruct::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

FText UK2Node_GetDCStruct::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("PreGraphNodeKN", "NodeTitleKey", "GetDCStruct");
}

FSlateIcon UK2Node_GetDCStruct::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.SpawnActor_16x");
	return Icon;
}

FText UK2Node_GetDCStruct::GetTooltipText() const
{
	return NSLOCTEXT("PreGetDCStructKN", "GetTooltipText",
	"Retrieve the corresponding data from each 'NodeInfo' for serialization.");
}

void UK2Node_DCStructDeserialize::BuildDCStructArrayPinByWrapperPin()
{
	auto Wrapper = static_cast<UClass*>(FindPinChecked(*PinNameHelper.WrapperName)->DefaultObject);
	if (Wrapper == nullptr)
		return ;
	UScriptStruct* StructType = const_cast<UDCWrapper_GraphWeaver*>(GetDefault<UDCWrapper_GraphWeaver>(Wrapper))->GetStructType();
	FCreatePinParams Params;
	Params.ContainerType = EPinContainerType::Array;
	auto PreDCStructPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, StructType, *PinNameHelper.DCStructName, Params);
	PreDCStructPin->PinType.bIsReference = true;
}

TArray<UEdGraphPin*> UK2Node_DCStructDeserialize::GetDefaultPins()
{
	TArray<UEdGraphPin*> r;
	r.Reserve(DefaultPinsNames.Num());
	for (int i = 0; i < DefaultPinsNames.Num(); i++)
	{
		for (auto Pin : Pins)
		{
			if (Pin->PinName == DefaultPinsNames[i])
			{
				r.Emplace(Pin);
				break ;
			}
		}
	}
	return r;
}

void UK2Node_DCStructDeserialize::UpdateDefaultPins()
{
	DefaultPins = GetDefaultPins();
}

void UK2Node_DCStructDeserialize::FixupDefaultPins()
{
	RestoreSplitPins(DefaultPins);
	RewireOldPinsToNewPins(DefaultPins, Pins, {});
}

void UK2Node_DCStructDeserialize::UpdateDCStructPin()
{
	DCStructPin = FindPin(PinNameHelper.DCStructName);
}

void UK2Node_DCStructDeserialize::FixupDCStructPin()
{
	TArray<UEdGraphPin*> OldPin{DCStructPin};
	RewireOldPinsToNewPins(OldPin, Pins, {});
}

void UK2Node_DCStructDeserialize::AllocateDefaultPins()
{
	DefaultPinsNames.Empty();
	
	auto ExecPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	DefaultPinsNames.Emplace(ExecPin->PinName.ToString());
	
	auto ThenPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	DefaultPinsNames.Emplace(ThenPin->PinName.ToString());
	
	auto WrapperPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Class, UDCWrapper_GraphWeaver::StaticClass(), *PinNameHelper.WrapperName);
	WrapperPin->DefaultObject = UDCWrapperForDefaultStruct_GraphWeaver::StaticClass();
	WrapperPin->bNotConnectable = true;
	DefaultPinsNames.Emplace(WrapperPin->PinName.ToString());

	auto ViewPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UGraphView::StaticClass(), *PinNameHelper.ViewName);
	DefaultPinsNames.Emplace(ViewPin->PinName.ToString());
}

void UK2Node_DCStructDeserialize::PostPlacedNewNode()
{
	BuildDCStructArrayPinByWrapperPin();
}

void UK2Node_DCStructDeserialize::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (Pin->PinName == PinNameHelper.WrapperName)
	{
		Modify();
		UpdateDefaultPins();
		Pins.Reset();
		ErrorMsg.Reset();
		AllocateDefaultPins();
		FixupDefaultPins();
		BuildDCStructArrayPinByWrapperPin();
		GetGraph()->NotifyNodeChanged(this);
		return ;
	}
}

void UK2Node_DCStructDeserialize::ReconstructNode()
{
	UpdateDefaultPins();
	UpdateDCStructPin();
	Pins.Reset();
	AllocateDefaultPins();
	FixupDefaultPins();
	BuildDCStructArrayPinByWrapperPin();
	FixupDCStructPin();
	GetGraph()->NotifyNodeChanged(this);
}

void UK2Node_DCStructDeserialize::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	if (FindPinChecked(PinNameHelper.WrapperName)->DefaultObject == nullptr)[[unlikely]]
	{
		Error:
		UK2Node_CallFunction* Non = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		Non->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, NonFunction), UFunctionForUK2Node::StaticClass());
		Non->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *Non->GetExecPin());
		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *Non->GetThenPin());

		BreakAllNodeLinks();
		return ;
	}
	UClass* WrapperType = static_cast<UClass*>(FindPinChecked(PinNameHelper.WrapperName)->DefaultObject);
	auto FunctionToCall = "ResetPerNode_" + UFunctionForUK2Node::GetClassCPPNameFromDefaultObject(WrapperType->GetDefaultObjectName().ToString());

	FString ViewName, DCStructArrayName;
	UFunction* Function = WrapperType->FindFunctionByName(*FunctionToCall);

	if (Function == nullptr)[[unlikely]]
	{
		UE_LOG(LogTemp, Error, TEXT("Function '%s' not found."), *FunctionToCall);
		goto Error;
	}
	uint8 FindView = 0, FindDCArray = 0;//只寻找第一个匹配项
	for (TFieldIterator<FProperty> It(Function) ; It ; ++It)
	{
		FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue ;
		}
		if (FindView == 1 && FindDCArray == 1)
			break ;
		if (FindView == 0)
		{
			if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
			{
				if (ObjectProp->PropertyClass && ObjectProp->PropertyClass->IsChildOf(UGraphView::StaticClass()))
				{
					// 这是一个 UGraphView* 或 UGraphView 子类的指针(TObjectPtr)
					ViewName = Property->GetName();
					FindView = 1;
					continue ;
				}
			}
		}
		if (FindDCArray == 0)
		{
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				// 获取数组内部元素类型
				FProperty* InnerProp = ArrayProp->Inner;
        
				// 检查元素是否是结构体
				if (FStructProperty* StructProp = CastField<FStructProperty>(InnerProp))
				{
					UScriptStruct* Struct = StructProp->Struct;
            
					// 检查是否是 FDCStruct_GraphWeaver 或其子结构体
					if (Struct && Struct->IsChildOf(FDCStruct_GraphWeaver::StaticStruct()))
					{
						DCStructArrayName = *Property->GetName();
						FindDCArray = 1;
						continue ;
					}
				}
			}
		}
	}

	if (ViewName.IsEmpty() || DCStructArrayName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Function '%s' is missing a required parameter. "
			"Expected either a UGraphView-derived UObject pointer, or a TArray of structs derived from FDCStruct_GraphWeaver."), 
			*FunctionToCall);
		goto Error;
	}
	
	UK2Node_CallFunction* ResetPerNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	ResetPerNode->FunctionReference.SetExternalMember(*FunctionToCall, WrapperType);
	ResetPerNode->AllocateDefaultPins();

	if (ResetPerNode->FindPin(DCStructArrayName)->Direction != EGPD_Input)
	{
		UE_LOG(LogTemp, Error, TEXT("Parameter '%s' of function '%s' is missing the 'const' or 'UPARAM(ref)' specifier."),
			*DCStructArrayName, *FunctionToCall);
		goto Error;
	}

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *ResetPerNode->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *ResetPerNode->GetThenPin());
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.ViewName), *ResetPerNode->FindPinChecked(ViewName));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(PinNameHelper.DCStructName),
		*ResetPerNode->FindPinChecked(DCStructArrayName));
	BreakAllNodeLinks();
}

void UK2Node_DCStructDeserialize::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_DCStructDeserialize::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

FText UK2Node_DCStructDeserialize::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("PreGraphNodeKN", "NodeTitleKey", "DCStructDeserialize");
}

FSlateIcon UK2Node_DCStructDeserialize::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Default_16x");
	return Icon;
}

FText UK2Node_DCStructDeserialize::GetTooltipText() const
{
	return NSLOCTEXT("PreDCStructDeserializeKN", "GetTooltipText", "Deserialize each 'NodeInfo' based on the stored data. \n "
	"Note that before using the node, you must first call GraphView::ResetBasicInformFromBasicDC to initialize the 'GraphView' information.");
}


#undef LLOCTEXT_NAMESPACE

//////////////////////////////////////////////////////////////////////////////////////////
#define LOCTEXT_NAMESPACE "FunctionsForGraphWeaver"

void UK2Node_ValidateRankingConsistency::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UGraphView::StaticClass(), TEXT("GraphView"));
	
	FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Array;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("IgnoredNodeIndices"), PinParams);

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, TEXT("Consistency"));
}

void UK2Node_ValidateRankingConsistency::ExpandNode(class FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* CallCheckFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallCheckFunc->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UGraphView, ValidateRankingConsistency_Inner), UGraphView::StaticClass());
	CallCheckFunc->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("GraphView")), *CallCheckFunc->FindPinChecked(TEXT("GraphView")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("Consistency")), *CallCheckFunc->GetReturnValuePin());

	if (FindPinChecked(TEXT("IgnoredNodeIndices"))->LinkedTo.Num() > 0)
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("IgnoredNodeIndices")),
			*CallCheckFunc->FindPinChecked(TEXT("IgnoredNodeIndices")));
	else
	{
		UK2Node_CallFunction* GetEmptyIntArray = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		GetEmptyIntArray->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, GetEmptyIntArray),
			UFunctionForUK2Node::StaticClass());
		GetEmptyIntArray->AllocateDefaultPins();

		GetEmptyIntArray->GetReturnValuePin()->MakeLinkTo(CallCheckFunc->FindPinChecked(TEXT("IgnoredNodeIndices")));
	}

	BreakAllNodeLinks();
}

FText UK2Node_ValidateRankingConsistency::GetTooltipText() const
{
	return NSLOCTEXT("PreGraphNodeKN", "GetTooltipText_CHeckRanking", "Check whether the 'Ranking' of each element in 'RealNodes' is correct within the family \n "
	"relationships. Manually deleting certain elements from 'RealNodes' (not recommended) or calling \n "
	"the 'RemoveNodes' function may cause incorrect 'Ranking'. See the 'RemoveNodes' function for details.");
}

FText UK2Node_ValidateRankingConsistency::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("UK2NodeName", "CheckIndexIsRightName", "ValidateRankingConsistency");
}

FText UK2Node_ValidateRankingConsistency::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

void UK2Node_ValidateRankingConsistency::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}



void UK2Node_ValidateRankingConsistencyLight::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UGraphView::StaticClass(), TEXT("GraphView"));
	
	FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Array;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("IgnoredNodeIndices"), PinParams);

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, TEXT("Consistency"));
}

void UK2Node_ValidateRankingConsistencyLight::ExpandNode(class FKismetCompilerContext& CompilerContext,
                                                         UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* CallCheckFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallCheckFunc->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UGraphView, ValidateRankingConsistencyLight_Inner), UGraphView::StaticClass());
	CallCheckFunc->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("GraphView")), *CallCheckFunc->FindPinChecked(TEXT("GraphView")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("Consistency")), *CallCheckFunc->GetReturnValuePin());

	if (FindPinChecked(TEXT("IgnoredNodeIndices"))->LinkedTo.Num() > 0)
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("IgnoredNodeIndices")),
			*CallCheckFunc->FindPinChecked(TEXT("IgnoredNodeIndices")));
	else
	{
		UK2Node_CallFunction* GetEmptyIntArray = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		GetEmptyIntArray->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionForUK2Node, GetEmptyIntArray),
			UFunctionForUK2Node::StaticClass());
		GetEmptyIntArray->AllocateDefaultPins();

		GetEmptyIntArray->GetReturnValuePin()->MakeLinkTo(CallCheckFunc->FindPinChecked(TEXT("IgnoredNodeIndices")));
	}

	BreakAllNodeLinks();
}

FText UK2Node_ValidateRankingConsistencyLight::GetTooltipText() const
{
	return NSLOCTEXT("PreGraphNodeKN", "GetTooltipText_CHeckRanking", "Check whether the 'Ranking' of each element in 'RealNodes' is correct within the family \n "
	"relationships. Manually deleting certain elements from 'RealNodes' (not recommended) or calling \n "
	"the 'RemoveNodes' function may cause incorrect 'Ranking'. See the 'RemoveNodes' function for details.\n "
	"This function does not print logs. If you only want to check whether the 'Ranking' is entirely correct, this function is more suitable than \n "
	"'ValidateRankingConsistency'.");
}

FText UK2Node_ValidateRankingConsistencyLight::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("UK2NodeName", "CheckIndexIsRightLightName", "ValidateRankingConsistencyLight");
}

FText UK2Node_ValidateRankingConsistencyLight::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

void UK2Node_ValidateRankingConsistencyLight::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

#undef LOCTEXT_NAMESPACE



/*
* BlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AbortExecution,
			INVTEXT("execMakeCustomDataEffectData 必须使用Struct类型进行链接")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
ContainerPtrToValuePtr
*/

//以下是写这款插件的时候借鉴学习的源代码
//UK2Node_GetDataTableRow
//UK2Node_SpawnActorFromClass
//UK2Node_IfThenElse
//PostReconstructNode
//UK2Node_ConstructObjectFromClass
//SpawnActor()
//FCriticalSection
//UK2Node::ReallocatePinsDuringReconstruction
//UEdGraphNode::PostLoadSubobjects()
//UK2Node_Select
//UK2Node_MakeStruct
//UObject::PostEditChangeProperty
//UEdGraphNode::ValidateNodeDuringCompilation
//UK2Node_GenericCreateObject

//UEdGraphNode::PostPasteNode
//UK2Node::DoPinsMatchForReconstruction
//UK2Node::GetNodeRefreshPriority
//UEdGraphNode::PinTypeChanged
//UK2Node::ReconstructNode()
//UEdGraphSchema_K2::SplitPin()
//UK2Node_Variable
//struct FMemberReference
//CompileClassLayout
//UK2Node_GetArrayItem