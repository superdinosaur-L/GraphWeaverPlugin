// Copyright 2026 RainButterfly. All Rights Reserved.

//实际上因为涉及到使用原生c++的单例模式来进行View,Node,DCStruct的管理,而虚幻的撤销操作又不能对
//单例模式起作用,所以即使添加了Modify也不能起什么作用

//记得给View,Node等添加多线程安全锁
#pragma once

#include "CoreMinimal.h"
#include <coroutine>
#include "K2Node.h"
#include "Engine/TimerHandle.h"
#include <atomic>
#include <memory>
#include "EdGraph/EdGraph.h"
#include "K2NodeForGraph.generated.h"

class UDCWrapper_GraphWeaver;
class UK2Node_CallFunction;
class UK2Node_SpawnGraphView;
class UK2Node_SpawnGraphNode;
struct FNamesInputNode;
class UEdGraphPin;
class UK2Node_GetDCStruct;


#define EMPTY_LOG_UK2NODE() UE_LOG(LogTemp, Error, TEXT("        "))
#define WAITING_MOD_LOG_UK2NODE() \
    do { \
        UE_LOG(LogTemp, Error, TEXT("The code here is incomplete and needs to be fixed immediately.")); \
        UE_LOG(LogTemp, Error, TEXT("File: %s, Func: %s, Line: %d"), TEXT(__FILE__), TEXT(__FUNCTION__), __LINE__); \
        EMPTY_LOG_UK2NODE() \
    } while(0)

class FEditorLinkerTask
{
public:
	struct promise_type
	{
		std::suspend_never initial_suspend() noexcept;
		std::suspend_never final_suspend() noexcept;
		FEditorLinkerTask get_return_object() noexcept;
		void return_void() noexcept;
		void unhandled_exception() noexcept;
	};
};

class FPollAwaiter
{
public:
	// 构造函数：超时秒数 + 监控对象（用于防止UObject被GC）
	FPollAwaiter(float InTimeout, TWeakObjectPtr<UK2Node_SpawnGraphView> InOwner)
		: TimeoutSeconds(InTimeout), WeakOwner(InOwner) {}
    
	// 禁止拷贝（避免生命周期混乱）
	FPollAwaiter(const FPollAwaiter&) = delete;
	FPollAwaiter& operator=(const FPollAwaiter&) = delete;
	~FPollAwaiter() { bIsActive = false; }

public:
	double TimeoutSeconds;
	TWeakObjectPtr<UK2Node_SpawnGraphView> WeakOwner;
	std::atomic<bool> bIsActive{true};

	// 所有轮询状态数据（通过shared_ptr共享）
	struct PollState
	{
		std::coroutine_handle<> Handle;
		float TimeoutSeconds;
		TWeakObjectPtr<UK2Node_SpawnGraphView> WeakOwner;
		std::atomic<bool> bIsActive{true};
		float ElapsedTime = 0.0f;
		FTimerHandle TimerHandle;
	};

	static bool IsConditionMet(std::shared_ptr<PollState> State);
	// 静态轮询函数（不捕获this，避免悬挂指针）
	static void StartPolling(std::shared_ptr<PollState> State, float ElapsedTime);

	bool await_ready() noexcept;
	void await_suspend(std::coroutine_handle<> handle) noexcept;
	void await_resume() noexcept;
};


//存放所有的UK2Node_SpawnGraphView
class  AllGraphViewArray
{
public:
	static AllGraphViewArray& Get()
	{
		static AllGraphViewArray Instance;  // C++11保证线程安全
		return Instance;
	}
	
	void AddView_OSS(UK2Node_SpawnGraphView* View);

	//包含重排序View的Index.OSS是一条龙的意思
	void RemoveView_OSS(UK2Node_SpawnGraphView* View);

	//在Views里面通过GraphViewName去查找对应的SpawnGraphView
	UK2Node_SpawnGraphView* FindViewByCommonName(const FString& Name) const;
	
	TArray<TStrongObjectPtr<UK2Node_SpawnGraphView>>& GetAllViews()
	{
		return Views;
	}

	//当删除View的时候，需要更新每个View在数组里面的下标
	void UpdateAllViewIndex();
private:

