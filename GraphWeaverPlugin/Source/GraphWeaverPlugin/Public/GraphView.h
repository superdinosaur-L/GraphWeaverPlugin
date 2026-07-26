// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once
#include <any>
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GraphView.generated.h"

class UDCWrapper_GraphWeaver;
class UGraphView;
class UGraphNode;
struct FNamesInputNode;

#if 1
UENUM(Blueprintable)
namespace NAGraphConstructErrorCode
{
	enum EErrorCodeForConstructView : uint8
	{
		None,
		NameSameAsRoot,
		BroNameSameAsRoot,
	};
}

// The strategy that should be adopted when the same GraphNode is involved in the construction
// For those who can use C++ code, each of the following options includes an additional action:
// updating the pointer UGraphNode* SourceGraphNode. This does not affect normal use
UENUM(BlueprintType)
namespace NAWayToDealSameGraphNode
{
	enum  EWayToDealSameGraphNode : uint8
	{
		NothingToDo,
		//Reminds you on the screen and in the TempLog that duplicate nodes have been added to the build.
		//Recommended for reviewing whether you have accidentally added the same node multiple times.
		OnlyWarningSameNode,
	};
}


USTRUCT(BlueprintType)
struct GRAPHWEAVERPLUGIN_API FNamesInputNodeMirror
{
	GENERATED_BODY()
	//序列化保留,用于后续重构时校验

	///////////////////////////
	//The name defined by the user for this node. It can be retrieved via FunctionTools::GetSelfNameFromDescription().
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|NamesInputNodeMirror")
	FString SelfName = "None";

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|NamesInputNodeMirror")
	TArray<FString> ParentNames;

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|NamesInputNodeMirror")
	TArray<FString> BroNames;

	void operator=(const FNamesInputNode& Source);
	void operator=(const FNamesInputNodeMirror& o)
	{
		SelfName = o.SelfName;
		ParentNames = o.ParentNames;
		BroNames = o.BroNames;
	}
};


USTRUCT(BlueprintType)
struct GRAPHWEAVERPLUGIN_API FConstructConfig
{
	GENERATED_BODY()
	
	// Set this option to false if you want to use the 'Names' construction method but do not want to use rule-based naming, to reduce memory usage;
	// otherwise, set it to true. True will speed up construction but increase memory usage, and the difference will be more noticeable
	// when many 'GraphNode's are involved in building the same 'GraphView'.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "NamingOfRules"))
	bool NamingOfRules = true;

	//The number of characters at the beginning of a name that defines a family.
	//For example, if you have AA2, AA3; BB2, BB3, the Precision value is 2.
	//If you have AA1 and AB2, the Precision value can be 1 (family character A) or 2 (family characters AA and AB).
	//Note that this option only takes effect when 'ConstructMethod' is set to 'Names' and 'NamingOfRules' is true.
	//If this value exceeds the length of a name, the entire name is copied. If the value is less than 1, it defaults to 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "Precision"))
	int32 Precision = 1;

	void operator=(const FConstructConfig& other);
};



//不使用标准的TMap，因为不能对TMap<CHAR,TArray<int32>>使用UPROPERTY修饰，后果:当角色(容器)死亡的时候，变量FirstWordOfName会消亡，导致下一次构建图的时候(采用LHCode_G的方式)构建速度会很慢

USTRUCT(BlueprintType)
struct GRAPHWEAVERPLUGIN_API FClanInfo_GraphWeaver
{
	GENERATED_BODY()

	//Stores the clan name
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphViewSimpleMap")
	FString ClanName;

	//The Index of each member in the clan within 'RealNodes'
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphViewSimpleMap")
	TArray<int32> Indices;

	FClanInfo_GraphWeaver() = default;
	FClanInfo_GraphWeaver(FString& TargetKey, int32 Index, UGraphView* TargetView);
	~FClanInfo_GraphWeaver() = default;
};

//Used to describe one's relationship with a single family member
USTRUCT(BlueprintType)
struct GRAPHWEAVERPLUGIN_API FKinInfo_GraphWeaver
{
	GENERATED_BODY()

	//The subscript of family members in RealNodes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphViewSimplePair")
	int32 IndexInRealNodes;

	//'Ranking' should be treated as an Index. The minimum value is 0, not 1.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphViewSimplePair")
	int32 Ranking;

