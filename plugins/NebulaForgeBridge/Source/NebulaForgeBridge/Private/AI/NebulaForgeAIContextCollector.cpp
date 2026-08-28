#include "AI/NebulaForgeAIContextCollector.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "NebulaForgeAIService.h"
#include "NebulaForgeAISettings.h"
#include "Async/Async.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    FString JsonObjectToString(const TSharedRef<FJsonObject>& Obj)
    {
        FString Out;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
        FJsonSerializer::Serialize(Obj, Writer);
        return Out;
    }
}

void FNebulaForgeAIContextCollector::Collect(
    const TArray<EContextKind>& Kinds, TFunction<void(const TArray<FNebulaAIContextChip>&)> OnCollected)
{
    // Marshal collection to the game thread, then deliver the result there.
    AsyncTask(ENamedThreads::GameThread, [this, Kinds, OnCollected]()
    {
        const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
        TArray<FNebulaAIContextChip> Chips;
        if (!Settings || !Settings->Privacy.bSendProjectContext)
        {
            AsyncTask(ENamedThreads::GameThread, [OnCollected, Chips]()
            {
                OnCollected(Chips);
            });
            return;
        }

        for (EContextKind Kind : Kinds)
        {
            switch (Kind)
            {
            case EContextKind::ProjectInfo:
                Chips.Add(BuildProjectInfoChip());
                break;
            case EContextKind::CurrentLevel:
                Chips.Add(BuildCurrentLevelChip());
                break;
            case EContextKind::SelectedActors:
                if (Settings->Permissions.bReadSelection)
                {
                    Chips.Add(BuildSelectedActorsChip());
                }
                break;
            case EContextKind::SelectedAssets:
                if (Settings->Permissions.bReadSelection)
                {
                    Chips.Add(BuildSelectedAssetsChip());
                }
                break;
            case EContextKind::EditorMode:
                Chips.Add(BuildEditorModeChip());
                break;
            case EContextKind::OutputLog:
                if (Settings->Privacy.bIncludeOutputLog && Settings->Permissions.bReadOutputLog)
                {
                    Chips.Add(BuildOutputLogChip());
                }
                break;
            case EContextKind::Screenshot:
                // v1 collects a placeholder description; sending raw
                // screenshots requires the privacy switch which defaults off.
                if (Settings->Privacy.bSendScreenshots && Settings->Permissions.bReadViewportScreenshot)
                {
                    FNebulaAIContextChip Chip;
                    Chip.Id = TEXT("screenshot");
                    Chip.Label = TEXT("Screenshot");
                    Chip.PayloadJson = TEXT("{\"note\":\"viewport screenshot attachments arrive in a later release\"}");
                    Chip.EstimatedTokens = FNebulaAIDiagnostics::EstimateTokens(Chip.PayloadJson);
                    Chips.Add(MoveTemp(Chip));
                }
                break;
            default:
                break;
            }
        }

        OnCollected(Chips);
    });
}

FText FNebulaForgeAIContextCollector::GetKindLabel(EContextKind Kind)
{
    switch (Kind)
    {
    case EContextKind::CurrentLevel: return NSLOCTEXT("NebulaForgeAI", "CtxCurrentLevel", "Current level");
    case EContextKind::SelectedActors: return NSLOCTEXT("NebulaForgeAI", "CtxSelectedActors", "Selected actors");
    case EContextKind::SelectedAssets: return NSLOCTEXT("NebulaForgeAI", "CtxSelectedAssets", "Selected assets");
    case EContextKind::EditorMode: return NSLOCTEXT("NebulaForgeAI", "CtxEditorMode", "Editor mode");
    case EContextKind::OutputLog: return NSLOCTEXT("NebulaForgeAI", "CtxOutputLog", "Output log");
    case EContextKind::Screenshot: return NSLOCTEXT("NebulaForgeAI", "CtxScreenshot", "Screenshot");
    case EContextKind::ProjectInfo:
    default: return NSLOCTEXT("NebulaForgeAI", "CtxProjectInfo", "Project info");
    }
}

FNebulaAIContextChip FNebulaForgeAIContextCollector::BuildProjectInfoChip() const
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("projectName"), FApp::GetProjectName());
    Obj->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString(EVersionComponent::Minor));

    FNebulaAIContextChip Chip;
    Chip.Id = TEXT("project");
    Chip.Label = FApp::GetProjectName();
    Chip.PayloadJson = JsonObjectToString(Obj);
    Chip.EstimatedTokens = FNebulaAIDiagnostics::EstimateTokens(Chip.PayloadJson);
    return Chip;
}