	//阻止GC
	TArray<TStrongObjectPtr<UK2Node_SpawnGraphView>> Views;
};

//在ReconstructNode,PostPlaceNewNode,PostPasteNode里面加入到SpawnGraphViewArray里面
UCLASS(meta = (Keywords = "Spawn Create Graph"))
class UK2NODETOOLS_API UK2Node_SpawnGraphView : public UK2Node
{
	GENERATED_BODY()
public:
	struct SpawnGraphViewPinName
	{
		FString ConstructConfig = "ConstructConfig";
		FString ReturnValue = "GraphView";
		FString ExplicitViewName = "GraphViewName";
		FString HiddenViewName = "GraphViewName_Hidden";
		FString WayToDealSameNode = "WayToDealSameNode";
		FString RealNodesReserveSize = "AllocateSize";
		FString RealNodesType = "RealNodesType";
		FString DCWrapperType = "DCWrapperType";
		FString ConstructConfig_NamingOfRules = "NamingOfRules";
		FString ConstructConfig_Precision = "Precision";
	} PinNameHelper;
public:
	////当前节点是否是复制粘贴出来的。给拷贝体使用
	UPROPERTY()
	uint8 BeCopied = 0;//sure

	//名字相同的时候直接判死刑,表示该节点将永久失去任何作用
	UPROPERTY()
	uint8 NameSameAsOtherView = 0;//sure
	
	UPROPERTY()
	TArray<FString> DefaultPinsName;
	
	UPROPERTY()
	int32 IndexValueOfWayToDealSameNode = 0;//sure

	UPROPERTY()
	int32 IndexInViewArray = -1;
	
	UPROPERTY()
	FGuid UserGuid;
	
	TArray<int32> ChildNodeIndex;
	TArray<int32> GetDCStructIndex;
	
	FString OldExplicitName;
	UObject* OldWrapperType = nullptr;

	UPROPERTY(EditAnywhere, Category = "GraphWeaver|UK2Node" ,
		meta = (ToolTip = "When you create a 'SpawnGraphNode' and set 'GetGraphViewWay' to 'Link',  and then create a 'SpawnGraphView', the system must wait for the 'SpawnGraphView' to be fully loaded into the Blueprint before it can connect to the corresponding 'SpawnGraphNode'.The 'MaxWaitTime' specifies how long (in seconds) the system will wait for the View to load into the Blueprint.If this value is too large, it may cause stuttering or lag.If your computer has sufficient performance, you can reduce this value.The minimum allowed value is '0.2' seconds."))
	double MaxWaitTime = 2.f;

	bool AboutToDie = false;//sure
public:
	bool CheckSelfIsValid();
	
	UEdGraphPin* GetNamesConfigPin() const;
	UEdGraphPin* GetReturnValuePin() const;
	UEdGraphPin* GetExplicitViewNamePin() const;
	UEdGraphPin* GetHiddenViewNamePin() const;
	UEdGraphPin* GetWrapperTypePin() const;

	//获取自己的所有孩子UK2Node_SpawnGraphNode
	TArray<UK2Node_SpawnGraphNode*> GetRealSpawnNodes();
	TArray<UK2Node_GetDCStruct*> GetRealGetDCNodes();
	
	TArray<UEdGraphPin*> GetDefaultPins(bool IncludeSubPins = true);
	
	void UpdateHiddenViewNameForThisNode();

	//下面的两个FindWait会自动处理子节点和父节点的互相指认的逻辑，并且把找到的孩子从Wait数组里面移除
	
	/////////////////////////////////////////////////////////////////////////////////////
	TArray<UK2Node_SpawnGraphNode*> FindWaitChildNodeByCommonName_OSS();
	TArray<UK2Node_GetDCStruct*> FindWaitChildDCByCommonName_OSS();
	//由于在单线程里面调用该函数，所以一定可以保证节点的先后顺序
	TArray<UK2Node_SpawnGraphNode*> FindWaitNodeChildByLink();
	//由于在单线程里面调用该函数，所以一定可以保证节点的先后顺序
	TArray<UK2Node_GetDCStruct*> FindWaitDCChildByLink();
	//删除孩子节点里面GetGraphViewWay的方式为Name的节点。这里并没有让Name方式的孩子根据当前Name去寻找新的View
	TArray<UK2Node_SpawnGraphNode*> EmptyChildNode_Name();
	void EmptyChildDC_Name();