	FKinInfo_GraphWeaver()
	{
		IndexInRealNodes = -1;
		Ranking = -1;
	}
	FKinInfo_GraphWeaver(int32 Index, int32 _Ranking) : IndexInRealNodes(Index), Ranking(_Ranking){}
	bool operator==(int32 OtherIndex) const
	{
		return IndexInRealNodes == OtherIndex;
	}
};

//Used to describe one's relationship with all family members
USTRUCT(BlueprintType)
struct GRAPHWEAVERPLUGIN_API FNodeKinInfoPtr_GraphWeaver
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DescriptionPtr")
	TArray<FKinInfo_GraphWeaver> Parents;
	
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DescriptionPtr")
	TArray<FKinInfo_GraphWeaver> Children;
	
	//Only one party needs to point to the other. For example, if A1's Bro specifies A2, there is no need for A2 to reversely specify A1 in A2's Bro.
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DescriptionPtr")
	TArray<FKinInfo_GraphWeaver> Brothers;
};

//用于存储节点信息的基础类

//Base class for storing node information.
//When customizing an information carrier, it should inherit from this class
UCLASS(DisplayName = "NodeInfoBase")
class GRAPHWEAVERPLUGIN_API UNodeInfoBase_GraphWeaver : public UObject
{
	GENERATED_BODY()
public:
	//Only relationships that have been confirmed and connected are stored here; otherwise, they remain in the 'Mirror' containers above.
	//For example, given three nodes 'A1', 'A2', and 'A3', where 'A1' is the parent of 'A2' and 'A2' is the parent of 'A3',
	//but only 'A2' and 'A3' have participated in the construction so far, then only 'A3's 'Family' will contain 'A2', while 'A2's 'Family' will be empty.
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description")
	FNodeKinInfoPtr_GraphWeaver Family;

	//The source 'GraphNode' passed in for construction. This target will be automatically updated each time the graph is rebuilt.
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description", Transient)
	TObjectPtr<UGraphNode> SourceGraphNode;
	
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description")
	int32 IndexInRealNodes;

	//The tags, markers, etc., that you want to add to this 'NodeInfo'.
	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|Description")
	FGameplayTagContainer Tags;

	//Debug name for 'Description', format: 01Player_C_0_Script
	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|Description")
	FString ExplicitName;

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description")
	FNamesInputNodeMirror NamesInputNodeMirror;
	
	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|Description")
	bool Activated;
	
	virtual ~UNodeInfoBase_GraphWeaver() override {}
	UNodeInfoBase_GraphWeaver()
	{
		SourceGraphNode = nullptr;
		IndexInRealNodes = -1;
		Activated = false;
	}
};


//Used to store the basic information of 'GraphView' for serialization and deserialization (excluding the information of each 'NodeInfo').
USTRUCT(BlueprintType)
struct GRAPHWEAVERPLUGIN_API FDCBasicInfoForWholeView
{
	GENERATED_BODY()

	//以下某些参数通过SpawnGraphView节点传入,不需要该结构体存储该信息以减小内存占用
	//SelfOwner, GraphViewName, NamesConstructConfig, ConstructMethod,
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<FClanInfo_GraphWeaver> Clans;//用于重建图的时候加快构建速度

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<int32> WillVerticalAwakeNode;//用于重建图

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<int32> WillHorizontalAwakeNode;//用于重建图

	//The 'SelfName' of nodes removed via the 'RemoveNodes' function will be recorded here.
	//Recorded nodes will no longer actively participate in GraphView construction unless you explicitly clear this array along with 'RealNodes'.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<FString> NodeRemoved;
	
	FDCBasicInfoForWholeView(){}
};



//注意，因为在运行时可能改变GraphView的值，因此不要尝试在代码里面通过UGraphView::StaticClass()来获取CDO，这样会导致配置数据不一致。

UCLASS(BlueprintType)
class GRAPHWEAVERPLUGIN_API UGraphView : public UObject
{
	GENERATED_BODY()
public:
	UGraphView(const FObjectInitializer& ObjectInitializer);
	virtual ~UGraphView() override;