FNebulaAIContextChip FNebulaForgeAIContextCollector::BuildCurrentLevelChip() const
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (GEditor)
    {
        if (const UWorld* World = GEditor->GetEditorWorldContext().World())
        {
            Obj->SetStringField(TEXT("mapName"), World->GetMapName());
            FString WorldTypeName;
            switch (World->WorldType)
            {
            case EWorldType::Editor: WorldTypeName = TEXT("Editor"); break;
            case EWorldType::Game: WorldTypeName = TEXT("Game"); break;
            case EWorldType::PIE: WorldTypeName = TEXT("PIE"); break;
            case EWorldType::EditorPreview: WorldTypeName = TEXT("EditorPreview"); break;
            case EWorldType::GamePreview: WorldTypeName = TEXT("GamePreview"); break;
            case EWorldType::Inactive: WorldTypeName = TEXT("Inactive"); break;
            case EWorldType::GameRPC: WorldTypeName = TEXT("GameRPC"); break;
            default: WorldTypeName = TEXT("None"); break;
            }
            Obj->SetStringField(TEXT("worldType"), WorldTypeName);
            Obj->SetBoolField(TEXT("isPlayInEditor"), World->WorldType == EWorldType::PIE);
            int32 ActorCount = 0;
            for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
            {
                ++ActorCount;
            }
            Obj->SetNumberField(TEXT("actorCount"), ActorCount);
        }
    }

    FNebulaAIContextChip Chip;
    Chip.Id = TEXT("level");
    Chip.Label = Obj->HasField(TEXT("mapName"))
        ? Obj->GetStringField(TEXT("mapName"))
        : TEXT("(no level)");
    Chip.PayloadJson = JsonObjectToString(Obj);
    Chip.EstimatedTokens = FNebulaAIDiagnostics::EstimateTokens(Chip.PayloadJson);
    return Chip;
}

FNebulaAIContextChip FNebulaForgeAIContextCollector::BuildSelectedActorsChip() const
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> ActorsJson;
    int32 Count = 0;

    if (GEditor && GEditor->GetSelectedActors())
    {
        for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
        {
            AActor* Actor = Cast<AActor>(*It);
            if (!Actor || Count >= 20)
            {
                break;
            }
            ++Count;
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), Actor->GetName());
            Entry->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
            Entry->SetStringField(TEXT("path"), Actor->GetPathName());
            Entry->SetStringField(TEXT("location"), Actor->GetActorLocation().ToString());
            Entry->SetStringField(TEXT("rotation"), Actor->GetActorRotation().ToString());
            Entry->SetNumberField(TEXT("components"), Actor->GetComponents().Num());
            ActorsJson.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }
    Obj->SetArrayField(TEXT("actors"), ActorsJson);
    Obj->SetNumberField(TEXT("count"), Count);

    FNebulaAIContextChip Chip;
    Chip.Id = TEXT("selection-actors");
    Chip.Label = FString::Printf(TEXT("%d actor(s) selected"), Count);
    Chip.PayloadJson = JsonObjectToString(Obj);
    Chip.EstimatedTokens = FNebulaAIDiagnostics::EstimateTokens(Chip.PayloadJson);
    return Chip;
}

FNebulaAIContextChip FNebulaForgeAIContextCollector::BuildSelectedAssetsChip() const
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> AssetsJson;
    int32 Count = 0;

    if (GEditor && GEditor->GetSelectedObjects())
    {
        for (FSelectionIterator It(*GEditor->GetSelectedObjects()); It; ++It)
        {
            UObject* Object = Cast<UObject>(*It);
            if (!Object || !Object->IsAsset() || Count >= 20)
            {
                continue;
            }
            ++Count;
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("path"), Object->GetPathName());
            Entry->SetStringField(TEXT("class"), Object->GetClass()->GetName());
            Entry->SetBoolField(TEXT("dirty"), Object->GetOutermost()->IsDirty());
            AssetsJson.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }
    Obj->SetArrayField(TEXT("assets"), AssetsJson);
    Obj->SetNumberField(TEXT("count"), Count);

    FNebulaAIContextChip Chip;
    Chip.Id = TEXT("selection-assets");
    Chip.Label = FString::Printf(TEXT("%d asset(s) selected"), Count);
    Chip.PayloadJson = JsonObjectToString(Obj);
    Chip.EstimatedTokens = FNebulaAIDiagnostics::EstimateTokens(Chip.PayloadJson);
    return Chip;
}

FNebulaAIContextChip FNebulaForgeAIContextCollector::BuildEditorModeChip() const
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    const FEdMode* ActiveMode = GLevelEditorModeTools().GetActiveMode(FEditorModeID());
    Obj->SetStringField(TEXT("mode"), ActiveMode ? ActiveMode->GetID().ToString() : TEXT("Default"));

    FNebulaAIContextChip Chip;
    Chip.Id = TEXT("editor-mode");
    Chip.Label = Obj->GetStringField(TEXT("mode"));
    Chip.PayloadJson = JsonObjectToString(Obj);
    Chip.EstimatedTokens = FNebulaAIDiagnostics::EstimateTokens(Chip.PayloadJson);
    return Chip;
}

FNebulaAIContextChip FNebulaForgeAIContextCollector::BuildOutputLogChip() const
{
    // Recent warning/error lines, capped and redacted. The ring buffer is
    // owned by the AI service facade (declared in NebulaForgeAIService.h).
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();

    if (GNebulaAILogRingBuffer.IsValid())
    {
        const TArray<FString> Lines = GNebulaAILogRingBuffer->GetRecentLines(30);
        TArray<TSharedPtr<FJsonValue>> LineJson;
        for (const FString& Line : Lines)
        {
            LineJson.Add(MakeShared<FJsonValueString>(Line));
        }
        Obj->SetArrayField(TEXT("recentLines"), LineJson);
    }

    FNebulaAIContextChip Chip;
    Chip.Id = TEXT("output-log");
    Chip.Label = TEXT("Output log (recent)");
    Chip.PayloadJson = JsonObjectToString(Obj);
    Chip.EstimatedTokens = FNebulaAIDiagnostics::EstimateTokens(Chip.PayloadJson);
    return Chip;
}