	//删除所有子孩子
	void EmptyAllChild();

	/////////////////////////////////////////////////////////
	FEditorLinkerTask DelayLinkChild_Link();

	FString GenerateRandomString(int32 Length);
public:
	virtual void AllocateDefaultPins() override;
	virtual void PostReconstructNode() override;
	//设置UserGuid是为了可以通过删除节点的Guid检测
	virtual void PostPasteNode() override;
	virtual void ReconstructNode() override;
	virtual void PostPlacedNewNode() override;
	//virtual void PostLoad() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FText GetTooltipText() const override;
	virtual void DestroyNode() override;
	//该函数由拷贝体执行而不是本体执行。不能理解
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool ShouldShowNodeProperties() const override;
	
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
};


//因为这里要找的实际上是UK2Node_SpawnGraphView，而不是真正要找GraphView，而UK2Node只能存在于蓝图里面，所以要找的GraphView目标
//必须由UK2Node_SpawnGraphView生成
UENUM()
namespace NAGetGraphViewWay
{
	enum EGetGraphViewWay : uint8
	{
		Name,
		Link,
	};
}


///////////////////////////////////////////////////////////////////////////////////////////
//因为虚幻底层的代码限制的问题，该类不能在PinDefaultValueChanged里面动态的清空Pin，删除Pin，


///////////////////////////////////////////////////////////////////////////////
UCLASS()
class UK2NODETOOLS_API UK2Node_SpawnGraphNode : public UK2Node
{
	GENERATED_BODY()
public:
	struct SpawnGraphNodePinName
	{
		FString SourceViewName = "GraphViewName";
		FString SourceViewInBlueprint = "GraphViewIn";
		FString SourceViewHiddenName = "ViewHiddenName";
		FString GetViewWay = "GetViewWay";
		FString AutoBuild = "AutoBuild";
		FString ReturnValue = "GraphNode";
		FString NamesInput = "NamesInput";
		FString NamesInput_SelfName = "SelfName";
		FString NamesInput_ParentNames = "ParentNames";
		FString NamesInput_BroNames = "BroNames";
	} PinNameHelper;
public:
	//当前节点是否是复制粘贴出来的。不能使用UPROPERTY进行修饰
	uint8 BeCopied = 0;//sure
	
	//下面的几个int32类型加一个UPROPERTY只是为了让Modify可以回溯

	/////////////////////////////////////////////
	//SourceGraphView在AllGraphViewArray里面的Index
	UPROPERTY()
	int32 IndexOfSourceGraphView = -1;//sure 拷贝体使用

	//在AllGraphNodeArray里面的Index
	UPROPERTY()
	int32 IndexInAllNodeArray = -1;

	UPROPERTY()
	int32 IndexInWaitNodeArray = -1;

	UPROPERTY()
	int32 IndexValueOfGetViewWay = 0;//sure

	//所有的FString类型数组只记录父亲节点,不记录子节点
	
	/////////////////////////////////////////////////////////////////
	UPROPERTY()
	TArray<FString> PinsFirst_Name;//sure 拷贝体使用
	TArray<UEdGraphPin*> PinsFirst_Name_Ptr;
	UPROPERTY()
	TArray<FString> PinsFirst_Link;//sure 拷贝体使用
	TArray<UEdGraphPin*> PinsFirst_Link_Ptr;
	
	UPROPERTY()
	TArray<FString> PinsSecond_NamesConfig;//sure 
	TArray<UEdGraphPin*> PinsSecond_NamesConfig_Ptr;

	UPROPERTY()
	TArray<FString> PinsDefault;//sure 拷贝体使用
	TArray<UEdGraphPin*> PinsDefault_Ptr;

	UPROPERTY()
	FGuid UserGuid;
	
	FString OldExplicitViewName;
	UObject* OldViewIn;