	int32 AddNodeInfo_Construct()
	{
		return RealNodes.Emplace(NewObject<UNodeInfoBase_GraphWeaver>(this, RealNodesType, "Root"));
	}
	//I really can't find a pooling technique that allows manual placement new, meow (´⌒`｡)
	int32 AddNodeInfo()
	{
		return RealNodes.Emplace(NewObject<UNodeInfoBase_GraphWeaver>(this, RealNodesType));
	}
	template<typename T = UNodeInfoBase_GraphWeaver>
	T* GetNodeInfoAs(int32 Index)
	{
		return static_cast<T*>(RealNodes[Index].Get());
	}
public:
	//Used to record the meta-type of the actual NodeInfo.
	UPROPERTY(BlueprintReadOnly ,Category = "GraphWeaver|GraphView")
	TObjectPtr<UClass> RealNodesType;

public:
	//所有的ExposeOnSpawn并不是真的要生成引脚，只是为了用来作为UK2Node_SpawnGraph的引脚
	
	//External Owner.Automatically set by the blueprint node 'SpawnGraphView'
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView", meta = (ExposeOnSpawn = "Owner"))
	UObject* Owner;
	
	//The name of this 'GraphView'. Can only be changed within the 'GraphViewName' of the 'SpawnGraphView' Blueprint node.
	//Please check the naming rules by hovering over 'GraphViewName'.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView", meta = (ExposeOnSpawn = "GraphViewName"))
	FString GraphViewName;

	//在UFunctionTools_GraphWeaver::ModGraphViewNaCon_NotManuallyCall里面设置,当Precision <= 1 的时候会自动设置为 1
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphView", meta = (ExposeOnSpawn = "NamesConstructConfig"))
	FConstructConfig ConstructConfig;

	//只有在NamesConstructConfig.NamingOfRules == true 的时候Clans才会被使用
	
	///////////////////////////////////////////////////////////////////////////////
	//This variable assists in constructing the 'GraphView' when 'ConstructMethod' is 'Names' and 'NamingOfRules' is true.
	//Users are only recommended to use it for debugging (although you cannot modify this value directly).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TArray<FClanInfo_GraphWeaver> Clans;
	
	//The actual storage for graph data. 'GraphNode' is only responsible for passing data to this array.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TArray<TObjectPtr<UNodeInfoBase_GraphWeaver>> RealNodes;

	// The index of the GraphNode node in AlreadyRecorded where the parent node is not fully linked
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TArray<int32> WillVerticalAwakeNode;
	// The index of the GraphNode node in AlreadyRecorded whose sibling nodes have not been fully linked
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TArray<int32> WillHorizontalAwakeNode;

	//注意一下,这个不要夜不能被序列化,防止在NamesConstructWay和AddNewNodeIntelligent等函数里面被滥用

	/////////////////////////////////////////////////////////////////////////////
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TEnumAsByte<NAGraphConstructErrorCode::EErrorCodeForConstructView> ErrorCodeForConstructView;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphView")
	TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> WayToDealSameNode;

	//The 'SelfName' of nodes removed via the 'RemoveNodes' function will be recorded here.
	//Recorded nodes will no longer actively participate in GraphView construction unless you explicitly clear this array along with 'RealNodes'.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphView")
	TArray<FString> NodeRemoved;
public:
	enum CheckResult
	{
		Non, Repeat, HasError, Undefined,
	};
	CheckResult CheckSameNode_Names(UGraphNode* Node);
	bool CheckIsNodeRemoved(UGraphNode* Node);
	
	void DealingWithParent_ChildRelationships(int32 ParentIndex, int32 ChildIndex);
	void DealingWithBrothersRelationships(int32 BroIndex, int32 SelfIndex);

	void RemoveDeletedRelationship(UGraphNode* Node);
	CheckResult NamesConstructWay(UGraphNode* TargetNode);

	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView", meta = (ToolTip = "Set the initial capacity in advance to speed up the construction process. Automatically called by the 'SpawnGraphView' blueprint node."))
	void AllocateGraphViewSize(int32 Size);

	void LogByErrorCode(UGraphNode* NewNode);
	
	//Add a new 'GraphNode' to the 'GraphView'. Generally, you should not call this function manually;
	//instead, it should be automatically invoked by setting the 'AutoBuild' parameter to true in 'SpawnGraphNode'.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	bool AddNewNodeIntelligent(UGraphNode* NewNode);
	//Re-add a previously deleted 'GraphNode' back into the 'GraphView'.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	bool ReAddNode(UGraphNode* Node);
	
