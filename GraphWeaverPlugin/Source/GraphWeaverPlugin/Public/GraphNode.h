// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GraphNode.generated.h"

/**
 * 
 */

class UGraphView;
class UNodeInfoBase_GraphWeaver;

USTRUCT(BlueprintType)
struct FNamesInputNode
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "SelfName"))
	FString SelfName = "None";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "ParentNodeNames"))
	TArray<FString> ParentNames;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "BroNames"))
	TArray<FString> BroNames;
};

//UGraphNode只是作为用户操作的一种载体，因为自身无法进行序列化和反序列化，所以在GraphView里面都是以镜像的方式来存储对应的GraphNode

/////////////////////////////////////////////////////////////////////////////////////////////////////////
UCLASS(BlueprintType)
class GRAPHWEAVERPLUGIN_API UGraphNode : public UObject
{
	GENERATED_BODY()
public:
	UGraphNode();
	virtual ~UGraphNode() override;

public:
	//The owner of this node. Automatically set in the function 'UK2Node_SpawnGraphNode->UFunctionTools::SpawnNodeAndSetBasicProperty'
	//(called automatically by the blueprint node 'SpawnGraphNode').
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphNode", meta = (ExposeOnSpawn = "SelfOuter"))
	UObject* Outer;

	//Which 'GraphView' does this 'GraphNode' and its corresponding 'Description' belong to.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphNode", meta = (ExposeOnSpawn = "SourceGraph"))
	UGraphView* SourceGraphView;

	//在UFunctionTools::ModNamesInput_NotManuallyCall里面自动设置

	//////////////////////////////////////////////////////////////////////////////
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphNode", meta = (ExposeOnSpawn = "NamesInput"))
	FNamesInputNode NamesInput;


	//FunctionTools.cpp::SpawnNodeAndSetBasicProperty中设置
	
	//A human-readable name for debugging purposes that reflects internal and external relationships.
	//Automatically set in UK2Node_SpawnGraphNode->FunctionTools.cpp::SpawnNodeAndSetBasicProperty.
	//Format: 01Player_C_0_GraphNode
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphNode")
	FString ExplicitName;

	//The mirrored Index of 'Description' within 'RealNodes' of 'SourceGraphView'
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphNode")
	int32 IndexInRealNodes = -1;
	
	//bool Recorded;
public:
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphNode")
	bool ProcessInformAuto(UGraphView* InGraph);

	//Retrieve the corresponding 'NodeInfo' from the 'RealNodes' collection of the 'GraphView'.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	UNodeInfoBase_GraphWeaver* ObtainSelfNodeInfo();

	//Gets all child NodeInfo of the specified NodeInfo with Activated set to true, and returns their Indexes in RealNodes.Do not include self.
	//Warning: If you use a structure like "A is B's parent, B is C's parent, C is A's parent" that causes the parent chain to form a loop,
	//do NOT use this method! Otherwise, it will cause a stack overflow and lead to program crash.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	TArray<int32> ObtainAllActivatedChildNodeInfo();
	
	//Gets all sibling Descriptions of the specified Description with Activated set to true.Do not include self.
	//Unlike the function ObtainAllActivatedChildDescription, this function allows loops in sibling relationships.
	//For example, if you record that A is B's sibling, B is C's sibling, and C is A's sibling, and both B and C have Activated set to true,
	//then inputting A will return the Indexes of B and C in RealNodes.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	TArray<int32> ObtainAllActivatedBroNodeInfo();

	//Only retrieves nodes that have a direct sibling or parent-child relationship with the currently specified node.
	//For example, if A is passed in, B is A's sibling node, C is A's child, and D is B's child. Assuming B, C, and D all have Activated set to true,
	//this function will only return B and C; D will not be returned as it has no direct relationship with A.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	TArray<int32> ObtainDirectActivatedChildAndBroNodeInfo();

	//Retrieve all Child entries of a given 'Description'
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	TArray<int32> ObtainAllChildNodeInfo();

	//Removes the specified node and clears it from 'RealNodes'. If 'RemoveChildren' is false, only the node itself is removed from 'RealNodes'.
	//Otherwise, all associated child nodes will be removed as well. This function automatically handles the relationships between nodes.
	//If 'ReorderRanking' is true, it may incur significant performance overhead. If you need to delete multiple nodes multiple times without immediately
	//using the 'Family.Ranking' property, it is recommended to manually call 'FixupRanking' once after all deletion operations are complete.
	//When RemoveChildren is false, FixupRanking will automatically force-correct the Ranking values.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphNode")
	void RemoveSelfInfo(bool RemoveChild = true, bool ReorderRanking = false);
};


