	UPROPERTY(EditAnywhere, Category = "GraphWeaver|UK2Node", meta = (ToolTip = "Whether this 'GraphNode' should automatically participate in the construction of the 'GraphView'."))
	bool AutoBuild = true;
public:
	bool CheckViewIsValid(UK2Node_SpawnGraphView* View);
	
	UEdGraphPin* GetExplicitViewNamePin() const;
	UEdGraphPin* GetHiddenViewNamePin() const;
	//获取  设置TargetView在目标蓝图里面  的Pin
	UEdGraphPin* GetViewInPin() const;
	UEdGraphPin* GetFindViewWayPin() const;

	UK2Node_SpawnGraphView* GetRealSpawnView();

	TArray<UEdGraphPin*> GetDefaultPins();
	void UpdateDefaultPins();
	void FixUpDefaultPins();
	
	//生成GraphViewNamePin或者GraphViewInPin
	void BuildFirstPins();
	TArray<UEdGraphPin*> GetPinsFirst(bool IncludeSubPins = true);
	void UpdateFirstPins();
	void FixUpFirstPins();
	
	void UpdateSourceView();
	
	//生成NamesInputPin
	void BuildSecondPins();
	TArray<UEdGraphPin*> GetPinsSecond();
	void UpdateSecondPins();
	void FixUpSecondPins();

	//清除和第二阶段的Pin连接的Pin(第一阶段不允许有任何连接，所以不需要再写一个类似的函数)
	void BreakLinkedToSecondPins();
	void BreakLinkedToAllPins();
public:

	virtual void AllocateDefaultPins() override;
	//或许叫PostPinDefaultValueChanged会更合适
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void DestroyNode() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;

	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual void ReconstructNode() override;
	virtual void PostLoad() override;
	

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool ShouldShowNodeProperties() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FText GetTooltipText() const override;
};


//仅用作等待UK2Node_SpawnGraphView后到的情况
class AllWaitNodeArray
{
public:
	AllWaitNodeArray(const AllWaitNodeArray& o) = delete;
	AllWaitNodeArray(AllWaitNodeArray&& o) = delete;
	AllWaitNodeArray& operator=(const AllWaitNodeArray& o) = delete;

	
	static AllWaitNodeArray& Get()
	{
		static AllWaitNodeArray Obj;
		return Obj;
	}

	void AddNewNode_OSS(UK2Node_SpawnGraphNode* WaitNode)
	{
		int32 Index = WaitNodeArray.Emplace(WaitNode);
		WaitNode->IndexInWaitNodeArray = Index;
	}

	void  RemoveNode_OSS(int32 IndexInNodeWait)
	{
		WaitNodeArray[IndexInNodeWait]->IndexInWaitNodeArray = -1;
		if (IndexInNodeWait == WaitNodeArray.Num() - 1)
		{
			WaitNodeArray.RemoveAt(IndexInNodeWait);
			return ;
		}
		WaitNodeArray[WaitNodeArray.Num() - 1]->IndexInWaitNodeArray = IndexInNodeWait;
		WaitNodeArray.RemoveAtSwap(IndexInNodeWait);
	}
	
	void RemoveNode_OSS(UK2Node_SpawnGraphNode* WaitNode)
	{
		RemoveNode_OSS(WaitNode->IndexInWaitNodeArray);
	}
	
	auto GetAllNodes()->TArray<TStrongObjectPtr<UK2Node_SpawnGraphNode>>&
	{
		return WaitNodeArray;
	}
private:
	AllWaitNodeArray();
	~AllWaitNodeArray();

	TArray<TStrongObjectPtr<UK2Node_SpawnGraphNode>> WaitNodeArray;
};

//用来记录所有的UK2Node_SpawnGraphNode蓝图节点
class AllGraphNodeArray
{
public:
	static AllGraphNodeArray& Get()
	{
		static AllGraphNodeArray Obj;
		return Obj;
	}

	TArray<TStrongObjectPtr<UK2Node_SpawnGraphNode>>& GetNodes()
	{
		return Nodes;
	}
	