	//Set a new value for 'WayToDealSameNode'.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	void SetWayToDealSameNode(TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> Way);

	//检查Ranking是否合理
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphView", meta = (BlueprintInternalUseOnly = "true"))
	static bool ValidateRankingConsistency_Inner(UGraphView* GraphView, UPARAM(ref)TArray<int32>& IgnoredNodeIndices);
	//检查Ranking是否合理
	bool ValidateRankingConsistency(TArray<int32>&& IgnoredNodeIndices);
	bool ValidateRankingConsistency(TArray<int32>& IgnoredNodeIndices);
	
	//只检查Ranking是否存在一个错误,不关心谁错,也不进行日志打印
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphView", meta = (BlueprintInternalUseOnly = "true"))
	static bool ValidateRankingConsistencyLight_Inner(UGraphView* GraphView, UPARAM(ref)TArray<int32>& IgnoredNodeIndices);
	bool ValidateRankingConsistencyLight(TArray<int32>&& IgnoredNodeIndices);
	bool ValidateRankingConsistencyLight(TArray<int32>& IgnoredNodeIndices);
	void AddRemovedNodeName(TObjectPtr<UNodeInfoBase_GraphWeaver> Node);
	//序列化和反序列化
public:
	//This function is only used to obtain the basic data for the serialization and deserialization of GraphView.
	//If you need to also back up the "NodeInfo" for each node, please additionally call the function "GetDCStruct"
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	FDCBasicInfoForWholeView GetBasicDCForWholeView();

	//Deserialize to initialize the basic data of 'GraphView'.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	void ResetBasicInformFromBasicDC(UPARAM(ref)FDCBasicInfoForWholeView& BasicDC);

	//Reset the entire 'GraphView' to its initial state.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	void ResetView()
	{
		Clans.Empty();
		FString RootClanName = "RTest";
		Clans.Emplace(RootClanName, 0, this);
		RealNodes.Empty();
		AddNodeInfo_Construct();
		RealNodes[0]->Activated = true;
		RealNodes[0].Get()->NamesInputNodeMirror.SelfName = "Root";
		RealNodes[0]->ExplicitName = "Root_Script";
		RealNodes[0]->IndexInRealNodes = 0;
		WillVerticalAwakeNode.Empty();
		WillHorizontalAwakeNode.Empty();
		ErrorCodeForConstructView = NAGraphConstructErrorCode::None;
		NodeRemoved.Empty();
	}
};
#endif


//如果修改这个名字,需要同步修改StructGenerator的内容
///////////////////////////////////////////////////////////////////////

//DC stands for Data Carrier.
//A carrier used to save and load each 'NodeInfo' node's information during the archiving process.
//When customizing an information carrier, it should inherit from this class
USTRUCT(BlueprintType, DisplayName = "DCStruct")
struct GRAPHWEAVERPLUGIN_API FDCStruct_GraphWeaver  //Basic Basic Basic Basic Basic Basic Basic Basic Basic Basic Basic Basic 
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Save")
	FNamesInputNodeMirror Names;
	
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DataCarrier")
	FNodeKinInfoPtr_GraphWeaver Family;
	
	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|DataCarrier")
	FGameplayTagContainer Tags;

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DataCarrier")
	int32 IndexInRealNodes;

	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|DataCarrier")
	bool Activated;

	FDCStruct_GraphWeaver()
	{
		IndexInRealNodes = -1;
		Activated = false;
	}
	FDCStruct_GraphWeaver(TObjectPtr<UNodeInfoBase_GraphWeaver> NodeInfo)
	{
		Names = NodeInfo->NamesInputNodeMirror;
		Family = NodeInfo->Family;
		Tags = NodeInfo->Tags;
		IndexInRealNodes = NodeInfo->IndexInRealNodes;
		Activated = NodeInfo->Activated;
	}
	virtual ~FDCStruct_GraphWeaver(){}

	virtual void ResetNodeInfo(TObjectPtr<UNodeInfoBase_GraphWeaver> NodeInfo)
	{
		NodeInfo->NamesInputNodeMirror = Names;
		NodeInfo->Family = Family;
		NodeInfo->Tags = Tags;
		NodeInfo->IndexInRealNodes = IndexInRealNodes;
		NodeInfo->Activated = Activated;
	}
};



