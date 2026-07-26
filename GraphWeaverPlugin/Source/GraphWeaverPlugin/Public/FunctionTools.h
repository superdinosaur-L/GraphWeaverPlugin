// Copyright 2026 RainButterfly. All Rights Reserved.
#pragma once


#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FunctionTools.generated.h"

namespace NAWayToDealSameGraphNode
{
	enum EWayToDealSameGraphNode : uint8;
}

class UGraphNode;
class UGraphView;
struct FConstructConfig;
struct FNamesInputNode;
class UNodeInfoBase_GraphWeaver;

class RealRunView
{
public:
	static RealRunView& Get()
	{
		static RealRunView Instance;
		return Instance;
	}

	TArray<UGraphView*> GetArray()
	{
		FScopeLock Lock(&Mutex);
		return Views;
	}

	void AddView(UGraphView* View)
	{
		FScopeLock Lock(&Mutex);
		Views.Emplace(View);
	}

	void RemoveView(UGraphView* View)
	{
		FScopeLock Lock(&Mutex);
		Views.RemoveSingleSwap(View);
	}
private:
	TArray<UGraphView*> Views;
	mutable FCriticalSection Mutex;
};


UCLASS()
class GRAPHWEAVERPLUGIN_API UFunctionTools_GraphWeaver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFunctionTools_GraphWeaver();
	virtual ~UFunctionTools_GraphWeaver() override;
	
	//下面的这些函数之所以要传入SourceGraphView是因为不敢保证在调用这些函数的时候Description的SourceGraphNode是有效的,但是敢保证SourceGraphView一定是有效的

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//Gets all child Descriptions of the specified Description with Activated set to true, and returns their Indexes in RealNodes.Do not include self.
	//Warning: If you use a structure like "A is B's parent, B is C's parent, C is A's parent" that causes the parent chain to form a loop,
	//do NOT use this method! Otherwise, it will cause a stack overflow and lead to program crash.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<int32> ObtainAllActivatedChildNodeInfo(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo);

	//Gets all sibling Descriptions of the specified Description with Activated set to true.Do not include self.
	//Unlike the function ObtainAllActivatedChildDescription, this function allows loops in sibling relationships.
	//For example, if you record that A is B's sibling, B is C's sibling, and C is A's sibling, and both B and C have Activated set to true,
	//then inputting A will return the Indexes of B and C in RealNodes.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<int32> ObtainAllActivatedBroNodeInfo(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo);

	//Only retrieves nodes that have a direct sibling or parent-child relationship with the currently specified node.
	//For example, if A is passed in, B is A's sibling node, C is A's child, and D is B's child. Assuming B, C, and D all have Activated set to true,
	//this function will only return B and C; D will not be returned as it has no direct relationship with A.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<int32> ObtainDirectActivatedChildAndBroNodeInfo(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo);

	//Retrieve all Child entries of a given 'Description'
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<int32> ObtainAllChildNodeInfo(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo);

	// Each GraphNodeDescription stored in the GraphView has a 'Ranking' indicating its position within a certain object.  
	// After certain operations, such as calling the 'RemoveNodes' function, these Ranking values may change, requiring this function to correct them.  
	// Each call to this function may incur significant performance overhead, so it is recommended not to call it frequently unless necessary.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionTools")
	static void FixupRanking(UGraphView* DisorderedView);
	
	//Removes the specified node and clears it from 'RealNodes'. If 'RemoveChildren' is false, only the node itself is removed from 'RealNodes'.
	//Otherwise, all associated child nodes will be removed as well. This function automatically handles the relationships between nodes.
	//If 'ReorderRanking' is true, it may incur significant performance overhead. If you need to delete multiple nodes multiple times without immediately
	//using the 'Family.Ranking' property, it is recommended to manually call 'FixupRanking' once after all deletion operations are complete.
	//When RemoveChildren is false, FixupRanking will automatically force-correct the Ranking values.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionTools")
	static void RemoveNodes(UGraphView* SourceGraphView, UNodeInfoBase_GraphWeaver* NodeInfo, bool RemoveChildren = true, bool ReorderRanking = false);
	
	//Retrieve the corresponding GraphView and the Index of the Description within the GraphView from a GraphNode
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static UGraphView* GetViewAndIndexFromNode(UGraphNode* Target, int32& Index);
public:
	//打印变量类型
	template<typename T>
	static FString GetTypeName(const T& Var)
	{
		return UTF8_TO_TCHAR(typeid(T).name());
	}

	template<typename T>
	static TArray<T> ArrayLeftSplit(TArray<T>& SourceArray, int32 LeftArrayNum)
	{
		TArray<T> Result;
		int32 Num = 0;
		for (auto &i : SourceArray)
		{
			Num++;
			if (Num <= LeftArrayNum)
				Result.Emplace(i);
		}
		return Result;
	}

	//获取某个数组的最后几个元素
	template<typename T>
	static TArray<T> ArrayLastSeveral(TArray<T>& SourceArray, int32 LeftNum)
	{
		TArray<T> Result;
		if (SourceArray.Num() <= LeftNum)
			return Result;
		for( ; LeftNum < SourceArray.Num() ; LeftNum++)
		{
			Result.Emplace(SourceArray[LeftNum]);
		}
		return Result;
	}

	template<typename T>
	static TArray<T> ArrayNotInclude(TArray<T>& SourceArray, TArray<T>& IncludeArray)
	{
		TArray<T> rr;
		uint8 bIsOk = 1;
		for (auto& i : SourceArray)
		{
			bIsOk = 1;
			for (auto& j : IncludeArray)
			{
				if (i == j)
				{
					bIsOk = 0;
					break ;
				}
			}
			if (bIsOk)
				rr.Emplace(i);
		}
		return rr;
	}
	template<typename T>
	static T GetArrayLastElem(TArray<T>& Array)
	{
		return Array[Array.Num() - 1];
	}
};

inline int32 DefaultObjectNamePrefix = 9;

UCLASS()
class GRAPHWEAVERPLUGIN_API UFunctionForUK2Node : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//仅在UK2Node里面占位使用
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static void NonFunction();

	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<int32> GetEmptyIntArray();
public://common part
	static FString GetClassCPPNameFromDefaultObject(const FString& InName);

	
public://SpawnGraphView
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphView* SpawnViewAndSetBasicProperty(UObject* Outer, FString GraphViewName, NAWayToDealSameGraphNode::EWayToDealSameGraphNode WayToDealSameNode
		, UClass* RealNodesType, UClass* DCWrapperType, int32 AllocateSize, FConstructConfig ConstructConfig);

	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphNode* CallProcessInformAuto(UGraphNode* Target, bool Call);

public://SpawnGraphNode
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphNode* SpawnNodeAndSetBasicProperty(UObject* Outer, UPARAM(ref)FNamesInputNode& NamesInput, FString ViewName);
	
public://GetDCStruct
	//生成指定类型的FDCStruct_GraphWeaver
	//UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	//static void GetDCStruct();
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static void LogClassDefaultObjectName(UObject* o)
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *o->GetClass()->GetDefaultObjectName().ToString());
	}
};




