	int32 AddNewNode_OSS(UK2Node_SpawnGraphNode* NewNode);
	int32 RemoveNode_OSS(int32 Index)
	{
		if (Index == Nodes.Num() - 1)
		{
			Nodes.RemoveAt(Index);
			return -1;
		}
		Nodes[Nodes.Num() - 1].Get()->IndexInAllNodeArray = Index;
		Nodes.RemoveAtSwap(Index);
		return Nodes.Num();
	}
	
	int32 RemoveNode_OSS(UK2Node_SpawnGraphNode* Target)
	{
		return RemoveNode_OSS(Target->IndexInAllNodeArray);
	}
	//StartIndex表示从当前Index开始重新对IndexInAllNodeArray排序
	void UpdateAllNodeIndex(int32 StartIndex = -1);
public:
	//阻止GC
	TArray<TStrongObjectPtr<UK2Node_SpawnGraphNode>> Nodes;
private:
	AllGraphNodeArray();
};


//下面是一些辅助使用插件的节点


UCLASS()
class UK2NODETOOLS_API UK2Node_GetDCStruct : public UK2Node
{
	GENERATED_BODY()
public:
	struct StructPinName
	{
		FString GraphView = "GraphView";
		FString GraphViewName = "GraphViewName";
		FString GraphViewIn = "GraphViewIn";
		FString ReturnValue = "DCStruct";
		FString GetViewWay = "GetViewWay";
		FString DCWrapper = "DCWrapper";
	}PinNameHelper;
public:
	UPROPERTY()
	int32 IndexValueOfGetViewWay = 0;//sure

	UPROPERTY()
	TArray<FString> DefaultPinNames;// sure
	TArray<UEdGraphPin*> DefaultPins;

	UPROPERTY()
	TArray<FString> FirstPinsName_NameWay;// sure
	TArray<UEdGraphPin*> FirstPins_NameWay;

	UPROPERTY()
	TArray<FString> FirstPinsName_LinkWay;// sure
	TArray<UEdGraphPin*> FirstPins_LinkWay;

	//为了美观,GraphViewPin总是应该作为最后一个输入Pin生成被放在最下面,所以需要单独列出来照顾
	UEdGraphPin* GraphViewPin;

	UPROPERTY()
	int32 IndexOfSourceGraphView = -1;//sure
	
	int32 IndexInArray = -1;
	int32 IndexInWaitArray = -1;

	//在引擎启动的时候，分身会先执行一次ExpandNode再执行PostLoad。但是这个时候问我还没有在ReconstructNode里面修建第二个引脚。所以第一次应该避免调用ExpandNode
	//使用UPROPERTY让本体的数值拷贝到分身
	UPROPERTY()
	bool StartUpEngine = false;//sure

	UPROPERTY()
	FGuid UserGuid;

	FString OldViewName;
	UObject* OldViewIn;

	bool BeCopied = false;//sure
public:
	bool CheckViewIsValid(UK2Node_SpawnGraphView* View);
	
	void UpdateSourceView();
	
	void BuildFirstPins();
	//仅仅通过当前的DCWrapperPin的类型来生成输出的结构体类型
	void BuildReturnPinsByDCWrapperPins();
	void BuildGraphViewPin();

	TArray<UEdGraphPin*> GetDefaultPins();
	TArray<UEdGraphPin*> GetFirstPins();
	TArray<UEdGraphPin*> GetDefaultAndFirstPins();
	UEdGraphPin* GetGraphViewPin();
	void UpdateDefaultPins();
	void UpdateFirstPins();
	void UpdateGraphViewPin();
	void FixupDefaultPins();
	void FixupFirstPins();
	void FixupGraphViewPin();

	//根据当前的GraphView更新DCWrapper的值,当GraphView为空的时候DCWrapper也为空
	void UpdateDCWrapperPinValueByGraphView();
	void BreakAllLinkedTo()
	{
		for (auto Pin : Pins)
		{
			for (auto LinkedPin : Pin->LinkedTo)
			{
				//由GetDCStruct节点进行Modify()
				LinkedPin->LinkedTo.RemoveSingle(Pin);
				LinkedPin->GetOwningNode()->PinConnectionListChanged(LinkedPin);
				LinkedPin->GetOwningNode()->GetGraph()->NotifyNodeChanged(LinkedPin->GetOwningNode());
			}
		}
	}
public:
	UEdGraphPin* GetViewNamePin() const;
	UEdGraphPin* GetViewInPin() const;
	UEdGraphPin* GetReturnValuePin() const;
	UEdGraphPin* GetFindViewWayPin() const;
	UEdGraphPin* GetDCWrapperPin() const;
public:
	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ReconstructNode() override;
	virtual void PostLoad() override;
	virtual void PostPasteNode() override;
	virtual void DestroyNode() override;

	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	//只有分身才执行这个
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FText GetTooltipText() const override;
};