//A tool class used for creating and providing FDataCarrier in the blueprint.
//Under no circumstances should you directly use this class anywhere!!!
UCLASS(HideDropdown)
class GRAPHWEAVERPLUGIN_API UDCWrapper_GraphWeaver : public UObject  //Basic Basic Basic Basic Basic Basic Basic Basic Basic Basic Basic Basic 
{
	GENERATED_BODY()
public:
	//GetDCStruct函数不能使用覆写，否则只能调用到父类的函数。不能返回指针，因为结构体不允许返回指针。不能返回对象，因为不满足协变返回的要求
	virtual UScriptStruct* GetStructType()
	{
		return nullptr;
	}
	virtual ~UDCWrapper_GraphWeaver() override {}
};

//A 'DCWrapper' used to hold/load the 'FDCStruct_GraphWeaver'.
UCLASS(BlueprintType, DisplayName = "DCWrapperForDefaultStruct")//反射
class GRAPHWEAVERPLUGIN_API UDCWrapperForDefaultStruct_GraphWeaver : public UDCWrapper_GraphWeaver
{
	GENERATED_BODY()
public:
	UDCWrapperForDefaultStruct_GraphWeaver(){}
	virtual UScriptStruct* GetStructType() override
	{
		return FDCStruct_GraphWeaver::StaticStruct();
	}
	virtual ~UDCWrapperForDefaultStruct_GraphWeaver() override {}

	//必须设置为static来让UK2Node_CallFunction调用
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|Save", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FDCStruct_GraphWeaver> GetDCStruct_UDCWrapperForDefaultStruct_GraphWeaver(UGraphView* GraphView)
	{
		int32 ConstructNum = GraphView->RealNodes.Num();
		TArray<FDCStruct_GraphWeaver> rr;
		rr.Reserve(ConstructNum);//Reserve不调用构造函数不修改Num
		for (int i = 0; i < ConstructNum; i++)
			rr.Emplace(GraphView->RealNodes[i]);
		return rr;
	}

	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|Save", meta = (BlueprintInternalUseOnly = "true"))
	static void ResetPerNode_UDCWrapperForDefaultStruct_GraphWeaver(UGraphView* GraphView, UPARAM(ref)TArray<FDCStruct_GraphWeaver>& DCArray)
	{
		for (int i = 1 ; i < DCArray.Num() ; i++)
		{
			GraphView->AddNodeInfo();
		}
		for (int32 i = 0; i < DCArray.Num(); i++)
		{
			DCArray[i].ResetNodeInfo(GraphView->RealNodes[i]);
		}
	}
};


	//一定不能是引用、指针，因为这两种类型不能参与序列化、反序列化。注意，你需要自己手动定义结构体的析构函数
	#define MEMBER_DECL(type, name) UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DataCarrier") \
		type name;
	#define FP_1(action, type, name) action(type, name)
	#define FP_2(action, type, name, ...) action(type, name) FP_1(action, __VA_ARGS__)
	#define FP_3(action, type, name, ...) action(type, name) FP_2(action, __VA_ARGS__)
	#define FP_4(action, type, name, ...) action(type, name) FP_3(action, __VA_ARGS__)
	#define FP_5(action, type, name, ...) action(type, name) FP_4(action, __VA_ARGS__)
	#define FP_6(action, type, name, ...) action(type, name) FP_5(action, __VA_ARGS__)
	#define FP_7(action, type, name, ...) action(type, name) FP_6(action, __VA_ARGS__)
	#define FP_8(action, type, name, ...) action(type, name) FP_7(action, __VA_ARGS__)
	#define FP_9(action, type, name, ...) action(type, name) FP_8(action, __VA_ARGS__)
	#define FP_10(action, type, name, ...) action(type, name) FP_9(action, __VA_ARGS__)

	#define PASTETWOTEXTS(a, b)  a ## b
	#define CONCAT(a, b)        PASTETWOTEXTS(a, b)

	#define FP_COUNT_INNER(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20, N, ...) N
	#define FP_COUNT(...) FP_COUNT_INNER(__VA_ARGS__, 10,9,9,8,8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0,0)

	#define CHOOSE_FP_N(...) CONCAT(FP_, FP_COUNT(__VA_ARGS__))(MEMBER_DECL, __VA_ARGS__)












