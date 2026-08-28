// =============================================================================
// NebulaForgeAIContextCollector.h
// =============================================================================
// Gathers opted-in editor context into a compact, explicit envelope
// (plan section 6.1). Every collected item becomes a removable UI chip.
//
// Collection runs on the game thread; results are delivered through a
// callback marshalled with AsyncTask(GameThread).
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"

class FNebulaForgeAIContextCollector
{
public:
    /** Context kinds the user can attach before sending. */
    enum class EContextKind : uint8
    {
        ProjectInfo = 0,
        CurrentLevel = 1,
        SelectedActors = 2,
        SelectedAssets = 3,
        EditorMode = 4,
        OutputLog = 5,
        Screenshot = 6
    };

    /**
     * Collect the requested context kinds (subject to privacy/permission
     * settings) and invoke OnCollected on the game thread.
     */
    void Collect(const TArray<EContextKind>& Kinds, TFunction<void(const TArray<FNebulaAIContextChip>&)> OnCollected);

    /** Human-readable label for a context kind. */
    static FText GetKindLabel(EContextKind Kind);

private:
    FNebulaAIContextChip BuildProjectInfoChip() const;
    FNebulaAIContextChip BuildCurrentLevelChip() const;
    FNebulaAIContextChip BuildSelectedActorsChip() const;
    FNebulaAIContextChip BuildSelectedAssetsChip() const;
    FNebulaAIContextChip BuildEditorModeChip() const;
    FNebulaAIContextChip BuildOutputLogChip() const;
};
