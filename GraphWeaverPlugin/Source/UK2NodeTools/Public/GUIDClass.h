// Copyright 2026 RainButterfly. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GUIDClass.generated.h"

UCLASS(Config=EditorPerProjectUserSettings)
class UGraphWeaverPerUserGuid : public UObject
{
    GENERATED_BODY()

public:

    // 保存到 ini
    UPROPERTY(Config)
    FGuid UserGuid;
};