class AllWaitDCStructArray
{
public:
	static AllWaitDCStructArray& Get()
	{
		static AllWaitDCStructArray Obj;
		return Obj;
	}

	auto& GetArray()
	{
		return DCWaitArray;
	}

	void AddNewElem_OSS(UK2Node_GetDCStruct* NewNode)
	{
		int32 Index = DCWaitArray.Emplace(NewNode);
		DCWaitArray[Index]->IndexInWaitArray = Index;
	}

	void RemoveDC_OSS(UK2Node_GetDCStruct* Target)
	{
		if (Target->IndexInWaitArray == DCWaitArray.Num() - 1)
		{
			DCWaitArray.RemoveAt(DCWaitArray.Num() - 1);
			Target->IndexInWaitArray = -1;
			return ;
		}
		DCWaitArray[DCWaitArray.Num() - 1]->IndexInWaitArray = Target->IndexInWaitArray;
		DCWaitArray.RemoveAtSwap(Target->IndexInWaitArray);
		Target->IndexInWaitArray = -1;
	}
private:
	TArray<TStrongObjectPtr<UK2Node_GetDCStruct>> DCWaitArray;
};

class AllGetDCStructArray
{
public:
	static AllGetDCStructArray& Get()
	{
		static AllGetDCStructArray Obj;
		return Obj;
	}

	auto& GetArray()
	{
		return GetDCStructArray;
	}

	void AddNewElem_OSS(UK2Node_GetDCStruct* NewNode)
	{
		int32 Index = GetDCStructArray.Emplace(NewNode);
		GetDCStructArray[Index]->IndexInArray = Index;
	}

	void RemoveNode_OSS(UK2Node_GetDCStruct* Node)
	{
		if (Node->IndexInArray == GetDCStructArray.Num() - 1)
		{
			GetDCStructArray.RemoveAt(GetDCStructArray.Num() - 1);
			return;
		}
		GetDCStructArray[GetDCStructArray.Num() - 1]->IndexInArray = Node->IndexInArray;
		GetDCStructArray.RemoveAtSwap(Node->IndexInArray);
	}
private:
	TArray<TStrongObjectPtr<UK2Node_GetDCStruct>> GetDCStructArray;
};



UCLASS()
class UK2Node_DCStructDeserialize : public UK2Node
{
	GENERATED_BODY()
	struct PinNameHelperStruct
	{
		FString WrapperName = "DCWrapper";
		FString DCStructName = "DCStruct";
		FString ViewName = "GraphView";
	} PinNameHelper;
public:
	void BuildDCStructArrayPinByWrapperPin();
	
	UPROPERTY()
	TArray<FString> DefaultPinsNames;
	TArray<UEdGraphPin*> DefaultPins;

	TArray<UEdGraphPin*> GetDefaultPins();
	void UpdateDefaultPins();
	void FixupDefaultPins();

	UEdGraphPin* DCStructPin = nullptr;

	void UpdateDCStructPin();
	void FixupDCStructPin();
public:
	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ReconstructNode() override;

	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FText GetTooltipText() const override;
};











/////////////////////////////////////////////

//检查Ranking是否合理
UCLASS()
class UK2Node_ValidateRankingConsistency : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;

	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	virtual FText GetTooltipText() const override;
	virtual bool IsNodePure() const override{return true;}
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
};


//快速检查Ranking是否完全正确,不进行日志打印
UCLASS()
class UK2Node_ValidateRankingConsistencyLight : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;

	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	virtual FText GetTooltipText() const override;
	virtual bool IsNodePure() const override{return true;}
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
};

/*
*/




























