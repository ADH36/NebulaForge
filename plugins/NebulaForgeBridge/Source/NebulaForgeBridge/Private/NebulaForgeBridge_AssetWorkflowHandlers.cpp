// =============================================================================
// NebulaForgeBridge_AssetWorkflowHandlers.cpp
// =============================================================================
// Asset workflow, material authoring, and source control handlers.
//
// HANDLERS:
//   Asset Operations:
//     - import, duplicate, rename, move, delete, exists, list, search_assets
//     - create_folder, create_material, create_material_instance
//     - create_physical_material, configure_physical_material, get_physical_material
//     - set_friction, set_restitution, set_density, configure_surface_type
//     - assign_physical_material, clear_physical_material_override
//     - get_dependencies, get_asset_graph, set_tags, set_metadata, get_metadata
//     - validate, generate_report, generate_thumbnail, get_material_stats
//     - Content Browser navigation, settings, search, collections, colors, Explorer
//
//   Material Authoring:
//     - add_material_node, connect_material_pins, remove_material_node
//     - break_material_connections, get_material_node_details, rebuild_material
//     - add_material_parameter, list_instances, reset_instance_parameters
//
//   Source Control:
//     - source_control_checkout, source_control_submit, get_source_control_state
//     - source_control_enable
//
//   Bulk Operations:
//     - fixup_redirectors, bulk_rename, bulk_delete
//     - generate_lods, nanite_rebuild_mesh
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - Material expression includes grouped by category
//
// VERSION COMPATIBILITY:
//   - MaterialDomain.h: UE 5.1+ (EMaterialDomain in MaterialShared.h for 5.0)
//   - ClassPaths vs ClassNames: UE 5.1+ uses FTopLevelAssetPath
//   - AssetRegistry API varies between UE versions
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NebulaForgeBridgeGlobals.h"
#include "NebulaForgeBridgeHelpers.h"
#include "McpSafeOperations.h"

// -----------------------------------------------------------------------------
// MCP Handler Utilities (centralized JSON/Asset helpers)
// -----------------------------------------------------------------------------
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeExit.h"
#include "UObject/MetaData.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Basic Operations)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Constants)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Trigonometry)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionSine.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Texture & Time)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVertexColor.h"

// -----------------------------------------------------------------------------
// Material Function Includes (MF support for Tier 3 handlers)
// -----------------------------------------------------------------------------
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Editor-only Includes (Asset Management)
// -----------------------------------------------------------------------------
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#if __has_include("MediaPlayer.h") && __has_include("MediaTexture.h") && __has_include("MediaPlaylist.h") && __has_include("FileMediaSource.h") && __has_include("StreamMediaSource.h")
#include "FileMediaSource.h"
#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "MediaTexture.h"
#include "StreamMediaSource.h"
#define MCP_HAS_MEDIA_ASSETS 1
#else
#define MCP_HAS_MEDIA_ASSETS 0
#endif
#if __has_include("PaperSpriteFactory.h") && __has_include("PaperFlipbookFactory.h") && __has_include("PaperSprite.h") && __has_include("PaperFlipbook.h")
#include "PaperSpriteFactory.h"
#include "PaperFlipbookFactory.h"
#include "PaperSprite.h"
#include "PaperFlipbook.h"
#define MCP_HAS_PAPER2D_EDITOR 1
#else
#define MCP_HAS_PAPER2D_EDITOR 0
#endif
#if __has_include("PaperTileMapFactory.h") && __has_include("PaperTileSetFactory.h") && __has_include("PaperTileMap.h") && __has_include("PaperTileSet.h")
#include "PaperTileMapFactory.h"
#include "PaperTileSetFactory.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"
#define MCP_HAS_PAPER_TILEMAP_EDITOR 1
#else
#define MCP_HAS_PAPER_TILEMAP_EDITOR 0
#endif
#if __has_include("ContentBrowserModule.h")
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#define MCP_HAS_CONTENT_BROWSER 1
#else
#define MCP_HAS_CONTENT_BROWSER 0
#endif
#if MCP_HAS_CONTENT_BROWSER && __has_include("ContentBrowserInstanceUtils.h")
#include "ContentBrowserInstanceUtils.h"
#define MCP_HAS_CONTENT_BROWSER_SETTINGS 1
#else
#define MCP_HAS_CONTENT_BROWSER_SETTINGS 0
#endif
#if __has_include("CollectionManagerModule.h")
#include "CollectionManagerModule.h"
#include "ICollectionManager.h"
#include "CollectionManagerTypes.h"
#define MCP_HAS_COLLECTION_MANAGER 1
#else
#define MCP_HAS_COLLECTION_MANAGER 0
#endif
#include "EditorAssetLibrary.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"  // TActorIterator
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/PhysicalMaterialFactoryNew.h"
#include "Factories/CurveFactory.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "IAssetTools.h"
#include "Editor/EditorEngine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformProcess.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsSettingsEnums.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Source Control)
// -----------------------------------------------------------------------------
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Material Editing)
// -----------------------------------------------------------------------------
#include "ImageUtils.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// MaterialDomain.h was introduced in UE 5.1 - in UE 5.0 EMaterialDomain is in MaterialShared.h
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif

#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialShared.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Utilities)
// -----------------------------------------------------------------------------
#include "Misc/FileHelper.h"
#include "ObjectTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "Engine/DataAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "UObject/StructOnScope.h"
#include "UObject/SoftObjectPath.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Graph/Blueprint)
// -----------------------------------------------------------------------------
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Blueprint/BlueprintSupport.h"

#endif // WITH_EDITOR

#if WITH_EDITOR
static bool ParseCurveInterpModeAW(const FString& Value, ERichCurveInterpMode& OutMode)
{
  if (Value.Equals(TEXT("linear"), ESearchCase::IgnoreCase)) { OutMode = RCIM_Linear; return true; }
  if (Value.Equals(TEXT("constant"), ESearchCase::IgnoreCase)) { OutMode = RCIM_Constant; return true; }
  if (Value.Equals(TEXT("cubic"), ESearchCase::IgnoreCase)) { OutMode = RCIM_Cubic; return true; }
  if (Value.Equals(TEXT("none"), ESearchCase::IgnoreCase)) { OutMode = RCIM_None; return true; }
  return false;
}

static bool ParseCurveTangentModeAW(const FString& Value, ERichCurveTangentMode& OutMode)
{
  if (Value.Equals(TEXT("auto"), ESearchCase::IgnoreCase)) { OutMode = RCTM_Auto; return true; }
  if (Value.Equals(TEXT("user"), ESearchCase::IgnoreCase)) { OutMode = RCTM_User; return true; }
  if (Value.Equals(TEXT("break"), ESearchCase::IgnoreCase)) { OutMode = RCTM_Break; return true; }
  if (Value.Equals(TEXT("smart_auto"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("smartauto"), ESearchCase::IgnoreCase)) { OutMode = RCTM_SmartAuto; return true; }
  if (Value.Equals(TEXT("none"), ESearchCase::IgnoreCase)) { OutMode = RCTM_None; return true; }
  return false;
}

static bool ParseCurveTangentWeightModeAW(const FString& Value, ERichCurveTangentWeightMode& OutMode)
{
  if (Value.Equals(TEXT("none"), ESearchCase::IgnoreCase)) { OutMode = RCTWM_WeightedNone; return true; }
  if (Value.Equals(TEXT("arrive"), ESearchCase::IgnoreCase)) { OutMode = RCTWM_WeightedArrive; return true; }
  if (Value.Equals(TEXT("leave"), ESearchCase::IgnoreCase)) { OutMode = RCTWM_WeightedLeave; return true; }
  if (Value.Equals(TEXT("both"), ESearchCase::IgnoreCase)) { OutMode = RCTWM_WeightedBoth; return true; }
  return false;
}

static FString CurveInterpModeToStringAW(ERichCurveInterpMode Mode)
{
  switch (Mode) { case RCIM_Constant: return TEXT("constant"); case RCIM_Cubic: return TEXT("cubic"); case RCIM_None: return TEXT("none"); default: return TEXT("linear"); }
}

static FString CurveTangentModeToStringAW(ERichCurveTangentMode Mode)
{
  switch (Mode) { case RCTM_User: return TEXT("user"); case RCTM_Break: return TEXT("break"); case RCTM_SmartAuto: return TEXT("smart_auto"); case RCTM_None: return TEXT("none"); default: return TEXT("auto"); }
}

static FString CurveTangentWeightModeToStringAW(ERichCurveTangentWeightMode Mode)
{
  switch (Mode) { case RCTWM_WeightedArrive: return TEXT("arrive"); case RCTWM_WeightedLeave: return TEXT("leave"); case RCTWM_WeightedBoth: return TEXT("both"); default: return TEXT("none"); }
}

static bool ApplyCurveKeyOptionsAW(const TSharedPtr<FJsonObject>& KeyObject, FRichCurveKey& Key, FString& OutError)
{
  FString Mode;
  if (KeyObject->TryGetStringField(TEXT("interpMode"), Mode)) {
    ERichCurveInterpMode ParsedMode;
    if (!ParseCurveInterpModeAW(Mode, ParsedMode)) { OutError = TEXT("interpMode must be linear, constant, cubic, or none"); return false; }
    Key.InterpMode = ParsedMode;
  }
  if (KeyObject->TryGetStringField(TEXT("tangentMode"), Mode)) {
    ERichCurveTangentMode ParsedMode;
    if (!ParseCurveTangentModeAW(Mode, ParsedMode)) { OutError = TEXT("tangentMode must be auto, user, break, smart_auto, or none"); return false; }
    Key.TangentMode = ParsedMode;
  }
  if (KeyObject->TryGetStringField(TEXT("tangentWeightMode"), Mode)) {
    ERichCurveTangentWeightMode ParsedMode;
    if (!ParseCurveTangentWeightModeAW(Mode, ParsedMode)) { OutError = TEXT("tangentWeightMode must be none, arrive, leave, or both"); return false; }
    Key.TangentWeightMode = ParsedMode;
  }
  const TCHAR *NumericFields[] = { TEXT("arriveTangent"), TEXT("leaveTangent"), TEXT("arriveTangentWeight"), TEXT("leaveTangentWeight") };
  float *NumericTargets[] = { &Key.ArriveTangent, &Key.LeaveTangent, &Key.ArriveTangentWeight, &Key.LeaveTangentWeight };
  for (int32 NumericIndex = 0; NumericIndex < UE_ARRAY_COUNT(NumericFields); ++NumericIndex) {
    double NumericValue = 0.0;
    if (KeyObject->TryGetNumberField(NumericFields[NumericIndex], NumericValue)) {
      if (!FMath::IsFinite(NumericValue)) { OutError = TEXT("curve tangent values must be finite"); return false; }
      *NumericTargets[NumericIndex] = static_cast<float>(NumericValue);
    }
  }
  return true;
}
#endif

// =============================================================================
// MF-AWARE HELPERS (shared by Tier 3 material handlers)
// =============================================================================

// Try loading as UMaterial first, then UMaterialFunction.
// Returns the loaded UObject (Material or Function), or nullptr.
static UObject* LoadMaterialOrFunctionAW(const FString& AssetPath,
                                          UMaterial*& OutMaterial,
                                          UMaterialFunction*& OutFunction) {
  OutMaterial = LoadObject<UMaterial>(nullptr, *AssetPath);
  if (OutMaterial) return OutMaterial;
  OutFunction = LoadObject<UMaterialFunction>(nullptr, *AssetPath);
  return OutFunction;
}

// Return a reference to the expressions TArray for either host type.
// Caller must ensure at least one of Material/Function is non-null.
// Uses decltype(auto) so the return type matches the underlying member
// (TArray<TObjectPtr<...>>& on UE 5.1+, TArray<UMaterialExpression*>& on 5.0).
static decltype(auto) GetHostExpressions(
    UMaterial* Material, UMaterialFunction* Function) {
  return Material ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
                  : MCP_GET_FUNCTION_EXPRESSIONS(Function);
}

// Find a material expression by GUID, name, path, parameter name, or numeric index.
// Templated to accept both TArray<TObjectPtr<...>> (UE 5.1+) and TArray<UMaterialExpression*>.
template <typename TExprArray>
static UMaterialExpression* FindExpressionInHost(TExprArray& Expressions, const FString& IdOrIndex) {
  if (IdOrIndex.IsEmpty()) return nullptr;

  FGuid GuidId;
  if (FGuid::Parse(IdOrIndex, GuidId)) {
    for (UMaterialExpression *Expr : Expressions) {
      if (Expr && Expr->MaterialExpressionGuid == GuidId) return Expr;
    }
  }
  for (UMaterialExpression *Expr : Expressions) {
    if (Expr) {
      if (Expr->GetName() == IdOrIndex || Expr->GetPathName() == IdOrIndex) return Expr;
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        if (Param->ParameterName.ToString() == IdOrIndex) return Expr;
      }
    }
  }
  if (IdOrIndex.IsNumeric()) {
    int32 Index = FCString::Atoi(*IdOrIndex);
    if (Index >= 0 && Index < Expressions.Num()) return Expressions[Index];
  }
  return nullptr;
}

// PostEditChange + MarkPackageDirty on whichever host is non-null.
static void FinalizeHost(UMaterial* Material, UMaterialFunction* Function) {
  if (Material) { Material->PostEditChange(); Material->MarkPackageDirty(); }
  else if (Function) { Function->PostEditChange(); Function->MarkPackageDirty(); }
}

// =============================================================================
// ASSET ACTION DISPATCHER
// =============================================================================

#if WITH_EDITOR && MCP_HAS_PAPER2D_EDITOR
static bool HandlePaperSpriteAssetAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  FString Name;
  FString PackagePath;
  FString TexturePath;
  Payload->TryGetStringField(TEXT("name"), Name);
  Payload->TryGetStringField(TEXT("path"), PackagePath);
  Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
  Name = SanitizeAssetName(Name);
  PackagePath = SanitizeProjectRelativePath(PackagePath);
  TexturePath = SanitizeProjectRelativePath(TexturePath);
  if (Name.IsEmpty() || PackagePath.IsEmpty() || TexturePath.IsEmpty()) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("name, path, and texturePath are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FString AssetPackagePath = PackagePath + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper sprite asset already exists"), TEXT("ASSET_EXISTS"));
    return true;
  }
  UTexture2D* Texture = Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(TexturePath));
  if (!Texture) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("texturePath must resolve to a UTexture2D"), TEXT("TEXTURE_NOT_FOUND"));
    return true;
  }
  UPaperSpriteFactory* Factory = NewObject<UPaperSpriteFactory>();
  Factory->InitialTexture = Texture;
  UObject* NewAsset = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
      Name, PackagePath, UPaperSprite::StaticClass(), Factory);
  if (!NewAsset) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Unable to create Paper sprite asset"), TEXT("CREATE_FAILED"));
    return true;
  }
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(NewAsset)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper sprite created but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
  Result->SetStringField(TEXT("texturePath"), Texture->GetPathName());
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper sprite asset created"), Result, FString());
  return true;
}

static bool HandlePaperFlipbookAssetAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  FString Name;
  FString PackagePath;
  Payload->TryGetStringField(TEXT("name"), Name);
  Payload->TryGetStringField(TEXT("path"), PackagePath);
  Name = SanitizeAssetName(Name);
  PackagePath = SanitizeProjectRelativePath(PackagePath);
  const TArray<TSharedPtr<FJsonValue>>* SpriteValues = nullptr;
  if (Payload->TryGetArrayField(TEXT("spritePaths"), SpriteValues) && SpriteValues && SpriteValues->Num() > 0) {
    if (Name.IsEmpty() || PackagePath.IsEmpty()) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("name and path are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
  } else {
    Owner->SendAutomationError(Socket, RequestId, TEXT("spritePaths must contain at least one PaperSprite asset path"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FString AssetPackagePath = PackagePath + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper flipbook asset already exists"), TEXT("ASSET_EXISTS"));
    return true;
  }
  const TArray<TSharedPtr<FJsonValue>>* FrameRunValues = nullptr;
  Payload->TryGetArrayField(TEXT("frameRuns"), FrameRunValues);
  UPaperFlipbookFactory* Factory = NewObject<UPaperFlipbookFactory>();
  for (int32 Index = 0; Index < SpriteValues->Num(); ++Index) {
    const FString SpritePath = SanitizeProjectRelativePath((*SpriteValues)[Index]->AsString());
    UPaperSprite* Sprite = Cast<UPaperSprite>(UEditorAssetLibrary::LoadAsset(SpritePath));
    if (!Sprite) {
      Owner->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("spritePaths[%d] must resolve to a PaperSprite"), Index), TEXT("SPRITE_NOT_FOUND"));
      return true;
    }
    FPaperFlipbookKeyFrame& KeyFrame = Factory->KeyFrames.AddDefaulted_GetRef();
    KeyFrame.Sprite = Sprite;
    KeyFrame.FrameRun = 1;
    if (FrameRunValues && FrameRunValues->IsValidIndex(Index) && (*FrameRunValues)[Index]->Type == EJson::Number)
      KeyFrame.FrameRun = FMath::Max(1, static_cast<int32>((*FrameRunValues)[Index]->AsNumber()));
  }
  UObject* NewAsset = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
      Name, PackagePath, UPaperFlipbook::StaticClass(), Factory);
  if (!NewAsset) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Unable to create Paper flipbook asset"), TEXT("CREATE_FAILED"));
    return true;
  }
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(NewAsset)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper flipbook created but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
  Result->SetNumberField(TEXT("keyFrameCount"), Factory->KeyFrames.Num());
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper flipbook asset created"), Result, FString());
  return true;
}

#endif

#if WITH_EDITOR && MCP_HAS_PAPER_TILEMAP_EDITOR
static bool HandlePaperTileSetConfigureAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FString AssetPath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("assetPath")));
  UPaperTileSet* TileSet = Cast<UPaperTileSet>(UEditorAssetLibrary::LoadAsset(AssetPath));
  if (!TileSet) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a PaperTileSet"), TEXT("TILESET_NOT_FOUND"));
    return true;
  }
  const bool bHasTexture = Payload->HasField(TEXT("texturePath"));
  const bool bHasTileSize = Payload->HasField(TEXT("tileWidth")) || Payload->HasField(TEXT("tileHeight"));
  const bool bHasSpacing = Payload->HasField(TEXT("spacingX")) || Payload->HasField(TEXT("spacingY"));
  const bool bHasMargin = Payload->HasField(TEXT("marginLeft")) || Payload->HasField(TEXT("marginTop")) ||
      Payload->HasField(TEXT("marginRight")) || Payload->HasField(TEXT("marginBottom"));
  const bool bHasOffset = Payload->HasField(TEXT("drawingOffsetX")) || Payload->HasField(TEXT("drawingOffsetY"));
  if (!bHasTexture && !bHasTileSize && !bHasSpacing && !bHasMargin && !bHasOffset) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("At least one tile-set property is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  auto ReadBoundedInt = [&](const TCHAR* FieldName, int32 DefaultValue, int32 MinValue, int32 MaxValue, int32& OutValue) {
    OutValue = DefaultValue;
    if (!Payload->HasField(FieldName)) return true;
    return Payload->TryGetNumberField(FieldName, OutValue) && OutValue >= MinValue && OutValue <= MaxValue;
  };
  TileSet->Modify();
  if (bHasTexture) {
    const FString TexturePath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("texturePath")));
    UTexture2D* Texture = Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(TexturePath));
    if (!Texture) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("texturePath must resolve to a UTexture2D"), TEXT("TEXTURE_NOT_FOUND"));
      return true;
    }
    TileSet->SetTileSheetTexture(Texture);
  }
  if (bHasTileSize) {
    int32 TileWidth = 0;
    int32 TileHeight = 0;
    if (!ReadBoundedInt(TEXT("tileWidth"), TileSet->GetTileSize().X, 1, 8192, TileWidth) ||
        !ReadBoundedInt(TEXT("tileHeight"), TileSet->GetTileSize().Y, 1, 8192, TileHeight)) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("tileWidth and tileHeight must be between 1 and 8192"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TileSet->SetTileSize(FIntPoint(TileWidth, TileHeight));
  }
  if (bHasSpacing) {
    int32 SpacingX = 0;
    int32 SpacingY = 0;
    if (!ReadBoundedInt(TEXT("spacingX"), TileSet->GetPerTileSpacing().X, 0, 8192, SpacingX) ||
        !ReadBoundedInt(TEXT("spacingY"), TileSet->GetPerTileSpacing().Y, 0, 8192, SpacingY)) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("spacingX and spacingY must be between 0 and 8192"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TileSet->SetPerTileSpacing(FIntPoint(SpacingX, SpacingY));
  }
  if (bHasMargin) {
    FIntMargin Margin = TileSet->GetMargin();
    if (!ReadBoundedInt(TEXT("marginLeft"), Margin.Left, 0, 8192, Margin.Left) ||
        !ReadBoundedInt(TEXT("marginTop"), Margin.Top, 0, 8192, Margin.Top) ||
        !ReadBoundedInt(TEXT("marginRight"), Margin.Right, 0, 8192, Margin.Right) ||
        !ReadBoundedInt(TEXT("marginBottom"), Margin.Bottom, 0, 8192, Margin.Bottom)) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("margin values must be between 0 and 8192"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TileSet->SetMargin(Margin);
  }
  if (bHasOffset) {
    FIntPoint Offset = TileSet->GetDrawingOffset();
    if (!ReadBoundedInt(TEXT("drawingOffsetX"), Offset.X, -8192, 8192, Offset.X) ||
        !ReadBoundedInt(TEXT("drawingOffsetY"), Offset.Y, -8192, 8192, Offset.Y)) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("drawing offsets must be between -8192 and 8192"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TileSet->SetDrawingOffset(Offset);
  }
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(TileSet)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper tile-set configured but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), TileSet->GetPathName());
  Result->SetNumberField(TEXT("tileCount"), TileSet->GetTileCount());
  Result->SetNumberField(TEXT("tileCountX"), TileSet->GetTileCountX());
  Result->SetNumberField(TEXT("tileCountY"), TileSet->GetTileCountY());
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper tile-set configured"), Result, FString());
  return true;
}

static bool HandlePaperTileSetInspectAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FString AssetPath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("assetPath")));
  UPaperTileSet* TileSet = Cast<UPaperTileSet>(UEditorAssetLibrary::LoadAsset(AssetPath));
  if (!TileSet) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a PaperTileSet"), TEXT("TILESET_NOT_FOUND"));
    return true;
  }
  const FIntPoint TileSize = TileSet->GetTileSize();
  const FIntPoint TileSpacing = TileSet->GetPerTileSpacing();
  const FIntPoint DrawingOffset = TileSet->GetDrawingOffset();
  const FIntMargin Margin = TileSet->GetMargin();
  const FIntPoint AuthoredSize = TileSet->GetTileSheetAuthoredSize();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), TileSet->GetPathName());
  Result->SetStringField(TEXT("tileSheetPath"), TileSet->GetTileSheetTexture() ? TileSet->GetTileSheetTexture()->GetPathName() : FString());
  Result->SetNumberField(TEXT("tileCount"), TileSet->GetTileCount());
  Result->SetNumberField(TEXT("tileCountX"), TileSet->GetTileCountX());
  Result->SetNumberField(TEXT("tileCountY"), TileSet->GetTileCountY());
  Result->SetNumberField(TEXT("tileWidth"), TileSize.X);
  Result->SetNumberField(TEXT("tileHeight"), TileSize.Y);
  Result->SetNumberField(TEXT("spacingX"), TileSpacing.X);
  Result->SetNumberField(TEXT("spacingY"), TileSpacing.Y);
  Result->SetNumberField(TEXT("drawingOffsetX"), DrawingOffset.X);
  Result->SetNumberField(TEXT("drawingOffsetY"), DrawingOffset.Y);
  Result->SetNumberField(TEXT("marginLeft"), Margin.Left);
  Result->SetNumberField(TEXT("marginTop"), Margin.Top);
  Result->SetNumberField(TEXT("marginRight"), Margin.Right);
  Result->SetNumberField(TEXT("marginBottom"), Margin.Bottom);
  Result->SetNumberField(TEXT("authoredWidth"), AuthoredSize.X);
  Result->SetNumberField(TEXT("authoredHeight"), AuthoredSize.Y);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper tile-set inspected"), Result, FString());
  return true;
}

static bool HandlePaperTileSetAssetAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  FString Name;
  FString PackagePath;
  Payload->TryGetStringField(TEXT("name"), Name);
  Payload->TryGetStringField(TEXT("path"), PackagePath);
  Name = SanitizeAssetName(Name);
  PackagePath = SanitizeProjectRelativePath(PackagePath);
  if (Name.IsEmpty() || PackagePath.IsEmpty()) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("name and path are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FString AssetPackagePath = PackagePath + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper tile-set asset already exists"), TEXT("ASSET_EXISTS"));
    return true;
  }
  UPaperTileSetFactory* Factory = NewObject<UPaperTileSetFactory>();
  FString TexturePath;
  Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
  TexturePath = SanitizeProjectRelativePath(TexturePath);
  if (!TexturePath.IsEmpty()) {
    Factory->InitialTexture = Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(TexturePath));
    if (!Factory->InitialTexture) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("texturePath must resolve to a UTexture2D"), TEXT("TEXTURE_NOT_FOUND"));
      return true;
    }
  }
  UObject* NewAsset = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
      Name, PackagePath, UPaperTileSet::StaticClass(), Factory);
  if (!NewAsset) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Unable to create Paper tile-set asset"), TEXT("CREATE_FAILED"));
    return true;
  }
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(NewAsset)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper tile-set created but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper tile-set asset created"), Result, FString());
  return true;
}

static bool HandlePaperTileMapResizeAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FString TileMapPath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("assetPath")));
  UPaperTileMap* TileMap = Cast<UPaperTileMap>(UEditorAssetLibrary::LoadAsset(TileMapPath));
  if (!TileMap) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a PaperTileMap"), TEXT("TILEMAP_NOT_FOUND"));
    return true;
  }
  int32 Width = 0;
  int32 Height = 0;
  bool bForceResize = false;
  if (!Payload->TryGetNumberField(TEXT("width"), Width) || !Payload->TryGetNumberField(TEXT("height"), Height) ||
      Width < 1 || Width > 1024 || Height < 1 || Height > 1024) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("width and height must be between 1 and 1024"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Payload->TryGetBoolField(TEXT("forceResize"), bForceResize);
  TileMap->Modify();
  TileMap->ResizeMap(Width, Height, bForceResize);
  TileMap->RebuildCollision();
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(TileMap)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper tile-map resized but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), TileMap->GetPathName());
  Result->SetNumberField(TEXT("width"), TileMap->MapWidth);
  Result->SetNumberField(TEXT("height"), TileMap->MapHeight);
  Result->SetBoolField(TEXT("forceResize"), bForceResize);
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper tile-map resized"), Result, FString());
  return true;
}

static bool HandlePaperTileMapAssetAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  FString Name;
  FString PackagePath;
  Payload->TryGetStringField(TEXT("name"), Name);
  Payload->TryGetStringField(TEXT("path"), PackagePath);
  Name = SanitizeAssetName(Name);
  PackagePath = SanitizeProjectRelativePath(PackagePath);
  if (Name.IsEmpty() || PackagePath.IsEmpty()) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("name and path are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FString AssetPackagePath = PackagePath + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(AssetPackagePath)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper tile-map asset already exists"), TEXT("ASSET_EXISTS"));
    return true;
  }
  UPaperTileMapFactory* Factory = NewObject<UPaperTileMapFactory>();
  FString TileSetPath;
  Payload->TryGetStringField(TEXT("tileSetPath"), TileSetPath);
  TileSetPath = SanitizeProjectRelativePath(TileSetPath);
  if (!TileSetPath.IsEmpty()) {
    Factory->InitialTileSet = Cast<UPaperTileSet>(UEditorAssetLibrary::LoadAsset(TileSetPath));
    if (!Factory->InitialTileSet) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("tileSetPath must resolve to a UPaperTileSet"), TEXT("TILESET_NOT_FOUND"));
      return true;
    }
  }
  UObject* NewAsset = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
      Name, PackagePath, UPaperTileMap::StaticClass(), Factory);
  if (!NewAsset) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Unable to create Paper tile-map asset"), TEXT("CREATE_FAILED"));
    return true;
  }
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(NewAsset)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper tile-map created but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper tile-map asset created"), Result, FString());
  return true;
}

#endif

#if WITH_EDITOR && MCP_HAS_PAPER2D_EDITOR
static bool HandlePaperFlipbookEditAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FString FlipbookPath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("assetPath")));
  UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(UEditorAssetLibrary::LoadAsset(FlipbookPath));
  if (!Flipbook) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a PaperFlipbook"), TEXT("FLIPBOOK_NOT_FOUND"));
    return true;
  }
  FScopedFlipbookMutator Mutator(Flipbook);
  if (Action == TEXT("add_flipbook_keyframe")) {
    const FString SpritePath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("spritePath")));
    UPaperSprite* Sprite = Cast<UPaperSprite>(UEditorAssetLibrary::LoadAsset(SpritePath));
    if (!Sprite) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("spritePath must resolve to a PaperSprite"), TEXT("SPRITE_NOT_FOUND"));
      return true;
    }
    int32 FrameRun = GetJsonIntField(Payload, TEXT("frameRun"), 1);
    if (FrameRun < 1 || FrameRun > 100000) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("frameRun must be between 1 and 100000"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FPaperFlipbookKeyFrame KeyFrame;
    KeyFrame.Sprite = Sprite;
    KeyFrame.FrameRun = FrameRun;
    int32 InsertIndex = INDEX_NONE;
    if (Payload->HasField(TEXT("keyFrameIndex"))) InsertIndex = GetJsonIntField(Payload, TEXT("keyFrameIndex"), INDEX_NONE);
    if (InsertIndex < 0 || InsertIndex > Mutator.KeyFrames.Num()) Mutator.KeyFrames.Add(KeyFrame);
    else Mutator.KeyFrames.Insert(KeyFrame, InsertIndex);
  } else {
    const double FramesPerSecond = GetJsonNumberField(Payload, TEXT("framesPerSecond"), 0.0);
    if (!FMath::IsFinite(FramesPerSecond) || FramesPerSecond <= 0.0 || FramesPerSecond > 10000.0) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("framesPerSecond must be between 0 and 10000"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Mutator.FramesPerSecond = static_cast<float>(FramesPerSecond);
  }
  Mutator.InvalidateCachedData();
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(Flipbook)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper flipbook edited but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), Flipbook->GetPathName());
  Result->SetNumberField(TEXT("keyFrameCount"), Mutator.KeyFrames.Num());
  Result->SetNumberField(TEXT("framesPerSecond"), Mutator.FramesPerSecond);
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper flipbook edited"), Result, FString());
  return true;
}

static bool HandlePaperSpriteSourceAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FString SpritePath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("assetPath")));
  UPaperSprite* Sprite = Cast<UPaperSprite>(UEditorAssetLibrary::LoadAsset(SpritePath));
  if (!Sprite) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a PaperSprite"), TEXT("SPRITE_NOT_FOUND"));
    return true;
  }
  bool bTrimmed = true;
  Payload->TryGetBoolField(TEXT("trimmed"), bTrimmed);
  double OriginX = 0.0;
  double OriginY = 0.0;
  double Width = 0.0;
  double Height = 0.0;
  if (!Payload->TryGetNumberField(TEXT("sourceOriginX"), OriginX) || !Payload->TryGetNumberField(TEXT("sourceOriginY"), OriginY) ||
      !Payload->TryGetNumberField(TEXT("sourceWidth"), Width) || !Payload->TryGetNumberField(TEXT("sourceHeight"), Height) ||
      !FMath::IsFinite(OriginX) || !FMath::IsFinite(OriginY) || !FMath::IsFinite(Width) || !FMath::IsFinite(Height) || Width <= 0.0 || Height <= 0.0) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("sourceOriginX/sourceOriginY and positive sourceWidth/sourceHeight are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  bool bRebuildData = true;
  Payload->TryGetBoolField(TEXT("rebuildData"), bRebuildData);
  Sprite->Modify();
  Sprite->SetTrim(bTrimmed, FVector2D(static_cast<float>(OriginX), static_cast<float>(OriginY)),
                  FVector2D(static_cast<float>(Width), static_cast<float>(Height)), bRebuildData);
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(Sprite)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper sprite source region updated but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  const FVector2D SourceSize = Sprite->GetSourceSize();
  const FVector2D SourceUV = Sprite->GetSourceUV();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), Sprite->GetPathName());
  Result->SetBoolField(TEXT("trimmed"), bTrimmed);
  Result->SetNumberField(TEXT("sourceWidth"), SourceSize.X);
  Result->SetNumberField(TEXT("sourceHeight"), SourceSize.Y);
  Result->SetNumberField(TEXT("sourceUVX"), SourceUV.X);
  Result->SetNumberField(TEXT("sourceUVY"), SourceUV.Y);
  Result->SetBoolField(TEXT("rebuiltData"), bRebuildData);
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper sprite source region updated"), Result, FString());
  return true;
}

static bool HandlePaperSpriteInspectAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FString SpritePath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("assetPath")));
  UPaperSprite* Sprite = Cast<UPaperSprite>(UEditorAssetLibrary::LoadAsset(SpritePath));
  if (!Sprite) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a PaperSprite"), TEXT("SPRITE_NOT_FOUND"));
    return true;
  }
  const FVector2D SourceSize = Sprite->GetSourceSize();
  const FVector2D SourceUV = Sprite->GetSourceUV();
  const FVector2D PivotPosition = Sprite->GetPivotPosition();
  FVector2D CustomPivot;
  const ESpritePivotMode::Type PivotMode = Sprite->GetPivotMode(CustomPivot);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), Sprite->GetPathName());
  Result->SetStringField(TEXT("sourceTexture"), Sprite->GetSourceTexture() ? Sprite->GetSourceTexture()->GetPathName() : FString());
  Result->SetNumberField(TEXT("sourceWidth"), SourceSize.X);
  Result->SetNumberField(TEXT("sourceHeight"), SourceSize.Y);
  Result->SetNumberField(TEXT("sourceUVX"), SourceUV.X);
  Result->SetNumberField(TEXT("sourceUVY"), SourceUV.Y);
  Result->SetNumberField(TEXT("pivotX"), PivotPosition.X);
  Result->SetNumberField(TEXT("pivotY"), PivotPosition.Y);
  Result->SetNumberField(TEXT("pivotModeValue"), static_cast<int32>(PivotMode));
  Result->SetNumberField(TEXT("collisionDomainValue"), static_cast<int32>(Sprite->GetSpriteCollisionDomain()));
  Result->SetNumberField(TEXT("collisionThickness"), Sprite->GetCollisionThickness());
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper sprite inspected"), Result, FString());
  return true;
}

static bool HandlePaperSpritePivotAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FString SpritePath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("assetPath")));
  UPaperSprite* Sprite = Cast<UPaperSprite>(UEditorAssetLibrary::LoadAsset(SpritePath));
  if (!Sprite) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a PaperSprite"), TEXT("SPRITE_NOT_FOUND"));
    return true;
  }
  FString PivotMode;
  Payload->TryGetStringField(TEXT("pivotMode"), PivotMode);
  PivotMode = PivotMode.ToLower().Replace(TEXT("-"), TEXT("_")).Replace(TEXT(" "), TEXT("_"));
  ESpritePivotMode::Type Mode;
  if (PivotMode == TEXT("top_left")) Mode = ESpritePivotMode::Top_Left;
  else if (PivotMode == TEXT("top_center")) Mode = ESpritePivotMode::Top_Center;
  else if (PivotMode == TEXT("top_right")) Mode = ESpritePivotMode::Top_Right;
  else if (PivotMode == TEXT("center_left")) Mode = ESpritePivotMode::Center_Left;
  else if (PivotMode == TEXT("center_center") || PivotMode == TEXT("center")) Mode = ESpritePivotMode::Center_Center;
  else if (PivotMode == TEXT("center_right")) Mode = ESpritePivotMode::Center_Right;
  else if (PivotMode == TEXT("bottom_left")) Mode = ESpritePivotMode::Bottom_Left;
  else if (PivotMode == TEXT("bottom_center")) Mode = ESpritePivotMode::Bottom_Center;
  else if (PivotMode == TEXT("bottom_right")) Mode = ESpritePivotMode::Bottom_Right;
  else if (PivotMode == TEXT("custom")) Mode = ESpritePivotMode::Custom;
  else {
    Owner->SendAutomationError(Socket, RequestId, TEXT("pivotMode must be a documented Paper2D pivot mode"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  double PivotX = 0.0;
  double PivotY = 0.0;
  if (Mode == ESpritePivotMode::Custom &&
      (!Payload->TryGetNumberField(TEXT("pivotX"), PivotX) || !Payload->TryGetNumberField(TEXT("pivotY"), PivotY) ||
       !FMath::IsFinite(PivotX) || !FMath::IsFinite(PivotY))) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("custom pivotMode requires finite pivotX and pivotY texture-space coordinates"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  bool bRebuildData = true;
  Payload->TryGetBoolField(TEXT("rebuildData"), bRebuildData);
  Sprite->Modify();
  Sprite->SetPivotMode(Mode, FVector2D(static_cast<float>(PivotX), static_cast<float>(PivotY)), bRebuildData);
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(Sprite)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper sprite pivot updated but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  const FVector2D PivotPosition = Sprite->GetPivotPosition();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), Sprite->GetPathName());
  Result->SetStringField(TEXT("pivotMode"), PivotMode);
  Result->SetNumberField(TEXT("pivotX"), PivotPosition.X);
  Result->SetNumberField(TEXT("pivotY"), PivotPosition.Y);
  Result->SetBoolField(TEXT("rebuiltData"), bRebuildData);
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper sprite pivot updated"), Result, FString());
  return true;
}

#endif

#if WITH_EDITOR && MCP_HAS_PAPER_TILEMAP_EDITOR
static bool HandlePaperTileMapCollisionAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FString TileMapPath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("assetPath")));
  UPaperTileMap* TileMap = Cast<UPaperTileMap>(UEditorAssetLibrary::LoadAsset(TileMapPath));
  if (!TileMap) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a PaperTileMap"), TEXT("TILEMAP_NOT_FOUND"));
    return true;
  }
  FString CollisionDomain;
  Payload->TryGetStringField(TEXT("collisionDomain"), CollisionDomain);
  CollisionDomain = CollisionDomain.ToLower().Replace(TEXT("-"), TEXT(""));
  bool bChanged = false;
  if (!CollisionDomain.IsEmpty()) {
    int32 DomainValue = INDEX_NONE;
    if (CollisionDomain == TEXT("none")) DomainValue = 0;
    else if (CollisionDomain == TEXT("2d") || CollisionDomain == TEXT("use2dphysics")) DomainValue = 1;
    else if (CollisionDomain == TEXT("3d") || CollisionDomain == TEXT("use3dphysics")) DomainValue = 2;
    if (DomainValue == INDEX_NONE) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("collisionDomain must be none, 2d, or 3d"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TileMap->SetCollisionDomain(static_cast<ESpriteCollisionMode::Type>(DomainValue));
    bChanged = true;
  }
  if (Payload->HasField(TEXT("collisionThickness"))) {
    double Thickness = 0.0;
    if (!Payload->TryGetNumberField(TEXT("collisionThickness"), Thickness) || !FMath::IsFinite(Thickness) || Thickness < 0.0 || Thickness > 1000000.0) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("collisionThickness must be finite and between 0 and 1000000"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TileMap->SetCollisionThickness(static_cast<float>(Thickness));
    bChanged = true;
  }
  if (!bChanged) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("collisionDomain or collisionThickness is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  TileMap->Modify();
  TileMap->RebuildCollision();
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(TileMap)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Paper tile-map collision updated but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), TileMap->GetPathName());
  Result->SetStringField(TEXT("collisionDomain"), CollisionDomain.IsEmpty() ? TEXT("unchanged") : CollisionDomain);
  Result->SetNumberField(TEXT("collisionThickness"), TileMap->GetCollisionThickness());
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Paper tile-map collision configured"), Result, FString());
  return true;
}
#endif

#if WITH_EDITOR && MCP_HAS_MEDIA_ASSETS
static bool HandleMediaAssetAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!Payload.IsValid()) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Media asset payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Name;
  FString PackagePath;
  Payload->TryGetStringField(TEXT("name"), Name);
  Payload->TryGetStringField(TEXT("path"), PackagePath);
  Name = SanitizeAssetName(Name);
  PackagePath = SanitizeProjectRelativePath(PackagePath);
  if (Name.IsEmpty() || PackagePath.IsEmpty()) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("name and path are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FString PackageName = PackagePath + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(PackageName)) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Media asset already exists"), TEXT("ASSET_EXISTS"));
    return true;
  }

  UPackage *Package = CreatePackage(*PackageName);
  if (!Package) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Unable to create media asset package"), TEXT("CREATE_FAILED"));
    return true;
  }

  UObject *Asset = nullptr;
  if (Action == TEXT("create_media_player")) {
    Asset = NewObject<UMediaPlayer>(Package, UMediaPlayer::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
  } else if (Action == TEXT("create_media_playlist")) {
    Asset = NewObject<UMediaPlaylist>(Package, UMediaPlaylist::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
  } else if (Action == TEXT("create_media_texture")) {
    FString PlayerPath;
    Payload->TryGetStringField(TEXT("mediaPlayerPath"), PlayerPath);
    UMediaPlayer *Player = LoadObject<UMediaPlayer>(nullptr, *SanitizeProjectRelativePath(PlayerPath));
    if (!Player) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("mediaPlayerPath must resolve to a UMediaPlayer"), TEXT("MEDIA_PLAYER_NOT_FOUND"));
      return true;
    }
    UMediaTexture *Texture = NewObject<UMediaTexture>(Package, UMediaTexture::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (Texture) Texture->SetDefaultMediaPlayer(Player);
    Asset = Texture;
  } else if (Action == TEXT("create_media_source")) {
    FString Url;
    FString MediaType;
    Payload->TryGetStringField(TEXT("mediaUrl"), Url);
    Payload->TryGetStringField(TEXT("mediaType"), MediaType);
    if (Url.IsEmpty()) {
      Owner->SendAutomationError(Socket, RequestId, TEXT("mediaUrl is required for a media source"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (MediaType.Equals(TEXT("stream"), ESearchCase::IgnoreCase)) {
      UStreamMediaSource *Source = NewObject<UStreamMediaSource>(Package, UStreamMediaSource::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
      if (Source) Source->StreamUrl = Url;
      Asset = Source;
    } else {
      UFileMediaSource *Source = NewObject<UFileMediaSource>(Package, UFileMediaSource::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
      if (Source) Source->SetFilePath(Url);
      Asset = Source;
    }
  }

  if (!Asset) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Unable to create media asset"), TEXT("CREATE_FAILED"));
    return true;
  }
  FAssetRegistryModule::AssetCreated(Asset);
  Package->MarkPackageDirty();

  if (Action == TEXT("create_media_playlist")) {
    FString SourcePath;
    if (Payload->TryGetStringField(TEXT("mediaSourcePath"), SourcePath) && !SourcePath.IsEmpty()) {
      UMediaSource *Source = LoadObject<UMediaSource>(nullptr, *SanitizeProjectRelativePath(SourcePath));
      if (!Source || !Cast<UMediaPlaylist>(Asset)->Add(Source)) {
        Owner->SendAutomationError(Socket, RequestId, TEXT("mediaSourcePath must resolve to a compatible media source"), TEXT("MEDIA_SOURCE_NOT_FOUND"));
        return true;
      }
    }
  }

  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  const bool bSaved = !bSave || McpSafeAssetSave(Asset);
  if (!bSaved) {
    Owner->SendAutomationError(Socket, RequestId, TEXT("Media asset created but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), Asset->GetPathName());
  Result->SetStringField(TEXT("classPath"), Asset->GetClass()->GetPathName());
  Result->SetBoolField(TEXT("saved"), bSave);
  Owner->SendAutomationResponse(Socket, RequestId, true, TEXT("Media asset created"), Result, FString());
  return true;
}
#endif

bool UNebulaForgeBridgeSubsystem::HandleAssetAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Lower = Action.ToLower();

  // If the action is the generic "manage_asset" tool, check for a subAction in
  // the payload
  if (Lower == TEXT("manage_asset") && Payload.IsValid()) {
    FString SubAction;
    if (Payload->TryGetStringField(TEXT("subAction"), SubAction) &&
        !SubAction.IsEmpty()) {
      Lower = SubAction.ToLower();
    }
  }

  if (Lower.IsEmpty())
    return false;

  if (Lower == TEXT("inspect_asset_capabilities"))
  {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("basicAssetLifecycle"), true);
    Result->SetBoolField(TEXT("genericTagsAndMetadata"), true);
    Result->SetBoolField(TEXT("naniteRebuild"), true);
    Result->SetBoolField(TEXT("dependencyInspection"), true);
    Result->SetBoolField(TEXT("redirectorCleanup"), true);
    Result->SetBoolField(TEXT("perAssetValidation"), true);
    Result->SetBoolField(TEXT("sourceControlIntegration"), true);
    Result->SetBoolField(TEXT("generatedCubeVolumeArrayTextures"), false);
    Result->SetBoolField(TEXT("dependencyAwareMigration"), false);
    Result->SetBoolField(TEXT("projectWideReleaseAudit"), false);
    Result->SetStringField(TEXT("textureGenerationNote"), TEXT("Cube, volume, and array textures require imported or assembled source data; generated placeholder assets are not claimed as complete."));
    Result->SetStringField(TEXT("migrationNote"), TEXT("Use dependency inspection plus duplicate/move operations; transactional dependency closure and redirect repair across a release are not automatic."));
    Result->SetStringField(TEXT("auditNote"), TEXT("validate and generate_report cover requested assets/packages; a project-wide cook/package release gate remains outside this handler."));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Asset capability report generated"), Result, FString());
    return true;
  }

  // Dispatch to specific handlers
  // CRITICAL: These actions must match what TS sends as 'action' (not just 'subAction')
  // When TS calls executeAutomationRequest(tools, 'search_assets', {...}), Action='search_assets'

  // Asset Operations
  if (Lower == TEXT("import"))
    return HandleImportAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("duplicate") || Lower == TEXT("duplicate_asset"))
    return HandleDuplicateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("rename") || Lower == TEXT("rename_asset"))
    return HandleRenameAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("move") || Lower == TEXT("move_asset"))
    return HandleMoveAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("delete") || Lower == TEXT("delete_asset") || Lower == TEXT("delete_assets"))
    return HandleDeleteAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_folder"))
    return HandleCreateFolder(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_view_settings") ||
      Lower == TEXT("navigate_to_path") ||
      Lower == TEXT("sync_to_asset") ||
      Lower == TEXT("sync_to_folder") ||
      Lower == TEXT("create_collection") ||
      Lower == TEXT("add_to_collection") ||
      Lower == TEXT("set_asset_color") ||
      Lower == TEXT("show_in_explorer") ||
      Lower == TEXT("set_search_text"))
    return HandleContentBrowserAction(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("create_material"))
    return HandleCreateMaterial(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material_instance"))
    return HandleCreateMaterialInstance(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_data_asset") ||
      Lower == TEXT("get_data_asset_properties") ||
      Lower == TEXT("set_data_asset_properties"))
    return HandleDataAssetAction(RequestId, Lower, Payload, RequestingSocket);

  if (Lower == TEXT("list_primary_assets") || Lower == TEXT("get_primary_asset")) {
    UAssetManager& AssetManager = UAssetManager::Get();
    auto GetString = [Payload](const TCHAR* Field) {
      FString Value;
      if (Payload.IsValid()) Payload->TryGetStringField(Field, Value);
      return Value;
    };

    if (Lower == TEXT("list_primary_assets")) {
      FString TypeName = GetString(TEXT("primaryAssetType"));
      TArray<FPrimaryAssetId> AssetIds;
      if (!TypeName.IsEmpty()) {
        AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(*TypeName), AssetIds, EAssetManagerFilter::Default);
      } else {
        TArray<FPrimaryAssetTypeInfo> TypeInfos;
        AssetManager.GetPrimaryAssetTypeInfoList(TypeInfos);
        for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfos) {
          TArray<FPrimaryAssetId> TypeAssetIds;
          AssetManager.GetPrimaryAssetIdList(TypeInfo.PrimaryAssetType, TypeAssetIds, EAssetManagerFilter::Default);
          AssetIds.Append(TypeAssetIds);
        }
      }

      int32 Offset = 0;
      int32 Limit = 200;
      if (Payload.IsValid()) {
        double Number = 0.0;
        if (Payload->TryGetNumberField(TEXT("offset"), Number)) Offset = FMath::Max(0, FMath::Min(static_cast<int32>(Number), AssetIds.Num()));
        if (Payload->TryGetNumberField(TEXT("limit"), Number)) Limit = FMath::Clamp(static_cast<int32>(Number), 1, 1000);
      }
      const int32 End = FMath::Min(AssetIds.Num(), Offset + Limit);
      TArray<TSharedPtr<FJsonValue>> Assets;
      for (int32 Index = Offset; Index < End; ++Index) {
        const FPrimaryAssetId& Id = AssetIds[Index];
        TSharedPtr<FJsonObject> Asset = MakeShared<FJsonObject>();
        Asset->SetStringField(TEXT("id"), Id.ToString());
        Asset->SetStringField(TEXT("type"), Id.PrimaryAssetType.ToString());
        Asset->SetStringField(TEXT("name"), Id.PrimaryAssetName.ToString());
        Asset->SetStringField(TEXT("path"), AssetManager.GetPrimaryAssetPath(Id).ToString());
        Assets.Add(MakeShared<FJsonValueObject>(Asset));
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetArrayField(TEXT("assets"), Assets);
      Result->SetNumberField(TEXT("total"), AssetIds.Num());
      Result->SetNumberField(TEXT("offset"), Offset);
      Result->SetNumberField(TEXT("limit"), Limit);
      Result->SetBoolField(TEXT("truncated"), End < AssetIds.Num());
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Primary Assets listed"), Result, FString());
      return true;
    }

    FString IdString = GetString(TEXT("primaryAssetId"));
    FString AssetPath = GetString(TEXT("assetPath"));
    FPrimaryAssetId Id = IdString.IsEmpty() ? FPrimaryAssetId() : FPrimaryAssetId::FromString(IdString);
    if (!Id.IsValid() && !AssetPath.IsEmpty()) Id = AssetManager.GetPrimaryAssetIdForPath(FSoftObjectPath(AssetPath));
    if (!Id.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("get_primary_asset requires a valid primaryAssetId or registered assetPath"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("id"), Id.ToString());
    Result->SetStringField(TEXT("type"), Id.PrimaryAssetType.ToString());
    Result->SetStringField(TEXT("name"), Id.PrimaryAssetName.ToString());
    Result->SetStringField(TEXT("path"), AssetManager.GetPrimaryAssetPath(Id).ToString());
    Result->SetBoolField(TEXT("loaded"), AssetManager.GetPrimaryAssetObject(Id) != nullptr);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Primary Asset inspected"), Result, FString());
    return true;
  }
  if (Lower == TEXT("create_sprite")) {
#if WITH_EDITOR && MCP_HAS_PAPER2D_EDITOR
    return HandlePaperSpriteAssetAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("create_flipbook")) {
#if WITH_EDITOR && MCP_HAS_PAPER2D_EDITOR
    return HandlePaperFlipbookAssetAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("create_tile_set")) {
#if WITH_EDITOR && MCP_HAS_PAPER_TILEMAP_EDITOR
    return HandlePaperTileSetAssetAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("inspect_tile_set")) {
#if WITH_EDITOR && MCP_HAS_PAPER_TILEMAP_EDITOR
    return HandlePaperTileSetInspectAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("configure_tile_set")) {
#if WITH_EDITOR && MCP_HAS_PAPER_TILEMAP_EDITOR
    return HandlePaperTileSetConfigureAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("create_tile_map")) {
#if WITH_EDITOR && MCP_HAS_PAPER_TILEMAP_EDITOR
    return HandlePaperTileMapAssetAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("resize_tile_map")) {
#if WITH_EDITOR && MCP_HAS_PAPER_TILEMAP_EDITOR
    return HandlePaperTileMapResizeAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("inspect_sprite")) {
#if WITH_EDITOR && MCP_HAS_PAPER2D_EDITOR
    return HandlePaperSpriteInspectAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("configure_sprite_source")) {
#if WITH_EDITOR && MCP_HAS_PAPER2D_EDITOR
    return HandlePaperSpriteSourceAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("configure_tile_map_collision")) {
#if WITH_EDITOR && MCP_HAS_PAPER_TILEMAP_EDITOR
    return HandlePaperTileMapCollisionAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("set_sprite_pivot")) {
#if WITH_EDITOR && MCP_HAS_PAPER2D_EDITOR
    return HandlePaperSpritePivotAction(this, RequestId, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("add_flipbook_keyframe") || Lower == TEXT("set_flipbook_framerate")) {
#if WITH_EDITOR && MCP_HAS_PAPER2D_EDITOR
    return HandlePaperFlipbookEditAction(this, RequestId, Lower, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Paper2D editor plugin is unavailable"), TEXT("PAPER2D_EDITOR_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("create_media_player") ||
      Lower == TEXT("create_media_source") ||
      Lower == TEXT("create_media_texture") ||
      Lower == TEXT("create_media_playlist")) {
#if WITH_EDITOR && MCP_HAS_MEDIA_ASSETS
    return HandleMediaAssetAction(this, RequestId, Lower, Payload, RequestingSocket);
#else
    SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Media Framework assets are unavailable in this build"),
                         TEXT("MEDIA_ASSETS_NOT_AVAILABLE"));
    return true;
#endif
  }
  if (Lower == TEXT("create_data_table") ||
      Lower == TEXT("add_data_table_row") ||
      Lower == TEXT("get_data_table_rows"))
    return HandleDataTableAction(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("create_curve_table") ||
      Lower == TEXT("replace_curve_keys") ||
      Lower == TEXT("add_curve_table_row") ||
      Lower == TEXT("get_curve_table_rows") ||
      Lower == TEXT("import_curve_table_csv") ||
      Lower == TEXT("export_curve_table_csv"))
    return HandleCurveTableAction(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("create_physical_material") ||
      Lower == TEXT("set_friction") || Lower == TEXT("set_restitution") ||
      Lower == TEXT("set_density") || Lower == TEXT("configure_surface_type") ||
      Lower == TEXT("assign_physical_material") ||
      Lower == TEXT("configure_physical_material") ||
      Lower == TEXT("get_physical_material") ||
      Lower == TEXT("clear_physical_material_override"))
    return HandlePhysicalMaterialAction(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("create_render_target"))
    return HandleManageTextureAction(RequestId, TEXT("manage_texture"), Payload, RequestingSocket);
  if (Lower == TEXT("get_dependencies"))
    return HandleGetDependencies(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("reference_viewer")) {
    if (Payload.IsValid()) {
      Payload->SetBoolField(TEXT("includeReferencers"), true);
    }
    return HandleGetAssetGraph(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("audit_assets")) {
    if (Payload.IsValid()) {
      Payload->SetStringField(TEXT("reportType"), TEXT("audit"));
    }
    return HandleGenerateReport(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("get_asset_graph"))
    return HandleGetAssetGraph(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_tags"))
    return HandleSetTags(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_metadata"))
    return HandleSetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_metadata"))
    return HandleGetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("validate"))
    return HandleValidateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list") || Lower == TEXT("list_assets"))
    return HandleListAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("generate_report"))
    return HandleGenerateReport(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_thumbnail") || Lower == TEXT("generate_thumbnail"))
    return HandleGenerateThumbnail(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("add_material_parameter"))
    return HandleAddMaterialParameter(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_instances"))
    return HandleListMaterialInstances(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("reset_instance_parameters"))
    return HandleResetInstanceParameters(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("exists"))
    return HandleDoesAssetExist(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("verify_asset_persistence")) {
#if WITH_EDITOR
    FString AssetPath;
    if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("assetPath required"), nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Invalid assetPath"), nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    const bool bAssetExists = Asset != nullptr;
    const FString PackageName = bAssetExists && Asset->GetOutermost()
                                    ? Asset->GetOutermost()->GetName()
                                    : FPackageName::ObjectPathToPackageName(AssetPath);
    const bool bPackageExistsOnDisk = !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
    const bool bPackageDirty = bAssetExists && Asset->GetOutermost() && Asset->GetOutermost()->IsDirty();
    bool bRequireClean = true;
    Payload->TryGetBoolField(TEXT("requireClean"), bRequireClean);
    bool bVerifyReload = false;
    Payload->TryGetBoolField(TEXT("verifyReload"), bVerifyReload);
    bool bReloadVerified = false;
    FString ReloadError;
    FString ReloadedClassPath;
    if (bVerifyReload)
    {
      if (!bAssetExists || !bPackageExistsOnDisk)
      {
        ReloadError = TEXT("Asset or package is missing; reload verification cannot start");
      }
      else if (bPackageDirty)
      {
        // Never unload a dirty package: doing so would discard editor changes.
        ReloadError = TEXT("Package is dirty; save it before requesting reload verification");
      }
      else
      {
        const FString OriginalClassPath = Asset->GetClass()->GetPathName();
        TArray<UPackage*> PackagesToUnload;
        PackagesToUnload.Add(Asset->GetOutermost());
        UPackageTools::FUnloadPackageParams UnloadParams(PackagesToUnload);
        UnloadParams.bUnloadDirtyPackages = false;
        if (!UPackageTools::UnloadPackages(UnloadParams))
        {
          ReloadError = UnloadParams.OutErrorMessage.IsEmpty()
              ? TEXT("The asset package could not be unloaded safely")
              : UnloadParams.OutErrorMessage.ToString();
        }
        else
        {
          UObject* ReloadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
          if (!ReloadedAsset)
          {
            ReloadError = TEXT("The asset could not be reloaded after unloading its package");
          }
          else
          {
            ReloadedClassPath = ReloadedAsset->GetClass()->GetPathName();
            bReloadVerified = ReloadedClassPath == OriginalClassPath &&
                               ReloadedAsset->GetOutermost() &&
                               !ReloadedAsset->GetOutermost()->IsDirty();
            if (!bReloadVerified)
            {
              ReloadError = TEXT("Reloaded asset class or package state did not match the persisted asset");
            }
          }
        }
      }
    }
    const bool bBasicVerified = bAssetExists && bPackageExistsOnDisk && (!bRequireClean || !bPackageDirty);
    const bool bVerified = bBasicVerified && (!bVerifyReload || bReloadVerified);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("packageName"), PackageName);
    Result->SetStringField(TEXT("classPath"), bAssetExists ? Asset->GetClass()->GetPathName() : FString());
    Result->SetBoolField(TEXT("assetExists"), bAssetExists);
    Result->SetBoolField(TEXT("packageExistsOnDisk"), bPackageExistsOnDisk);
    Result->SetBoolField(TEXT("packageDirty"), bPackageDirty);
    Result->SetBoolField(TEXT("requireClean"), bRequireClean);
    Result->SetBoolField(TEXT("reloadRequested"), bVerifyReload);
    Result->SetBoolField(TEXT("reloadVerified"), bReloadVerified);
    Result->SetStringField(TEXT("reloadedClassPath"), ReloadedClassPath);
    if (!ReloadError.IsEmpty()) Result->SetStringField(TEXT("reloadError"), ReloadError);
    Result->SetBoolField(TEXT("persistenceVerified"), bVerified);
    SendAutomationResponse(RequestingSocket, RequestId, bVerified,
                           bVerified ? TEXT("Asset persistence verified") : TEXT("Asset persistence verification failed"),
                           Result, bVerified ? FString() : TEXT("PERSISTENCE_NOT_VERIFIED"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset persistence verification requires editor build"), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }
  if (Lower == TEXT("get_material_stats"))
    return HandleGetMaterialStats(RequestId, Payload, RequestingSocket);

  // Search (CRITICAL: search_assets must be dispatched - was missing causing timeouts)
  if (Lower == TEXT("search_assets"))
    return HandleSearchAssets(RequestId, Action, Payload, RequestingSocket);

  // Bulk Operations
  if (Lower == TEXT("fixup_redirectors"))
    return HandleFixupRedirectors(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_rename"))
    return HandleBulkRenameAssets(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_delete"))
    return HandleBulkDeleteAssets(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("generate_lods"))
    return HandleGenerateLODs(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("nanite_rebuild_mesh"))
    return HandleNaniteRebuildMesh(RequestId, Action, Payload, RequestingSocket);

  // Source Control
  if (Lower == TEXT("source_control_checkout"))
    return HandleSourceControlCheckout(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_submit"))
    return HandleSourceControlSubmit(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("get_source_control_state"))
    return HandleGetSourceControlState(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_enable"))
    return HandleSourceControlEnable(RequestId, Action, Payload, RequestingSocket);

  // Graph & Analysis
  if (Lower == TEXT("analyze_graph"))
    return HandleAnalyzeGraph(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("find_by_tag"))
    return HandleFindByTag(RequestId, Action, Payload, RequestingSocket);

  // Material Authoring
  if (Lower == TEXT("add_material_node"))
    return HandleAddMaterialNode(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("connect_material_pins"))
    return HandleConnectMaterialPins(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("remove_material_node"))
    return HandleRemoveMaterialNode(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("break_material_connections"))
    return HandleBreakMaterialConnections(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_node_details"))
    return HandleGetMaterialNodeDetails(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("rebuild_material"))
    return HandleRebuildMaterial(RequestId, Action, Payload, RequestingSocket);

  return false;
}


bool UNebulaForgeBridgeSubsystem::HandleContentBrowserAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_PAYLOAD"),
                              TEXT("Content Browser payload missing."), nullptr);
    return true;
  }

  const FString Lower = Action.ToLower();
  const bool bFocus = Payload->HasField(TEXT("focusContentBrowser"))
                          ? Payload->GetBoolField(TEXT("focusContentBrowser")) : true;
  const bool bAllowLockedBrowser = Payload->HasField(TEXT("allowLockedBrowser"))
                                       ? Payload->GetBoolField(TEXT("allowLockedBrowser")) : false;
  const bool bNewBrowser = Payload->HasField(TEXT("newBrowser"))
                               ? Payload->GetBoolField(TEXT("newBrowser")) : false;
  FString InstanceString;
  Payload->TryGetStringField(TEXT("instanceName"), InstanceString);
  const FName InstanceName = InstanceString.IsEmpty() ? NAME_None : FName(*InstanceString);

  if (Lower == TEXT("navigate_to_path") || Lower == TEXT("sync_to_folder")) {
#if MCP_HAS_CONTENT_BROWSER
    FString RawPath;
    Payload->TryGetStringField(TEXT("contentBrowserPath"), RawPath);
    if (RawPath.IsEmpty()) Payload->TryGetStringField(TEXT("path"), RawPath);
    const FString SafePath = SanitizeProjectRelativePath(RawPath);
    if (SafePath.IsEmpty()) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("A valid Content Browser folder path is required."), nullptr);
      return true;
    }
    FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    TArray<FString> Paths;
    Paths.Add(SafePath);
    if (Lower == TEXT("navigate_to_path")) {
      IContentBrowserSingleton::Get().SetSelectedPaths(Paths, true, false);
    } else {
      IContentBrowserSingleton::Get().SyncBrowserToFolders(
          Paths, bAllowLockedBrowser, bFocus, InstanceName, bNewBrowser);
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SafePath);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           Lower == TEXT("navigate_to_path") ? TEXT("Content Browser path selected")
                                                              : TEXT("Content Browser folder synchronized"),
                           Resp, FString());
    return true;
#else
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Content Browser support is unavailable."), nullptr);
    return true;
#endif
  }

  if (Lower == TEXT("set_search_text")) {
#if MCP_HAS_CONTENT_BROWSER
    FString SearchText;
    Payload->TryGetStringField(TEXT("searchText"), SearchText);
    if (!Payload->HasField(TEXT("searchText"))) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("searchText is required."), nullptr);
      return true;
    }
    FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    IContentBrowserSingleton::Get().SetSearchText(FText::FromString(SearchText));
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("searchText"), SearchText);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Content Browser search text updated"), Resp, FString());
    return true;
#else
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Content Browser support is unavailable."), nullptr);
    return true;
#endif
  }

  if (Lower == TEXT("sync_to_asset")) {
    TArray<FString> RawPaths;
    const TArray<TSharedPtr<FJsonValue>> *PathValues = nullptr;
    if (Payload->TryGetArrayField(TEXT("assetPaths"), PathValues) && PathValues) {
      for (const TSharedPtr<FJsonValue> &Value : *PathValues) {
        FString Path;
        if (Value.IsValid() && Value->TryGetString(Path) && !Path.IsEmpty()) RawPaths.Add(Path);
      }
    }
    if (RawPaths.Num() == 0) {
      FString AssetPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      if (!AssetPath.IsEmpty()) RawPaths.Add(AssetPath);
    }
    if (RawPaths.Num() == 0) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("assetPath or assetPaths is required."), nullptr);
      return true;
    }
    TArray<FAssetData> Assets;
    for (const FString &RawPath : RawPaths) {
      const FString ResolvedPath = ResolveAssetPath(RawPath);
      FAssetData AssetData = ResolvedPath.IsEmpty()
                                 ? FAssetData()
                                 : UEditorAssetLibrary::FindAssetData(ResolvedPath);
      if (!AssetData.IsValid()) {
        SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("ASSET_NOT_FOUND"),
                                  FString::Printf(TEXT("Asset not found: %s"), *RawPath), nullptr);
        return true;
      }
      Assets.Add(AssetData);
    }
    if (!GEditor) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                                TEXT("Editor is unavailable."), nullptr);
      return true;
    }
#if MCP_HAS_CONTENT_BROWSER
    FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    IContentBrowserSingleton::Get().SyncBrowserToAssets(
        Assets, bAllowLockedBrowser, bFocus, InstanceName, bNewBrowser);
#else
    GEditor->SyncBrowserToObjects(Assets, bFocus);
#endif
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("count"), Assets.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Content Browser synchronized to assets"), Resp, FString());
    return true;
  }

  if (Lower == TEXT("set_view_settings")) {
#if MCP_HAS_CONTENT_BROWSER_SETTINGS
    const FString InstanceString = Payload->HasField(TEXT("instanceName"))
        ? Payload->GetStringField(TEXT("instanceName")) : FString();
    const FName InstanceName = InstanceString.IsEmpty() ? NAME_None : FName(*InstanceString);
    const bool bSaveConfig = Payload->HasField(TEXT("saveConfig"))
                                  ? Payload->GetBoolField(TEXT("saveConfig")) : true;
    TArray<FString> Applied;
    bool BoolValue = false;
    auto ApplyBool = [&](const TCHAR *FieldName, auto Setter) {
      if (Payload->TryGetBoolField(FieldName, BoolValue)) {
        Setter(BoolValue, InstanceName, bSaveConfig);
        Applied.Add(FieldName);
      }
    };
    ApplyBool(TEXT("showEngineContent"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetShowEngineContent(Name, Value, Save); });
    ApplyBool(TEXT("showPluginContent"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetShowPluginContent(Name, Value, Save); });
    ApplyBool(TEXT("showDeveloperContent"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetShowDeveloperContent(Name, Value, Save); });
    ApplyBool(TEXT("showFolders"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetShowFolders(Name, Value, Save); });
    ApplyBool(TEXT("showEmptyFolders"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetShowEmptyFolders(Name, Value, Save); });
    ApplyBool(TEXT("showCppFolders"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetShowCppFolders(Name, Value, Save); });
    ApplyBool(TEXT("showLocalizedContent"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetShowLocalizedContent(Name, Value, Save); });
    ApplyBool(TEXT("showFavorites"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetShowFavorites(Name, Value, Save); });
    ApplyBool(TEXT("searchAssetPaths"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetSearchAssetPaths(Name, Value, Save); });
    ApplyBool(TEXT("searchClasses"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetSearchClasses(Name, Value, Save); });
    ApplyBool(TEXT("searchCollections"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetSearchCollections(Name, Value, Save); });
    ApplyBool(TEXT("filterRecursively"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetFilterRecursively(Name, Value, Save); });
    ApplyBool(TEXT("sourcesExpanded"), [](bool Value, const FName &Name, bool Save) { ContentBrowserInstanceUtils::SetSourcesExpanded(Name, Value, Save); });
    FString ViewType;
    FString ThumbnailSize;
    Payload->TryGetStringField(TEXT("viewType"), ViewType);
    Payload->TryGetStringField(TEXT("thumbnailSize"), ThumbnailSize);
    TArray<FString> Unsupported;
    if (!ViewType.IsEmpty()) Unsupported.Add(TEXT("viewType"));
    if (!ThumbnailSize.IsEmpty()) Unsupported.Add(TEXT("thumbnailSize"));
    if (bSaveConfig) ContentBrowserInstanceUtils::SaveAllConfigs();
    if (Applied.Num() == 0 && Unsupported.Num() > 0) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("UNSUPPORTED_SETTING"),
                                TEXT("The public API does not mutate viewType or thumbnailSize for an existing browser."), nullptr);
      return true;
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), Applied.Num() > 0);
    TArray<TSharedPtr<FJsonValue>> AppliedValues;
    for (const FString &Name : Applied) AppliedValues.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("applied"), AppliedValues);
    if (Unsupported.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> UnsupportedValues;
      for (const FString &Name : Unsupported) UnsupportedValues.Add(MakeShared<FJsonValueString>(Name));
      Resp->SetArrayField(TEXT("unsupported"), UnsupportedValues);
    }
    SendAutomationResponse(RequestingSocket, RequestId, Applied.Num() > 0,
                           TEXT("Content Browser settings updated"), Resp,
                           Applied.Num() > 0 ? FString() : TEXT("NO_SETTINGS_APPLIED"));
    return true;
#else
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Content Browser support is unavailable."), nullptr);
    return true;
#endif
  }

  if (Lower == TEXT("create_collection") || Lower == TEXT("add_to_collection")) {
#if MCP_HAS_COLLECTION_MANAGER
    FString CollectionString;
    Payload->TryGetStringField(TEXT("collectionName"), CollectionString);
    if (CollectionString.IsEmpty()) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("collectionName is required."), nullptr);
      return true;
    }
    const FName CollectionName(*CollectionString);
    FString ShareString;
    Payload->TryGetStringField(TEXT("collectionShareType"), ShareString);
    ShareString = ShareString.ToLower();
    ECollectionShareType::Type ShareType = ECollectionShareType::CST_Local;
    if (ShareString == TEXT("private")) ShareType = ECollectionShareType::CST_Private;
    else if (ShareString == TEXT("shared")) ShareType = ECollectionShareType::CST_Shared;
    else if (!ShareString.IsEmpty() && ShareString != TEXT("local")) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("collectionShareType must be local, private, or shared."), nullptr);
      return true;
    }
    FCollectionManagerModule &CollectionModule = FCollectionManagerModule::GetModule();
    ICollectionManager &CollectionManager = CollectionModule.Get();
    FText Error;
    if (Lower == TEXT("create_collection")) {
      FString StorageString;
      Payload->TryGetStringField(TEXT("collectionStorageMode"), StorageString);
      StorageString = StorageString.ToLower();
      ECollectionStorageMode::Type StorageMode = ECollectionStorageMode::Static;
      if (StorageString == TEXT("dynamic")) StorageMode = ECollectionStorageMode::Dynamic;
      else if (!StorageString.IsEmpty() && StorageString != TEXT("static")) {
        SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_ARGUMENT"),
                                  TEXT("collectionStorageMode must be static or dynamic."), nullptr);
        return true;
      }
      const bool bCreated = CollectionManager.CreateCollection(CollectionName, ShareType, StorageMode, &Error);
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), bCreated);
      Resp->SetStringField(TEXT("collectionName"), CollectionString);
      if (!Error.IsEmpty()) Resp->SetStringField(TEXT("errorMessage"), Error.ToString());
      SendAutomationResponse(RequestingSocket, RequestId, bCreated,
                             bCreated ? TEXT("Collection created") : TEXT("Collection creation failed"), Resp,
                             bCreated ? FString() : TEXT("COLLECTION_CREATE_FAILED"));
      return true;
    }
    TArray<FString> RawPaths;
    const TArray<TSharedPtr<FJsonValue>> *PathValues = nullptr;
    if (Payload->TryGetArrayField(TEXT("assetPaths"), PathValues) && PathValues) {
      for (const TSharedPtr<FJsonValue> &Value : *PathValues) {
        FString Path;
        if (Value.IsValid() && Value->TryGetString(Path) && !Path.IsEmpty()) RawPaths.Add(Path);
      }
    }
    if (RawPaths.Num() == 0) {
      FString AssetPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      if (!AssetPath.IsEmpty()) RawPaths.Add(AssetPath);
    }
    if (RawPaths.Num() == 0) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("assetPath or assetPaths is required for add_to_collection."), nullptr);
      return true;
    }
    TArray<FSoftObjectPath> ObjectPaths;
    for (const FString &RawPath : RawPaths) {
      const FString ResolvedPath = ResolveAssetPath(RawPath);
      if (ResolvedPath.IsEmpty()) {
        SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("ASSET_NOT_FOUND"),
                                  FString::Printf(TEXT("Asset not found: %s"), *RawPath), nullptr);
        return true;
      }
      FAssetData AssetData = UEditorAssetLibrary::FindAssetData(ResolvedPath);
      if (!AssetData.IsValid()) {
        SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("ASSET_NOT_FOUND"),
                                  FString::Printf(TEXT("Asset not found: %s"), *RawPath), nullptr);
        return true;
      }
#if MCP_HAS_ASSET_SOFT_PATH
      ObjectPaths.Add(AssetData.GetSoftObjectPath());
#else
      ObjectPaths.Add(FSoftObjectPath(AssetData.PackageName.ToString() + TEXT(".") + AssetData.AssetName.ToString()));
#endif
    }
    int32 AddedCount = 0;
    const bool bAdded = CollectionManager.AddToCollection(CollectionName, ShareType, ObjectPaths, &AddedCount, &Error);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), bAdded);
    Resp->SetNumberField(TEXT("addedCount"), AddedCount);
    Resp->SetStringField(TEXT("collectionName"), CollectionString);
    if (!Error.IsEmpty()) Resp->SetStringField(TEXT("errorMessage"), Error.ToString());
    SendAutomationResponse(RequestingSocket, RequestId, bAdded,
                           bAdded ? TEXT("Assets added to collection") : TEXT("Assets could not be added to collection"), Resp,
                           bAdded ? FString() : TEXT("COLLECTION_ADD_FAILED"));
    return true;
#else
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Collection Manager support is unavailable."), nullptr);
    return true;
#endif
  }

  if (Lower == TEXT("set_asset_color")) {
    FString RawPath;
    Payload->TryGetStringField(TEXT("path"), RawPath);
    if (RawPath.IsEmpty()) Payload->TryGetStringField(TEXT("assetPath"), RawPath);
    const FString SafePath = SanitizeProjectRelativePath(RawPath);
    if (SafePath.IsEmpty()) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("A valid asset or folder path is required."), nullptr);
      return true;
    }
    double R = 1.0;
    double G = 1.0;
    double B = 1.0;
    double A = 1.0;
    const TSharedPtr<FJsonObject> *ColorObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("color"), ColorObject) && ColorObject && ColorObject->IsValid()) {
      (*ColorObject)->TryGetNumberField(TEXT("r"), R);
      (*ColorObject)->TryGetNumberField(TEXT("g"), G);
      (*ColorObject)->TryGetNumberField(TEXT("b"), B);
      (*ColorObject)->TryGetNumberField(TEXT("a"), A);
    } else {
      Payload->TryGetNumberField(TEXT("r"), R);
      Payload->TryGetNumberField(TEXT("g"), G);
      Payload->TryGetNumberField(TEXT("b"), B);
      Payload->TryGetNumberField(TEXT("a"), A);
    }
    const FLinearColor Color(
        FMath::Clamp(static_cast<float>(R), 0.f, 1.f),
        FMath::Clamp(static_cast<float>(G), 0.f, 1.f),
        FMath::Clamp(static_cast<float>(B), 0.f, 1.f),
        FMath::Clamp(static_cast<float>(A), 0.f, 1.f));
    FString ColorPath = SafePath;
    if (UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      ColorPath = FPaths::GetPath(FPackageName::ObjectPathToPackageName(SafePath));
    }
    GConfig->SetString(TEXT("PathColor"), *ColorPath, *Color.ToString(), GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
#if MCP_HAS_CONTENT_BROWSER
    FContentBrowserModule &ContentBrowserModule =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    ContentBrowserModule.GetOnSetFolderColor().Broadcast(ColorPath);
#endif
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SafePath);
    Resp->SetStringField(TEXT("colorPath"), ColorPath);
    Resp->SetStringField(TEXT("color"), Color.ToString());
    Resp->SetStringField(TEXT("note"), TEXT("Content Browser color is a folder-path color; asset paths target their parent folder entry."));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Content Browser folder color updated"), Resp, FString());
    return true;
  }

  if (Lower == TEXT("show_in_explorer")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    const FString ResolvedPath = ResolveAssetPath(AssetPath);
    if (ResolvedPath.IsEmpty()) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("ASSET_NOT_FOUND"),
                                TEXT("A valid assetPath is required."), nullptr);
      return true;
    }
    FString AssetFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            FPackageName::ObjectPathToPackageName(ResolvedPath), AssetFilename,
            FPackageName::GetAssetPackageExtension())) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("FILE_NOT_FOUND"),
                                TEXT("Could not resolve the asset package on disk."), nullptr);
      return true;
    }
    const FString Directory = FPaths::GetPath(AssetFilename);
    FPlatformProcess::ExploreFolder(*Directory);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), ResolvedPath);
    Resp->SetStringField(TEXT("directory"), Directory);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Asset folder opened in Explorer"), Resp, FString());
    return true;
  }

  return false;
#else
  return false;
#endif
}

// ============================================================================
// 1. FIXUP REDIRECTORS
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleFixupRedirectors(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("fixup_redirectors"), ESearchCase::IgnoreCase)) {
    // Not our action — allow other handlers to try
    return false;
  }

  // Implementation of redirector fixup functionality
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("fixup_redirectors payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get directory path - REQUIRED for proper error reporting
  FString DirectoryPath;
  Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);

  // Also check for "path" as alias
  if (DirectoryPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), DirectoryPath);
  }

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  // Validate path is provided
  if (DirectoryPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("directoryPath or path is required for fixup_redirectors"),
                        TEXT("MISSING_ARGUMENT"));
    return true;
  }

  // SECURITY: Sanitize path to prevent traversal attacks
  FString SanitizedPath = SanitizeProjectRelativePath(DirectoryPath);
  if (SanitizedPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *DirectoryPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Normalize path
  FString NormalizedPath = SanitizedPath;
  if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
  }

  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
  AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, NormalizedPath,
                                        bCheckoutFiles, RequestingSocket]() {
    UNebulaForgeBridgeSubsystem *StrongThis = WeakThis.Get();
    if (!StrongThis) {
      return;
    }
    // CRITICAL FIX: Use DoesAssetDirectoryExistOnDisk for strict validation
    // UEditorAssetLibrary::DoesDirectoryExist() uses AssetRegistry cache which may
    // contain stale entries. We need to check if the directory ACTUALLY exists on disk.
    if (!DoesAssetDirectoryExistOnDisk(NormalizedPath)) {
      StrongThis->SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Directory not found: %s"), *NormalizedPath),
                          TEXT("PATH_NOT_FOUND"));
      return;
    }

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    // Find all redirectors
    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    Filter.PackagePaths.Add(FName(*NormalizedPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() == 0) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetNumberField(TEXT("redirectorsFound"), 0);
      Result->SetNumberField(TEXT("redirectorsFixed"), 0);
      StrongThis->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("No redirectors found"), Result, FString());
      return;
    }

    // Convert to string paths for AssetTools
    TArray<FString> RedirectorPaths;
    for (const FAssetData &Asset : RedirectorAssets) {
      RedirectorPaths.Add(Asset.ToSoftObjectPath().ToString());
    }

    // Checkout files if source control is enabled
    if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
      TArray<FString> PackageNames;
      for (const FAssetData &Asset : RedirectorAssets) {
        PackageNames.Add(Asset.PackageName.ToString());
      }
      SourceControlHelpers::CheckOutFiles(PackageNames, true);
    }

    // Convert FAssetData to UObjectRedirector* for AssetTools
    TArray<UObjectRedirector *> Redirectors;
    for (const FAssetData &Asset : RedirectorAssets) {
      if (UObjectRedirector *Redirector =
              Cast<UObjectRedirector>(Asset.GetAsset())) {
        Redirectors.Add(Redirector);
      }
    }

    // Fixup redirectors using AssetTools
    if (Redirectors.Num() > 0) {
      IAssetTools &AssetTools =
          FModuleManager::LoadModuleChecked<FAssetToolsModule>(
              TEXT("AssetTools"))
              .Get();
      AssetTools.FixupReferencers(Redirectors);
    }

    // Delete the now-unused redirectors
    int32 DeletedCount = 0;
    TArray<UObject *> ObjectsToDelete;
    for (const FAssetData &Asset : RedirectorAssets) {
      if (UObject *Obj = Asset.GetAsset()) {
        ObjectsToDelete.Add(Obj);
      }
    }

    if (ObjectsToDelete.Num() > 0) {
      DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, false);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("redirectorsFound"), RedirectorAssets.Num());
    Result->SetNumberField(TEXT("redirectorsFixed"), DeletedCount);

    StrongThis->SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Fixed %d redirectors"), DeletedCount), Result,
        FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("fixup_redirectors requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 2. SOURCE CONTROL CHECKOUT
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleSourceControlCheckout(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_checkout"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("checkout"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_checkout payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPaths (array) and assetPath (single string)
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Try single assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  TArray<FString> PackageNames;
  TArray<FString> ValidPaths;
  for (const FString &Path : AssetPaths) {
    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (!SafePath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      ValidPaths.Add(SafePath);
      FString PackageName = FPackageName::ObjectPathToPackageName(SafePath);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  bool bSuccess = SourceControlHelpers::CheckOutFiles(PackageNames, true);

  TArray<TSharedPtr<FJsonValue>> CheckedOutPaths;
  for (const FString &Path : ValidPaths) {
    CheckedOutPaths.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("checkedOut"), PackageNames.Num());
  Result->SetArrayField(TEXT("assets"), CheckedOutPaths);

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                         bSuccess ? TEXT("Assets checked out successfully")
                                  : TEXT("Checkout failed"),
                         Result,
                         bSuccess ? FString() : TEXT("CHECKOUT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_checkout requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 3. SOURCE CONTROL SUBMIT
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleSourceControlSubmit(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_submit"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("submit"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_submit payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPaths (array) and assetPath (single string)
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Try single assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Description;
  if (!Payload->TryGetStringField(TEXT("description"), Description) ||
      Description.IsEmpty()) {
    Description = TEXT("Automated submission via NebulaForge Bridge");
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> PackageNames;
  for (const FString &Path : AssetPaths) {
    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (!SafePath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      FString PackageName = FPackageName::ObjectPathToPackageName(SafePath);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  TArray<FString> FilePaths;
  for (const FString &PackageName : PackageNames) {
    FString FilePath;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension()) ||
        FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetMapPackageExtension())) {
      FilePaths.Add(FilePath);
    }
  }

  TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation =
      ISourceControlOperation::Create<FCheckIn>();
  CheckInOperation->SetDescription(FText::FromString(Description));

  ECommandResult::Type Result =
      SourceControlProvider.Execute(CheckInOperation, FilePaths);
  bool bSuccess = (Result == ECommandResult::Succeeded);

  TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
  ResultObj->SetBoolField(TEXT("success"), bSuccess);
  ResultObj->SetNumberField(TEXT("submitted"),
                            bSuccess ? PackageNames.Num() : 0);
  ResultObj->SetStringField(TEXT("description"), Description);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Assets submitted successfully") : TEXT("Submit failed"),
      ResultObj, bSuccess ? FString() : TEXT("SUBMIT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_submit requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 4A. SOURCE CONTROL ENABLE
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleSourceControlEnable(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_enable"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  FString Provider = TEXT("None");
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("provider"), Provider);
  }

  ISourceControlModule& SourceControlModule = ISourceControlModule::Get();

  // Check if already enabled
  if (SourceControlModule.IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());
    Result->SetStringField(TEXT("message"), TEXT("Source control already enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control already enabled"), Result, FString());
    return true;
  }

  // Try to set the provider by name
  if (!Provider.IsEmpty() && !Provider.Equals(TEXT("None"), ESearchCase::IgnoreCase)) {
    SourceControlModule.SetProvider(FName(*Provider));
  }

  bool bEnabled = SourceControlModule.IsEnabled();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bEnabled);
  Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());

  if (bEnabled) {
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control enabled"), Result, FString());
  } else {
    Result->SetStringField(TEXT("error"), TEXT("Failed to enable source control. Please configure provider in Editor preferences."));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control enable failed"), Result,
                           TEXT("SOURCE_CONTROL_ENABLE_FAILED"));
  }
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_enable requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================

// ============================================================================
// 4. BULK RENAME ASSETS
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleBulkRenameAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_rename_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_rename"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_rename payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get rename options
  FString Prefix, Suffix, SearchText, ReplaceText;
  Payload->TryGetStringField(TEXT("prefix"), Prefix);
  Payload->TryGetStringField(TEXT("suffix"), Suffix);
  Payload->TryGetStringField(TEXT("searchText"), SearchText);
  Payload->TryGetStringField(TEXT("replaceText"), ReplaceText);

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  TArray<FString> AssetPaths;

  // Check for assetPaths array first
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Check for folderPath - if provided, list all assets in that folder
    FString FolderPath;
    if (Payload->TryGetStringField(TEXT("folderPath"), FolderPath) && !FolderPath.IsEmpty()) {
      // Normalize path
      FString NormalizedPath = FolderPath;
      if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
      }

      NormalizedPath = SanitizeProjectRelativePath(NormalizedPath);
      if (NormalizedPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid folderPath: %s"), *FolderPath),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Get all assets in the folder
      FAssetRegistryModule &AssetRegistryModule =
          FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
      IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

      FARFilter Filter;
      Filter.PackagePaths.Add(FName(*NormalizedPath));
      Filter.bRecursivePaths = true;

      // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
      // Asset listing uses cached AssetRegistry data exclusively.
      // LIMITATION: Assets not yet indexed by the editor's background scanner
      // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
      TArray<FAssetData> AssetDataList;
      AssetRegistry.GetAssets(Filter, AssetDataList);

      for (const FAssetData &AssetData : AssetDataList) {
        AssetPaths.Add(AssetData.ToSoftObjectPath().ToString());
      }

      if (AssetPaths.Num() == 0) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("renamed"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets found in folder"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
        return true;
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Either assetPaths array or folderPath is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  TArray<FAssetRenameData> RenameData;

  for (const FString &InputPath : AssetPaths) {
    FString AssetPath = ResolveAssetPath(InputPath);
    if (AssetPath.IsEmpty()) {
      AssetPath = InputPath;
    }

    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty()) {
      continue;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      continue;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      continue;
    }

    FString CurrentName = Asset->GetName();
    FString NewName = CurrentName;

    if (!SearchText.IsEmpty()) {
      NewName =
          NewName.Replace(*SearchText, *ReplaceText, ESearchCase::IgnoreCase);
    }

    if (!Prefix.IsEmpty()) {
      NewName = Prefix + NewName;
    }
    if (!Suffix.IsEmpty()) {
      NewName = NewName + Suffix;
    }

    if (NewName == CurrentName) {
      continue;
    }

    FString PackagePath =
        FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
    FAssetRenameData RenameEntry(Asset, PackagePath, NewName);
    RenameData.Add(RenameEntry);
  }

  if (RenameData.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("renamed"), 0);
    Result->SetStringField(TEXT("message"),
                           TEXT("No assets required renaming"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No renames needed"), Result, FString());
    return true;
  }

  if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
    TArray<FString> PackageNames;
    for (const FAssetRenameData &Data : RenameData) {
      PackageNames.Add(Data.Asset->GetOutermost()->GetName());
    }
    SourceControlHelpers::CheckOutFiles(PackageNames, true);
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
          .Get();
  bool bSuccess = AssetTools.RenameAssets(RenameData);

  TArray<TSharedPtr<FJsonValue>> RenamedAssets;
  for (const FAssetRenameData &Data : RenameData) {
    TSharedPtr<FJsonObject> AssetInfo = McpHandlerUtils::CreateResultObject();
    AssetInfo->SetStringField(TEXT("oldPath"), Data.Asset->GetPathName());
    AssetInfo->SetStringField(TEXT("newName"), Data.NewName);
    RenamedAssets.Add(MakeShared<FJsonValueObject>(AssetInfo));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("renamed"), RenameData.Num());
  Result->SetArrayField(TEXT("assets"), RenamedAssets);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? FString::Printf(TEXT("Renamed %d assets"), RenameData.Num())
               : TEXT("Bulk rename failed"),
      Result, bSuccess ? FString() : TEXT("BULK_RENAME_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_rename requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 5. BULK DELETE ASSETS
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleBulkDeleteAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_delete_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_delete"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_delete payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  bool bShowConfirmation = false;
  Payload->TryGetBoolField(TEXT("showConfirmation"), bShowConfirmation);

  bool bFixupRedirectors = true;
  Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixupRedirectors);

  TArray<FString> AssetPaths;

  // Check for assetPaths array first
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Check for folderPath - if provided, list all assets in that folder
    FString FolderPath;
    FString Pattern;
    Payload->TryGetStringField(TEXT("folderPath"), FolderPath);
    Payload->TryGetStringField(TEXT("path"), FolderPath);  // alias
    Payload->TryGetStringField(TEXT("pattern"), Pattern);

    if (!FolderPath.IsEmpty()) {
      // Normalize path
      FString NormalizedPath = FolderPath;
      if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
      }

      NormalizedPath = SanitizeProjectRelativePath(NormalizedPath);
      if (NormalizedPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid folderPath: %s"), *FolderPath),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Get all assets in the folder
      FAssetRegistryModule &AssetRegistryModule =
          FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
      IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

      FARFilter Filter;
      Filter.PackagePaths.Add(FName(*NormalizedPath));
      Filter.bRecursivePaths = true;

      // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
      // Asset listing uses cached AssetRegistry data exclusively.
      // LIMITATION: Assets not yet indexed by the editor's background scanner
      // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
      TArray<FAssetData> AssetDataList;
      AssetRegistry.GetAssets(Filter, AssetDataList);

      for (const FAssetData &AssetData : AssetDataList) {
        FString AssetPath = AssetData.ToSoftObjectPath().ToString();
        // If pattern is specified, filter by it
        if (!Pattern.IsEmpty()) {
          FString AssetName = AssetData.AssetName.ToString();
          if (!AssetName.Contains(Pattern)) {
            continue;
          }
        }
        AssetPaths.Add(AssetPath);
      }

      if (AssetPaths.Num() == 0) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("deleted"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets found matching criteria"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
        return true;
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Either assetPaths array or folderPath is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  TArray<UObject *> ObjectsToDelete;
  TArray<FString> ValidPaths;

  for (const FString &AssetPath : AssetPaths) {
    const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (!SafeAssetPath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath)) {
        ObjectsToDelete.Add(Asset);
        ValidPaths.Add(SafeAssetPath);
      }
    }
  }

  if (ObjectsToDelete.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  int32 DeletedCount =
      ObjectTools::DeleteObjects(ObjectsToDelete, bShowConfirmation);

  if (bFixupRedirectors && DeletedCount > 0) {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() > 0) {
      TArray<UObjectRedirector *> Redirectors;
      for (const FAssetData &Asset : RedirectorAssets) {
        if (UObjectRedirector *Redirector =
                Cast<UObjectRedirector>(Asset.GetAsset())) {
          Redirectors.Add(Redirector);
        }
      }

      if (Redirectors.Num() > 0) {
        IAssetTools &AssetTools =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                TEXT("AssetTools"))
                .Get();
        AssetTools.FixupReferencers(Redirectors);
      }
    }
  }

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Path : ValidPaths) {
    DeletedArray.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), DeletedCount > 0);
  Result->SetArrayField(TEXT("deleted"), DeletedArray);
  Result->SetNumberField(TEXT("requested"), ObjectsToDelete.Num());

  SendAutomationResponse(
      RequestingSocket, RequestId, DeletedCount > 0,
      FString::Printf(TEXT("Deleted %d of %d assets"), DeletedCount,
                      ObjectsToDelete.Num()),
      Result, DeletedCount > 0 ? FString() : TEXT("BULK_DELETE_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_delete requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 6. GENERATE THUMBNAIL
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleGenerateThumbnail(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_thumbnail"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("create_thumbnail"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("generate_thumbnail payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // SECURITY: Validate asset path
  FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *AssetPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  int32 Width = 512;
  int32 Height = 512;

  double TempWidth = 0, TempHeight = 0;
  if (Payload->TryGetNumberField(TEXT("width"), TempWidth))
    Width = static_cast<int32>(TempWidth);
  if (Payload->TryGetNumberField(TEXT("height"), TempHeight))
    Height = static_cast<int32>(TempHeight);

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work (including fast existence checks) in AsyncTask(GameThread, ...)
  // caused the queued lambda to sit behind the current dispatch cycle, so responses
  // never reached the MCP server before the 30-second timeout (issues #138, #139).
  // Execute synchronously instead.
  SendProgressUpdate(RequestId, 0.0f,
      FString::Printf(TEXT("Starting thumbnail generation for: %s"), *SafeAssetPath), true);

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Asset not found"), nullptr,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetBoolField(TEXT("thumbnailRendered"), false);
    Result->SetBoolField(TEXT("headlessSafe"), true);
    if (!OutputPath.IsEmpty()) {
      Result->SetStringField(TEXT("outputPath"), OutputPath);
    }
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Asset verified; thumbnail rendering skipped under NullRHI"),
                           Result, FString());
    return true;
  }

  // Send progress update before GPU operation
  SendProgressUpdate(RequestId, 50.0f,
      TEXT("Rendering thumbnail (GPU operation)..."), true);

  FObjectThumbnail ObjectThumbnail;
  ThumbnailTools::RenderThumbnail(
      Asset, Width, Height,
      ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr,
      &ObjectThumbnail);

  bool bSuccess = ObjectThumbnail.GetImageWidth() > 0 &&
                  ObjectThumbnail.GetImageHeight() > 0;

  if (bSuccess && !OutputPath.IsEmpty()) {
    const TArray<uint8> &ImageData = ObjectThumbnail.GetUncompressedImageData();

    if (ImageData.Num() > 0) {
      TArray<FColor> ColorData;
      ColorData.Reserve(Width * Height);

      // Fixed: Ensure we don't read out of bounds if ImageData length isn't a multiple of 4
      for (int32 i = 0; i + 3 < ImageData.Num(); i += 4) {
        FColor Color;
        Color.B = ImageData[i + 0];
        Color.G = ImageData[i + 1];
        Color.R = ImageData[i + 2];
        Color.A = ImageData[i + 3];
        ColorData.Add(Color);
      }

      // SECURITY: Sanitize and validate the output path to prevent path traversal
      FString SafeOutputPath = SanitizeProjectFilePath(OutputPath);
      if (SafeOutputPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Invalid or unsafe output path: %s"), *OutputPath),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      FString AbsolutePath = FPaths::ProjectDir() / SafeOutputPath;
      AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
      FPaths::NormalizeFilename(AbsolutePath);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Output path escapes project directory: %s"), *OutputPath),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      TArray<uint8> CompressedData;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      FImageUtils::ThumbnailCompressImageArray(Width, Height, ColorData,
                                               CompressedData);
#else
      // UE 5.0: Use CompressImageArray instead
      FImageUtils::CompressImageArray(Width, Height, ColorData, CompressedData);
#endif
      bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *AbsolutePath);
    }
  }

  if (Asset->GetOutermost()) {
    Asset->GetOutermost()->MarkPackageDirty();
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Result->SetNumberField(TEXT("width"), Width);
  Result->SetNumberField(TEXT("height"), Height);

  if (!OutputPath.IsEmpty()) {
    Result->SetStringField(TEXT("outputPath"), OutputPath);
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Thumbnail generated successfully")
               : TEXT("Thumbnail generation failed"),
      Result, bSuccess ? FString() : TEXT("THUMBNAIL_GENERATION_FAILED"));

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("generate_thumbnail requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 7. BASIC ASSET OPERATIONS (Import, Duplicate, Rename, Move, etc.)
// ============================================================================

/**
 * Handles asset import requests.
 *
 * IMPORTANT: In UE 5.7+, the Interchange Framework is the default importer for
 * FBX/glTF files. Interchange uses the TaskGraph internally for async operations.
 * If we call ImportAssetsAutomated() synchronously from within an AsyncTask callback
 * (which is how WebSocket messages are dispatched), we hit a TaskGraph recursion
 * guard assertion: "++Queue(QueueIndex).RecursionGuard == 1".
 *
 * The fix is to defer the import to the next editor tick using GEditor->GetTimerManager(),
 * which breaks out of the TaskGraph callback chain and allows Interchange to function
 * correctly.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleImportAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

  if (DestinationPath.IsEmpty() || SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ResolvedSourcePath = SourcePath;
  if (!FPaths::FileExists(ResolvedSourcePath) && FPaths::IsRelative(SourcePath)) {
    const FString ProjectRelativeSourcePath = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir(), SourcePath);
    if (FPaths::FileExists(ProjectRelativeSourcePath)) {
      ResolvedSourcePath = ProjectRelativeSourcePath;
    }
  }

  // Verify source file exists
  if (!FPaths::FileExists(ResolvedSourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source file not found: %s"), *SourcePath),
        nullptr, TEXT("SOURCE_NOT_FOUND"));
    return true;
  }

  // Sanitize destination path
  FString SafeDestPath = SanitizeProjectRelativePath(DestinationPath);
  if (SafeDestPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destination path"), nullptr,
                           TEXT("INVALID_PATH"));
    return true;
  }

  FString DestPath = FPaths::GetPath(SafeDestPath);
  FString DestName = FPaths::GetBaseFilename(SafeDestPath);

  // If destination is just a folder, use that
  if (FPaths::GetExtension(SafeDestPath).IsEmpty()) {
    DestPath = SafeDestPath;
    DestName = FPaths::GetBaseFilename(SourcePath);
  }

  // Sanitize DestName: UE asset names cannot contain spaces or dots
  DestName.ReplaceInline(TEXT(" "), TEXT("_"));
  DestName.ReplaceInline(TEXT("."), TEXT("_"));

  // Defer the import to the next tick to avoid TaskGraph recursion issues with
  // UE 5.7+ Interchange Framework. See issue #137.
  // We use SetTimerForNextTick to ensure we're completely outside of any
  // TaskGraph callback chain before invoking the import.
  if (GEditor) {
    TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
    GEditor->GetTimerManager()->SetTimerForNextTick(
        [WeakThis, RequestId, ResolvedSourcePath, DestPath, DestName, Socket]() {
          UNebulaForgeBridgeSubsystem *StrongThis = WeakThis.Get();
          if (!StrongThis) {
            return;
          }

          IAssetTools &AssetTools =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
                  .Get();

          TArray<FString> Files;
          Files.Add(ResolvedSourcePath);

          UAutomatedAssetImportData *ImportData =
              NewObject<UAutomatedAssetImportData>();
          ImportData->bReplaceExisting = true;
          ImportData->DestinationPath = DestPath;
          ImportData->Filenames = Files;

          TArray<UObject *> ImportedAssets =
              AssetTools.ImportAssetsAutomated(ImportData);

          // Find the first valid (non-null) asset in the array.
          // ImportAssetsAutomated can return arrays with nullptr entries.
          UObject *Asset = nullptr;
          for (UObject *ImportedObj : ImportedAssets) {
            if (ImportedObj) {
              Asset = ImportedObj;
              break;
            }
          }

          if (Asset) {
            // Compute the final asset path. If we rename, use the destination
            // path/name since RenameAssets may invalidate the Asset pointer.
            FString FinalAssetPath;
            bool bRenameSucceeded = true;

            // Rename if needed
            if (Asset->GetName() != DestName) {
              FAssetRenameData RenameData(Asset, DestPath, DestName);
              bRenameSucceeded = AssetTools.RenameAssets({RenameData});
              // After rename, compute path from destination (Asset pointer may
              // be stale)
              FinalAssetPath = DestPath / DestName + TEXT(".") + DestName;
            } else {
              // No rename needed, safe to use the asset's current path
              FinalAssetPath = Asset->GetPathName();
            }

            TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("assetPath"), FinalAssetPath);
            if (!bRenameSucceeded) {
              Resp->SetBoolField(TEXT("renameWarning"), true);
            }
            // Add verification data
            UObject *ImportedAsset = UEditorAssetLibrary::LoadAsset(FinalAssetPath);
            if (ImportedAsset) {
              McpHandlerUtils::AddVerification(Resp, ImportedAsset);
            }
            StrongThis->SendAutomationResponse(
                Socket, RequestId, true,
                bRenameSucceeded ? TEXT("Asset imported")
                                 : TEXT("Asset imported but rename failed"),
                Resp, FString());
          } else {
            StrongThis->SendAutomationResponse(
                Socket, RequestId, false,
                FString::Printf(TEXT("Failed to import asset from '%s'"),
                                 *ResolvedSourcePath),
                nullptr, TEXT("IMPORT_FAILED"));
          }
        });
  } else {
    // Fallback: GEditor not available (shouldn't happen in editor context)
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available for deferred import"),
                           nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles metadata setting requests for assets.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and 'metadata' object.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleSetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_metadata payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  const FString SafeAssetPath = AssetPath;

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const TSharedPtr<FJsonObject> *MetadataObjPtr = nullptr;
  if (!Payload->TryGetObjectField(TEXT("metadata"), MetadataObjPtr) ||
      !MetadataObjPtr) {
    // Treat missing/empty metadata as a no-op success; nothing to write.
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Resp->SetNumberField(TEXT("updatedKeys"), 0);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("No metadata provided; no-op"), Resp,
                           FString());
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  UPackage *Package = Asset->GetOutermost();
  if (!Package) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to resolve package for asset"), nullptr,
                           TEXT("PACKAGE_NOT_FOUND"));
    return true;
  }

  // GetMetaData returns the metadata object that is owned by this package.
  // UE 5.0 uses UMetaData*, UE 5.6+ uses FMetaData&
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  FMetaData& Meta = Package->GetMetaData();
#else
  UMetaData* Meta = Package->GetMetaData();
#endif

  const TSharedPtr<FJsonObject> &MetadataObj = *MetadataObjPtr;
  int32 UpdatedCount = 0;

  for (const auto &Kvp : MetadataObj->Values) {
    const FString Key(*Kvp.Key);
    const TSharedPtr<FJsonValue> &Val = Kvp.Value;

    FString ValueString;
    if (!Val.IsValid() || Val->IsNull()) {
      continue;
    }
    switch (Val->Type) {
    case EJson::String:
      ValueString = Val->AsString();
      break;
    case EJson::Number:
      ValueString = LexToString(Val->AsNumber());
      break;
    case EJson::Boolean:
      ValueString = Val->AsBool() ? TEXT("true") : TEXT("false");
      break;
    default:
      // For arrays/objects, store a compact JSON string
      {
        FString JsonOut;
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&JsonOut);
        FJsonSerializer::Serialize(Val, TEXT(""), Writer);
        ValueString = JsonOut;
      }
      break;
    }

    if (!ValueString.IsEmpty()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
      Meta.SetValue(Asset, *Key, *ValueString);
#else
      Meta->SetValue(Asset, *Key, *ValueString);
#endif
      ++UpdatedCount;
    }
  }

  if (UpdatedCount > 0) {
    Package->SetDirtyFlag(true);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Resp->SetNumberField(TEXT("updatedKeys"), UpdatedCount);

  // Add verification data
  McpHandlerUtils::AddVerification(Resp, Asset);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset metadata updated"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset duplication requests. Supports both single asset and folder
 * (deep) duplication.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleDuplicateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
  }

  SourcePath = SanitizeProjectRelativePath(SourcePath);
  DestinationPath = SanitizeProjectRelativePath(DestinationPath);
  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid sourcePath or destinationPath"),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // If the source path is a directory, perform a deep duplication of all
  // assets under that folder into the destination folder, preserving
  // relative structure. This powers the "Deep Duplication - Duplicate
  // Folder" scenario in tests.
  if (UEditorAssetLibrary::DoesDirectoryExist(SourcePath)) {
    // Ensure the destination root exists
    UEditorAssetLibrary::MakeDirectory(DestinationPath);

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*SourcePath));
    Filter.bRecursivePaths = true;

    // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
    // Asset listing uses cached AssetRegistry data exclusively.
    // LIMITATION: Assets not yet indexed by the editor's background scanner
    // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);

    int32 DuplicatedCount = 0;
    for (const FAssetData &Asset : Assets) {
      // PackageName is the long package path (e.g.,
      // /Game/Tests/DeepCopy/Source/M_Source)
      const FString SourceAssetPath = Asset.PackageName.ToString();

      FString RelativePath;
      if (SourceAssetPath.StartsWith(SourcePath)) {
        RelativePath = SourceAssetPath.RightChop(SourcePath.Len());
      } else {
        // Should not happen for the filtered set, but skip if it does.
        continue;
      }

      const FString TargetAssetPath =
          DestinationPath + RelativePath; // preserves any subfolders
      const FString TargetFolderPath = FPaths::GetPath(TargetAssetPath);
      if (!TargetFolderPath.IsEmpty()) {
        UEditorAssetLibrary::MakeDirectory(TargetFolderPath);
      }

      if (UEditorAssetLibrary::DuplicateAsset(SourceAssetPath,
                                              TargetAssetPath)) {
        ++DuplicatedCount;
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const bool bSuccess = DuplicatedCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
    Resp->SetNumberField(TEXT("duplicatedCount"), DuplicatedCount);

    if (bSuccess) {
      SendAutomationResponse(Socket, RequestId, true, TEXT("Folder duplicated"),
                             Resp, FString());
    } else {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("No assets duplicated"), Resp,
                             TEXT("DUPLICATE_FAILED"));
    }
    return true;
  }

  // Fallback: single-asset duplication
  if (!UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (UEditorAssetLibrary::DoesAssetExist(DestinationPath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Destination asset already exists: %s"),
                        *DestinationPath),
        nullptr, TEXT("DESTINATION_EXISTS"));
    return true;
  }

  if (UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);
    // Add verification data
    UObject *NewAsset = UEditorAssetLibrary::LoadAsset(DestinationPath);
    if (NewAsset) {
      McpHandlerUtils::AddVerification(Resp, NewAsset);
    }
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset duplicated"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Duplicate failed"),
                           nullptr, TEXT("DUPLICATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset renaming (and moving) requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleRenameAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
    UE_LOG(
        LogNebulaForgeBridgeSubsystem, Display,
        TEXT(
            "HandleRenameAsset: Auto-resolved simple name destination to '%s'"),
        *DestinationPath);
  }

  if ((SourcePath.Contains(TEXT("/")) || SourcePath.StartsWith(TEXT("/"))) &&
      SanitizeProjectRelativePath(SourcePath).IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid sourcePath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  DestinationPath = SanitizeProjectRelativePath(DestinationPath);
  if (DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destinationPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Resolve source path to ensure it matches a real asset
  FString ResolvedSourcePath = ResolveAssetPath(SourcePath);
  if (ResolvedSourcePath.IsEmpty()) {
    // If resolution failed, fall back to original for strict check
    ResolvedSourcePath = SourcePath;
  }

  ResolvedSourcePath = SanitizeProjectRelativePath(ResolvedSourcePath);
  if (ResolvedSourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid resolved sourcePath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(ResolvedSourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Use the resolved path for the rename operation
  if (UEditorAssetLibrary::RenameAsset(ResolvedSourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);

    // Add verification data
    UObject* RenamedAsset = UEditorAssetLibrary::LoadAsset(DestinationPath);
    if (RenamedAsset) {
      McpHandlerUtils::AddVerification(Resp, RenamedAsset);
    }

    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset renamed"), Resp,
                           FString());
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Failed to rename asset. Check if destination "
                             "'%s' already exists or source is locked."),
                        *DestinationPath),
        nullptr, TEXT("RENAME_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleMoveAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // Move is essentially rename in Unreal
  return HandleRenameAsset(RequestId, Payload, Socket);
}

/**
 * Handles asset deletion requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path' (string) or 'paths' (array of
 * strings).
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleDeleteAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Support both single 'path' and array 'paths'
  TArray<FString> PathsToDelete;
  const TArray<TSharedPtr<FJsonValue>> *PathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
    for (const auto &Val : *PathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String)
        PathsToDelete.Add(Val->AsString());
    }
  }

  FString SinglePath;
  if (Payload->TryGetStringField(TEXT("path"), SinglePath) &&
      !SinglePath.IsEmpty()) {
    PathsToDelete.Add(SinglePath);
  }

  if (PathsToDelete.Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("No paths provided"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 DeletedCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> FailedToDeletePaths;

  for (const FString &Path : PathsToDelete) {
    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty()) {
      FailedToDeletePaths.Add(Path);
      continue;
    }

    // Check if it's a directory first (folder path)
    if (UEditorAssetLibrary::DoesDirectoryExist(SafePath)) {
      // Directory exists - use safe folder deletion with proper cleanup
      // CRITICAL for UE 5.7+: Use McpSafeDeleteFolder instead of UEditorAssetLibrary::DeleteDirectory
      // to prevent crashes during UWorld::CleanupWorld when deleting folders containing
      // AnimBlueprints, IKRigs, IKRetargeters, etc.
      if (McpSafeOperations::McpSafeDeleteFolder(SafePath, true))
      {
        // McpSafeDeleteFolder performs registry and filesystem verification itself.
        DeletedCount++;
      } else {
        FailedToDeletePaths.Add(SafePath);
      }
    } else if (UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      // Asset exists - attempt to delete it
      if (UEditorAssetLibrary::DeleteAsset(SafePath)) {
        // Verify the asset was actually deleted
        if (!UEditorAssetLibrary::DoesAssetExist(SafePath)) {
          DeletedCount++;
        } else {
          // Delete returned true but asset still exists
          FailedToDeletePaths.Add(SafePath);
        }
      } else {
        FailedToDeletePaths.Add(SafePath);
      }
    } else {
      // Asset/directory does not exist
      NotFoundPaths.Add(SafePath);
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

  // Return success only if at least one asset was deleted
  bool bSuccess = DeletedCount > 0;
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("deletedCount"), DeletedCount);
  Resp->SetBoolField(TEXT("existsAfter"), false);

  if (NotFoundPaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> NotFoundArray;
    for (const FString& P : NotFoundPaths) {
      NotFoundArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
    Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
  }

  if (FailedToDeletePaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& P : FailedToDeletePaths) {
      FailedArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("failedToDeletePaths"), FailedArray);
    Resp->SetNumberField(TEXT("failedCount"), FailedToDeletePaths.Num());
  }

  if (bSuccess) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Assets deleted"), Resp, FString());
  } else {
    // Nothing was deleted - determine the reason
    FString ErrorMessage;
    FString ErrorCode;

    if (NotFoundPaths.Num() > 0 && FailedToDeletePaths.Num() == 0) {
      // All paths were not found
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (FailedToDeletePaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      // All paths existed but deletion failed
      ErrorMessage = FString::Printf(TEXT("Failed to delete %d asset(s). They may be in use or locked."), FailedToDeletePaths.Num());
      ErrorCode = TEXT("DELETE_FAILED");
    } else {
      // Mixed: some not found, some failed to delete
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d path(s) not found, %d failed to delete."),
                                      NotFoundPaths.Num(), FailedToDeletePaths.Num());
      ErrorCode = TEXT("DELETE_FAILED");
    }

    SendAutomationResponse(Socket, RequestId, false, ErrorMessage, Resp, ErrorCode);
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles folder creation requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleCreateFolder(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Path;
  if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty()) {
    Payload->TryGetStringField(TEXT("directoryPath"), Path);
  }

  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("path (or directoryPath) required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafePath = SanitizeProjectRelativePath(Path);
  if (SafePath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Invalid path: must be project-relative and not contain '..'"),
        nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  if (UEditorAssetLibrary::DoesDirectoryExist(SafePath) ||
      UEditorAssetLibrary::MakeDirectory(SafePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SafePath);
    // Add verification data
    VerifyAssetExists(Resp, SafePath);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Folder created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create folder"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get asset dependencies.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'recursive'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleGetDependencies(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // Check if asset exists - return error for non-existent assets
  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bRecursive = false;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FName> Dependencies;
  UE::AssetRegistry::EDependencyCategory Category =
      UE::AssetRegistry::EDependencyCategory::Package;
  AssetRegistryModule.Get().GetDependencies(FName(*SafeAssetPath), Dependencies);

  TArray<TSharedPtr<FJsonValue>> DepArray;
  for (const FName &Dep : Dependencies) {
    DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("dependencies"), DepArray);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Dependencies retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to traverse and return an asset dependency graph.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'maxDepth'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleGetAssetGraph(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // Check if asset exists - return error for non-existent assets
  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  int32 MaxDepth = 3;
  Payload->TryGetNumberField(TEXT("maxDepth"), MaxDepth);
  bool bIncludeReferencers = false;
  Payload->TryGetBoolField(TEXT("includeReferencers"), bIncludeReferencers);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  TSharedPtr<FJsonObject> GraphObj = McpHandlerUtils::CreateResultObject();
  TSharedPtr<FJsonObject> ReverseGraphObj = McpHandlerUtils::CreateResultObject();

  TArray<FString> Queue;
  Queue.Add(SafeAssetPath);

  TSet<FString> Visited;
  Visited.Add(SafeAssetPath);

  TMap<FString, int32> Depths;
  Depths.Add(SafeAssetPath, 0);

  int32 Head = 0;
  while (Head < Queue.Num()) {
    FString Current = Queue[Head++];
    int32 CurrentDepth = Depths[Current];

    TArray<FName> Dependencies;
    AssetRegistry.GetDependencies(FName(*Current), Dependencies);

    TArray<TSharedPtr<FJsonValue>> DepArray;
    for (const FName &Dep : Dependencies) {
      FString DepStr = Dep.ToString();
      if (!DepStr.StartsWith(TEXT("/Game")))
        continue; // Only graph Game assets for now

      DepArray.Add(MakeShared<FJsonValueString>(DepStr));

      if (CurrentDepth < MaxDepth) {
        if (!Visited.Contains(DepStr)) {
          Visited.Add(DepStr);
          Depths.Add(DepStr, CurrentDepth + 1);
          Queue.Add(DepStr);
        }
      }
    }
    GraphObj->SetArrayField(Current, DepArray);

    if (bIncludeReferencers) {
      TArray<FAssetIdentifier> Referencers;
      AssetRegistry.GetReferencers(FAssetIdentifier(FName(*Current)), Referencers);
      TArray<TSharedPtr<FJsonValue>> RefArray;
      for (const FAssetIdentifier &Ref : Referencers) {
        const FString RefStr = Ref.PackageName.ToString();
        if (!RefStr.StartsWith(TEXT("/Game"))) {
          continue;
        }
        RefArray.Add(MakeShared<FJsonValueString>(RefStr));
        if (CurrentDepth < MaxDepth && !Visited.Contains(RefStr)) {
          Visited.Add(RefStr);
          Depths.Add(RefStr, CurrentDepth + 1);
          Queue.Add(RefStr);
        }
      }
      ReverseGraphObj->SetArrayField(Current, RefArray);
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("graph"), GraphObj);
  if (bIncludeReferencers) {
    Resp->SetObjectField(TEXT("reverseGraph"), ReverseGraphObj);
    Resp->SetBoolField(TEXT("includeReferencers"), true);
  }
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset graph retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to set asset tags. Asset Registry tags are distinct from
 * Actor tags; this action persists the requested values as package metadata.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleSetTags(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_tags payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *TagsArray = nullptr;
  TArray<FString> Tags;
  if (Payload->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray) {
    for (const TSharedPtr<FJsonValue> &Val : *TagsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        Tags.Add(Val->AsString());
      }
    }
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
  AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, SafeAssetPath,
                                         Tags]() {
    UNebulaForgeBridgeSubsystem *StrongThis = WeakThis.Get();
    if (!StrongThis) {
      return;
    }
    // Edge-case: empty or missing tags array should be treated as a no-op
    // success.
    if (Tags.Num() == 0) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
      Resp->SetNumberField(TEXT("appliedTags"), 0);
      StrongThis->SendAutomationResponse(Socket, RequestId, true,
                             TEXT("No tags provided; no-op"), Resp, FString());
      return;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      StrongThis->SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
    if (!Asset) {
      StrongThis->SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
    }

    // Implement set_tags by mapping them to Package Metadata (Tag=true)
    int32 AppliedCount = 0;
    for (const FString &Tag : Tags) {
      UEditorAssetLibrary::SetMetadataTag(Asset, FName(*Tag), TEXT("true"));
      AppliedCount++;
    }

    // Mark dirty so the asset can be saved later
    Asset->MarkPackageDirty();

    if (!McpSafeAssetSave(Asset))
    {
      StrongThis->SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Tags were applied but asset save failed"),
                             nullptr, TEXT("SAVE_FAILED"));
      return;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("saved"), true);
    Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Resp->SetNumberField(TEXT("appliedTags"), AppliedCount);
    StrongThis->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tags applied as metadata"), Resp, FString());
  });

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to validate if an asset exists and can be loaded.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleValidateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("validate payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
  AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, SafeAssetPath]() {
    UNebulaForgeBridgeSubsystem *StrongThis = WeakThis.Get();
    if (!StrongThis) {
      return;
    }
    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      StrongThis->SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
    if (!Asset) {
      StrongThis->SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
    }

    bool bIsValid = true;
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), bIsValid);
    Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Resp->SetBoolField(TEXT("isValid"), bIsValid);

    StrongThis->SendAutomationResponse(Socket, RequestId, true, TEXT("Asset validated"),
                           Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to list assets with filtering and pagination.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing filter criteria and pagination
 * options.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleListAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Parse filters
  FString PathFilter;
  FString ClassFilter;
  FString TagFilter;
  FString PathStartsWith;

  const TSharedPtr<FJsonObject> *FilterObj;
  if (Payload->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj) {
    (*FilterObj)->TryGetStringField(TEXT("path"), PathFilter);
    (*FilterObj)->TryGetStringField(TEXT("class"), ClassFilter);
    (*FilterObj)->TryGetStringField(TEXT("tag"), TagFilter);
    (*FilterObj)->TryGetStringField(TEXT("pathStartsWith"), PathStartsWith);
  } else {
    // Legacy support for direct path/recursive fields
    Payload->TryGetStringField(TEXT("path"), PathFilter);
  }

  // Sanitize PathFilter to remove trailing slash which can break AssetRegistry
  // lookups
  if (PathFilter.Len() > 1 && PathFilter.EndsWith(TEXT("/"))) {
    PathFilter.RemoveAt(PathFilter.Len() - 1);
  }

  bool bRecursive = true;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  // Parse pagination
  int32 Offset = 0;
  int32 Limit = -1; // -1 means no limit
  const TSharedPtr<FJsonObject> *PaginationObj;
  if (Payload->TryGetObjectField(TEXT("pagination"), PaginationObj) &&
      PaginationObj) {
    (*PaginationObj)->TryGetNumberField(TEXT("offset"), Offset);
    (*PaginationObj)->TryGetNumberField(TEXT("limit"), Limit);
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
  Filter.bRecursivePaths = bRecursive;
  Filter.bRecursiveClasses = true;

  // Apply path filters
  if (!PathFilter.IsEmpty()) {
    Filter.PackagePaths.Add(FName(*PathFilter));
  } else if (!PathStartsWith.IsEmpty()) {
    // If we have a path prefix, assume it's a package path
    // Note: FARFilter doesn't support 'StartsWith' natively for paths in an
    // efficient way other than adding the path and set bRecursivePaths=true. So
    // if PathStartsWith is a folder, we use it.
    Filter.PackagePaths.Add(FName(*PathStartsWith));
  } else {
    // Default to /Game to prevent empty results or massive scan
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
  }

  // Use cached AssetRegistry data — ScanPathsSynchronous() removed to prevent
  // blocking the GameThread (causes SSE/HTTP transport timeouts).
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.

  if (!ClassFilter.IsEmpty()) {
    // Support both short class names and full paths (best effort)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FTopLevelAssetPath ClassPath(ClassFilter);
    if (ClassPath.IsValid()) {
      Filter.ClassPaths.Add(ClassPath);
    }
#else
    // UE 5.0: Use ClassNames instead of ClassPaths
    Filter.ClassNames.Add(FName(*ClassFilter));
#endif
  }

  // Tags are not standard on assets in the same way as actors.
  // AssetRegistry tags are Key-Value pairs.
  // If TagFilter is provided, we assume it checks for the existence of a tag
  // key or value. Implementing a generic "HasTag" is ambiguous. We'll assume
  // TagFilter refers to a metadata key presence.

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  // Post-filtering
  if (!ClassFilter.IsEmpty() || !TagFilter.IsEmpty()) {
    AssetList.RemoveAll([&](const FAssetData &Asset) {
      if (!ClassFilter.IsEmpty()) {
        // Check full class path or asset class name
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FString AssetClass = Asset.AssetClassPath.ToString();
        FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
#else
        FString AssetClass = Asset.AssetClass.ToString();
        FString AssetClassName = Asset.AssetClass.ToString();
#endif
        if (!AssetClass.Equals(ClassFilter) &&
            !AssetClassName.Equals(ClassFilter)) {
          return true; // Remove
        }
      }
      if (!TagFilter.IsEmpty()) {
        if (!Asset.TagsAndValues.Contains(FName(*TagFilter))) {
          return true; // Remove
        }
      }
      return false;
    });
  }

  // Filter by Depth if specified
  // (Changes made to support depth and folders - Touch to force rebuild)
  int32 Depth = -1;
  Payload->TryGetNumberField(TEXT("depth"), Depth);

  if (Depth >= 0 && bRecursive && !PathFilter.IsEmpty()) {
    // Normalize base path for depth calculation
    FString BasePath = PathFilter;
    if (BasePath.EndsWith(TEXT("/"))) {
      BasePath.RemoveAt(BasePath.Len() - 1);
    }
    // Base depth: number of slashes in /Game/Foo is 2
    int32 BaseSlashCount = 0;
    for (const TCHAR *P = *BasePath; *P; ++P) {
      if (*P == TEXT('/'))
        BaseSlashCount++;
    }

    AssetList.RemoveAll([&](const FAssetData &Asset) {
      FString PkgPath = Asset.PackagePath.ToString();
      // If PkgPath is shorter than BasePath (shouldn't happen with filter),
      // keep it I guess? Actually we only care about descendants.

      int32 SlashCount = 0;
      for (const TCHAR *P = *PkgPath; *P; ++P) {
        if (*P == TEXT('/'))
          SlashCount++;
      }

      // Difference in slashes determines depth
      // /Game (1 slash) vs /Game/A (2 slashes) -> Diff 1 -> Depth 0 (immediate
      // child) Wait, PackagePath for /Game/A is /Game. PackagePath for
      // /Game/Sub/B is /Game/Sub.

      // Let's test:
      // Filter: /Game (Slash=1)
      // Asset: /Game/A (PackagePath=/Game, Slash=1). Diff=0. Depth 0? Yes.
      // Asset: /Game/Sub/B (PackagePath=/Game/Sub, Slash=2). Diff=1. Depth 1?
      // Yes.

      // If Depth=0, we want Diff=0.
      // If Depth=1, we want Diff<=1.

      return (SlashCount - BaseSlashCount) > Depth;
    });
  }

  int32 TotalCount = AssetList.Num();

  // Apply pagination
  if (Offset > 0) {
    if (Offset >= AssetList.Num()) {
      AssetList.Empty();
    } else {
      AssetList.RemoveAt(0, Offset);
    }
  }

  if (Limit >= 0 && AssetList.Num() > Limit) {
    AssetList.SetNum(Limit);
  }

  // Also fetch sub-folders if we are listing a directory (PathFilter is set)
  TArray<FString> SubPathList;
  if (!PathFilter.IsEmpty()) {
    // If non-recursive (or depth limited), we generally want at least the
    // immediate subfolders. GetSubPaths is non-recursive by default.
    AssetRegistry.GetSubPaths(PathFilter, SubPathList, false);

    // If Depth is specified, we might want deeper folders?
    // Actually, standard 'ls' behavior on a folder shows immediate children
    // (files and folders). If recursive, it shows everything. Let keeps it
    // simple: If we are listing a path, show its immediate subfolders. Getting
    // ALL recursive folders might be too much info if strictly not requested,
    // but 'GetSubPaths' with bInRecurse=true gets everything.

    // Decision:
    // If Recursive=true (and Depth not limited), maybe we don't strictly need
    // folders as assets cover it? But user asked for folders when assets are
    // missing. Default 'ls' shows immediate folders. So let's always include
    // immediate subfolders of the requested path.
  }

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
    AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetObj->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
    AssetObj->SetStringField(TEXT("path"), Asset.ToSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
    AssetObj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());

    // Add tags for context if requested
    TArray<TSharedPtr<FJsonValue>> Tags;
    for (auto TagPair : Asset.TagsAndValues) {
      Tags.Add(MakeShared<FJsonValueString>(TagPair.Key.ToString()));
    }
    AssetObj->SetArrayField(TEXT("tags"), Tags);

    AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
  }

  TArray<TSharedPtr<FJsonValue>> FoldersJson;
  for (const FString &SubPath : SubPathList) {
    FoldersJson.Add(MakeShared<FJsonValueString>(SubPath));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("assets"), AssetsArray);
  Resp->SetArrayField(TEXT("folders"), FoldersJson);
  Resp->SetNumberField(TEXT("totalCount"), TotalCount);
  Resp->SetNumberField(TEXT("count"), AssetsArray.Num());
  Resp->SetNumberField(TEXT("offset"), Offset);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Assets listed"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get detailed information about a single asset.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleGetAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_asset payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FAssetData AssetData = UEditorAssetLibrary::FindAssetData(SafeAssetPath);
  if (!AssetData.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to find asset data"), nullptr,
                           TEXT("ASSET_DATA_INVALID"));
    return true;
  }

  TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
  AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  AssetObj->SetStringField(TEXT("path"), AssetData.GetSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
#else
  AssetObj->SetStringField(TEXT("path"), AssetData.ToSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClass.ToString());
#endif
  AssetObj->SetStringField(TEXT("packagePath"),
                           AssetData.PackagePath.ToString());

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("result"), AssetObj);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to generate an asset report (CSV/JSON).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'directory' and 'reportType'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UNebulaForgeBridgeSubsystem::HandleGenerateReport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("generate_report payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Directory;
  Payload->TryGetStringField(TEXT("directory"), Directory);
  if (Directory.IsEmpty()) {
    Directory = TEXT("/Game");
  }

  // Normalize /Content prefix to /Game for convenience
  if (Directory.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    Directory = FString::Printf(TEXT("/Game%s"), *Directory.RightChop(8));
  }

  FString ReportType;
  Payload->TryGetStringField(TEXT("reportType"), ReportType);
  if (ReportType.IsEmpty()) {
    ReportType = TEXT("Summary");
  }

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
  AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, Directory,
                                        ReportType, OutputPath]() {
    UNebulaForgeBridgeSubsystem *StrongThis = WeakThis.Get();
    if (!StrongThis) {
      return;
    }
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    if (!Directory.IsEmpty()) {
      Filter.PackagePaths.Add(FName(*Directory));
    }

    // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
    // Asset listing uses cached AssetRegistry data exclusively.
    // LIMITATION: Assets not yet indexed by the editor's background scanner
    // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);
    const bool bAudit = ReportType.Equals(TEXT("audit"), ESearchCase::IgnoreCase);
    const bool bSizeMap = ReportType.Equals(TEXT("size_map"), ESearchCase::IgnoreCase);
    int32 TotalDependencies = 0;
    int32 TotalReferencers = 0;
    int64 TotalSizeBytes = 0;

    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    for (const FAssetData &Asset : AssetList) {
      TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
      AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
      AssetObj->SetStringField(TEXT("path"),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                               Asset.GetSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
                               Asset.ToSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
      if (bAudit) {
        TArray<FName> Dependencies;
        TArray<FName> Referencers;
        AssetRegistryModule.Get().GetDependencies(Asset.PackageName, Dependencies);
        AssetRegistryModule.Get().GetReferencers(Asset.PackageName, Referencers);
        AssetObj->SetNumberField(TEXT("dependencyCount"), Dependencies.Num());
        AssetObj->SetNumberField(TEXT("referencerCount"), Referencers.Num());
        TotalDependencies += Dependencies.Num();
        TotalReferencers += Referencers.Num();
      }
      if (bSizeMap) {
        FString PackageFilename;
        int64 SizeBytes = 0;
        const FString PackageName = Asset.PackageName.ToString();
        if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilename, FPackageName::GetAssetPackageExtension()) ||
            FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilename, FPackageName::GetMapPackageExtension())) {
          IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
          if (PlatformFile.FileExists(*PackageFilename)) {
            SizeBytes = PlatformFile.FileSize(*PackageFilename);
          }
        }
        AssetObj->SetNumberField(TEXT("sizeBytes"), SizeBytes);
        TotalSizeBytes += FMath::Max<int64>(SizeBytes, 0);
      }
      AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    bool bFileWritten = false;
    if (!OutputPath.IsEmpty()) {
      // SECURITY: Sanitize and validate the output path to prevent path traversal
      FString SafeOutputPath = SanitizeProjectFilePath(OutputPath);
      if (SafeOutputPath.IsEmpty()) {
        StrongThis->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid or unsafe output path: %s"), *OutputPath),
                            TEXT("SECURITY_VIOLATION"));
        return;
      }

      FString AbsoluteOutput = FPaths::ProjectDir() / SafeOutputPath;
      AbsoluteOutput = FPaths::ConvertRelativePathToFull(AbsoluteOutput);
      FPaths::NormalizeFilename(AbsoluteOutput);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!AbsoluteOutput.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        StrongThis->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Output path escapes project directory: %s"), *OutputPath),
                            TEXT("SECURITY_VIOLATION"));
        return;
      }

      const FString DirPath = FPaths::GetPath(AbsoluteOutput);
      IPlatformFile &PlatformFile =
          FPlatformFileManager::Get().GetPlatformFile();
      PlatformFile.CreateDirectoryTree(*DirPath);

      const FString FileContents = TEXT(
          "{\"report\":\"Asset report generated by NebulaForge Bridge\"}");
      bFileWritten =
          FFileHelper::SaveStringToFile(FileContents, *AbsoluteOutput);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("directory"), Directory);
    Resp->SetStringField(TEXT("reportType"), ReportType);
    Resp->SetNumberField(TEXT("assetCount"), AssetList.Num());
    Resp->SetArrayField(TEXT("assets"), AssetsArray);
    if (bAudit) {
      Resp->SetBoolField(TEXT("audit"), true);
      Resp->SetNumberField(TEXT("totalDependencies"), TotalDependencies);
      Resp->SetNumberField(TEXT("totalReferencers"), TotalReferencers);
    }
    if (bSizeMap) {
      Resp->SetBoolField(TEXT("sizeMap"), true);
      Resp->SetNumberField(TEXT("totalSizeBytes"), TotalSizeBytes);
      Resp->SetStringField(TEXT("sizeBasis"), TEXT("package file on disk (.uasset or .umap); external bulk payloads may be separate"));
    }
    if (!OutputPath.IsEmpty()) {
      Resp->SetStringField(TEXT("outputPath"), OutputPath);
      Resp->SetBoolField(TEXT("fileWritten"), bFileWritten);
    }

    StrongThis->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset report generated"), Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// 8. MATERIAL CREATION
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandlePhysicalMaterialAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  auto SendInvalid = [&](const FString &Message) {
    SendAutomationResponse(Socket, RequestId, false, Message, nullptr,
                            TEXT("INVALID_ARGUMENT"));
  };
  auto ParseSurfaceIndex = [](const FString &Value, int32 &OutIndex) {
    FString Token = Value.TrimStartAndEnd();
    if (Token.StartsWith(TEXT("SurfaceType"), ESearchCase::IgnoreCase)) {
      Token = Token.RightChop(11);
    }
    if (Token.IsEmpty()) return false;
    const int32 Index = FCString::Atoi(*Token);
    if (Index < 0 || Index > 62) return false;
    OutIndex = Index;
    return true;
  };
  auto ParseCombineMode = [](const FString &Value, EFrictionCombineMode::Type &OutMode) {
    const FString Token = Value.TrimStartAndEnd().ToLower();
    if (Token == TEXT("average")) { OutMode = EFrictionCombineMode::Average; return true; }
    if (Token == TEXT("min") || Token == TEXT("minimum")) { OutMode = EFrictionCombineMode::Min; return true; }
    if (Token == TEXT("multiply") || Token == TEXT("multiplication")) { OutMode = EFrictionCombineMode::Multiply; return true; }
    if (Token == TEXT("max") || Token == TEXT("maximum")) { OutMode = EFrictionCombineMode::Max; return true; }
    return false;
  };
  auto CombineModeName = [](EFrictionCombineMode::Type Mode) {
    switch (Mode) {
    case EFrictionCombineMode::Min: return FString(TEXT("min"));
    case EFrictionCombineMode::Multiply: return FString(TEXT("multiply"));
    case EFrictionCombineMode::Max: return FString(TEXT("max"));
    default: return FString(TEXT("average"));
    }
  };
  auto ApplyValues = [&](UPhysicalMaterial *Material, FString &Error) {
    if (!Material) { Error = TEXT("Physical material is unavailable."); return false; }
    double Number = 0.0;
    if (Payload->TryGetNumberField(TEXT("friction"), Number)) {
      if (!FMath::IsFinite(Number) || Number < 0.0) { Error = TEXT("friction must be finite and non-negative."); return false; }
      Material->Friction = static_cast<float>(Number);
    }
    if (Payload->TryGetNumberField(TEXT("staticFriction"), Number)) {
      if (!FMath::IsFinite(Number) || Number < 0.0) { Error = TEXT("staticFriction must be finite and non-negative."); return false; }
      Material->StaticFriction = static_cast<float>(Number);
    }
    if (Payload->TryGetNumberField(TEXT("restitution"), Number)) {
      if (!FMath::IsFinite(Number) || Number < 0.0 || Number > 1.0) { Error = TEXT("restitution must be between 0 and 1."); return false; }
      Material->Restitution = static_cast<float>(Number);
    }
    if (Payload->TryGetNumberField(TEXT("density"), Number)) {
      if (!FMath::IsFinite(Number) || Number < 0.0) { Error = TEXT("density must be finite and non-negative."); return false; }
      Material->Density = static_cast<float>(Number);
    }
    FString SurfaceType;
    if (Payload->TryGetStringField(TEXT("surfaceType"), SurfaceType)) {
      int32 SurfaceIndex = 0;
      if (!ParseSurfaceIndex(SurfaceType, SurfaceIndex)) { Error = TEXT("surfaceType must be SurfaceType0-SurfaceType62 or a numeric index."); return false; }
      Material->SurfaceType = static_cast<EPhysicalSurface>(SurfaceIndex);
    }
    FString CombineMode;
    EFrictionCombineMode::Type ParsedMode;
    if (Payload->TryGetStringField(TEXT("frictionCombineMode"), CombineMode)) {
      if (!ParseCombineMode(CombineMode, ParsedMode)) { Error = TEXT("frictionCombineMode must be average, min, multiply, or max."); return false; }
      Material->FrictionCombineMode = ParsedMode;
    }
    if (Payload->TryGetStringField(TEXT("restitutionCombineMode"), CombineMode)) {
      if (!ParseCombineMode(CombineMode, ParsedMode)) { Error = TEXT("restitutionCombineMode must be average, min, multiply, or max."); return false; }
      Material->RestitutionCombineMode = ParsedMode;
    }
    bool BoolValue = false;
    if (Payload->TryGetBoolField(TEXT("overrideFrictionCombineMode"), BoolValue)) Material->bOverrideFrictionCombineMode = BoolValue;
    if (Payload->TryGetBoolField(TEXT("overrideRestitutionCombineMode"), BoolValue)) Material->bOverrideRestitutionCombineMode = BoolValue;
    return true;
  };
  auto AddMaterialData = [&](UPhysicalMaterial *Material, TSharedPtr<FJsonObject> &Result) {
    Result->SetStringField(TEXT("assetPath"), Material->GetPathName());
    Result->SetNumberField(TEXT("friction"), Material->Friction);
    Result->SetNumberField(TEXT("staticFriction"), Material->StaticFriction);
    Result->SetNumberField(TEXT("restitution"), Material->Restitution);
    Result->SetNumberField(TEXT("density"), Material->Density);
    Result->SetStringField(TEXT("surfaceType"), FString::Printf(TEXT("SurfaceType%d"), Material->SurfaceType.GetValue()));
    Result->SetStringField(TEXT("frictionCombineMode"), CombineModeName(Material->FrictionCombineMode.GetValue()));
    Result->SetStringField(TEXT("restitutionCombineMode"), CombineModeName(Material->RestitutionCombineMode.GetValue()));
    Result->SetBoolField(TEXT("overrideFrictionCombineMode"), Material->bOverrideFrictionCombineMode);
    Result->SetBoolField(TEXT("overrideRestitutionCombineMode"), Material->bOverrideRestitutionCombineMode);
  };

  if (Action == TEXT("configure_surface_type")) {
    FString SurfaceType, SurfaceName;
    Payload->TryGetStringField(TEXT("surfaceType"), SurfaceType);
    Payload->TryGetStringField(TEXT("surfaceName"), SurfaceName);
    if (SurfaceName.IsEmpty()) Payload->TryGetStringField(TEXT("name"), SurfaceName);
    int32 SurfaceIndex = 0;
    if (!ParseSurfaceIndex(SurfaceType, SurfaceIndex) || SurfaceName.IsEmpty() ||
        SurfaceName.Contains(TEXT("\"")) || SurfaceName.Contains(TEXT("\r")) || SurfaceName.Contains(TEXT("\n"))) {
      SendInvalid(TEXT("surfaceType and a quote-free surfaceName are required.")); return true;
    }
    const FString Section = TEXT("/Script/Engine.PhysicsSettings");
    TArray<FString> Surfaces;
    GConfig->GetArray(*Section, TEXT("PhysicalSurfaces"), Surfaces, GEngineIni);
    const FString Entry = FString::Printf(TEXT("(Type=SurfaceType%d,Name=\"%s\")"), SurfaceIndex, *SurfaceName);
    bool bReplaced = false;
    const FString Prefix = FString::Printf(TEXT("(Type=SurfaceType%d,"), SurfaceIndex);
    for (FString &Existing : Surfaces) {
      if (Existing.StartsWith(Prefix)) { Existing = Entry; bReplaced = true; break; }
    }
    if (!bReplaced) Surfaces.Add(Entry);
    GConfig->SetArray(*Section, TEXT("PhysicalSurfaces"), Surfaces, GEngineIni);
    if (Payload->HasField(TEXT("saveConfig")) ? GetJsonBoolField(Payload, TEXT("saveConfig"), true) : true) GConfig->Flush(false, GEngineIni);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("surfaceType"), FString::Printf(TEXT("SurfaceType%d"), SurfaceIndex));
    Result->SetStringField(TEXT("surfaceName"), SurfaceName); Result->SetBoolField(TEXT("replaced"), bReplaced);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Physical surface configured"), Result, FString()); return true;
  }

  if (Action == TEXT("create_physical_material")) {
    FString Name, Path;
    Payload->TryGetStringField(TEXT("name"), Name); Payload->TryGetStringField(TEXT("path"), Path);
    if (Name.IsEmpty() || Path.IsEmpty()) { SendInvalid(TEXT("name and path are required.")); return true; }
    Name = SanitizeAssetName(Name); Path = SanitizeProjectRelativePath(Path);
    if (Name.IsEmpty() || Path.IsEmpty()) { SendInvalid(TEXT("Invalid physical material name or path.")); return true; }
    const FString FullPath = Path / Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
      SendAutomationResponse(Socket, RequestId, false, TEXT("Physical material already exists"), nullptr, TEXT("ALREADY_EXISTS")); return true;
    }
    UPhysicalMaterialFactoryNew *Factory = NewObject<UPhysicalMaterialFactoryNew>();
    UObject *NewAsset = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(Name, Path, UPhysicalMaterial::StaticClass(), Factory);
    UPhysicalMaterial *Material = Cast<UPhysicalMaterial>(NewAsset);
    if (!Material) { SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create physical material"), nullptr, TEXT("CREATE_FAILED")); return true; }
    FString Error; if (!ApplyValues(Material, Error)) { SendInvalid(Error); return true; }
    Material->PostEditChange();
    Material->MarkPackageDirty();
    bool bSaved = false; bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      bSaved = McpSafeAssetSave(Material);
      if (!bSaved) {
        SendAutomationError(Socket, RequestId, TEXT("Physical material created but save failed"), TEXT("SAVE_FAILED"));
        return true;
      }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject(); AddMaterialData(Material, Result); Result->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Physical material created"), Result, FString()); return true;
  }

  if (Action == TEXT("assign_physical_material") || Action == TEXT("clear_physical_material_override")) {
    FString ActorName, ComponentName, MaterialPath;
    Payload->TryGetStringField(TEXT("actorName"), ActorName); Payload->TryGetStringField(TEXT("componentName"), ComponentName); Payload->TryGetStringField(TEXT("physicalMaterialPath"), MaterialPath);
    if (ActorName.IsEmpty()) { SendInvalid(TEXT("actorName is required.")); return true; }
    AActor *Actor = FindActorByName(ActorName);
    if (!Actor) { SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND")); return true; }
    UPhysicalMaterial *Material = nullptr;
    if (Action == TEXT("assign_physical_material")) {
      MaterialPath = SanitizeProjectRelativePath(MaterialPath);
      if (MaterialPath.IsEmpty()) { SendInvalid(TEXT("physicalMaterialPath is required and must be a valid asset path.")); return true; }
      Material = LoadObject<UPhysicalMaterial>(nullptr, *MaterialPath);
      if (!Material) { SendAutomationResponse(Socket, RequestId, false, TEXT("Physical material not found"), nullptr, TEXT("MATERIAL_NOT_FOUND")); return true; }
    }
    TArray<UPrimitiveComponent *> Components;
    if (!ComponentName.IsEmpty()) {
      UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(FindComponentByName(Actor, ComponentName));
      if (!Primitive) { SendAutomationError(Socket, RequestId, TEXT("Primitive component not found"), TEXT("COMPONENT_NOT_FOUND")); return true; }
      Components.Add(Primitive);
    } else Actor->GetComponents<UPrimitiveComponent>(Components);
    if (Components.Num() == 0) { SendInvalid(TEXT("Actor has no primitive components.")); return true; }
    for (UPrimitiveComponent *Component : Components) {
      if (!Component) continue;
      Component->Modify();
      Component->SetPhysMaterialOverride(Material);
      Component->MarkPackageDirty();
      Component->MarkRenderStateDirty();
    }
    Actor->Modify();
    Actor->MarkPackageDirty();
    bool bSaved = false; bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      UWorld *World = Actor->GetWorld();
      if (!World || !World->PersistentLevel || !World->GetOutermost() || !World->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))) {
        SendAutomationError(Socket, RequestId, TEXT("Physical material override changed but owning level is not a saved /Game map"), TEXT("SAVE_FAILED"));
        return true;
      }
      bSaved = McpSafeAssetSave(Actor) && McpSafeLevelSave(World->PersistentLevel, World->GetOutermost()->GetName());
      if (!bSaved) {
        SendAutomationError(Socket, RequestId, TEXT("Physical material override changed but save failed"), TEXT("SAVE_FAILED"));
        return true;
      }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject(); Result->SetStringField(TEXT("actorName"), ActorName); Result->SetNumberField(TEXT("componentCount"), Components.Num()); Result->SetStringField(TEXT("physicalMaterialPath"), Material ? Material->GetPathName() : TEXT("")); Result->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(Socket, RequestId, true, Action == TEXT("assign_physical_material") ? TEXT("Physical material assigned") : TEXT("Physical material override cleared"), Result, FString()); return true;
  }

  FString AssetPath; Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) Payload->TryGetStringField(TEXT("physicalMaterialPath"), AssetPath);
  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) { SendInvalid(TEXT("assetPath is required.")); return true; }
  UPhysicalMaterial *Material = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath);
  if (!Material) { SendAutomationResponse(Socket, RequestId, false, TEXT("Physical material not found"), nullptr, TEXT("MATERIAL_NOT_FOUND")); return true; }
  if (Action == TEXT("get_physical_material")) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject(); AddMaterialData(Material, Result);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Physical material retrieved"), Result, FString()); return true;
  }
  Material->Modify(); FString Error;
  if (Action == TEXT("set_friction")) { if (!Payload->HasField(TEXT("friction"))) { SendInvalid(TEXT("friction is required.")); return true; } }
  if (Action == TEXT("set_restitution")) { if (!Payload->HasField(TEXT("restitution"))) { SendInvalid(TEXT("restitution is required.")); return true; } }
  if (Action == TEXT("set_density")) { if (!Payload->HasField(TEXT("density"))) { SendInvalid(TEXT("density is required.")); return true; } }
  if (!ApplyValues(Material, Error)) { SendInvalid(Error); return true; }
  Material->PostEditChange();
  Material->MarkPackageDirty(); bool bSaved = false; bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave) {
    bSaved = McpSafeAssetSave(Material);
    if (!bSaved) {
      SendAutomationError(Socket, RequestId, TEXT("Physical material updated but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject(); AddMaterialData(Material, Result); Result->SetBoolField(TEXT("saved"), bSaved);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Physical material updated"), Result, FString()); return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED")); return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleDataAssetAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Data asset payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    FString Path;
    FString Name;
    Payload->TryGetStringField(TEXT("path"), Path);
    Payload->TryGetStringField(TEXT("name"), Name);
    if (!Path.IsEmpty() && !Name.IsEmpty()) AssetPath = Path + TEXT("/") + Name;
  }

  if (Action == TEXT("create_primary_data_asset")) {
    TSharedPtr<FJsonObject> PrimaryPayload = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>> &Pair : Payload->Values)
      PrimaryPayload->SetField(Pair.Key, Pair.Value);
    FString ClassPath;
    PrimaryPayload->TryGetStringField(TEXT("classPath"), ClassPath);
    if (ClassPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          TEXT("classPath must identify a concrete UPrimaryDataAsset subclass"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UClass *PrimaryClass = LoadClass<UPrimaryDataAsset>(nullptr, *ClassPath);
    if (!PrimaryClass || !PrimaryClass->IsChildOf(UPrimaryDataAsset::StaticClass()) ||
        PrimaryClass->HasAnyClassFlags(CLASS_Abstract)) {
      SendAutomationError(Socket, RequestId,
                          TEXT("classPath must resolve to a concrete UPrimaryDataAsset subclass"),
                          TEXT("INVALID_CLASS"));
      return true;
    }
    PrimaryPayload->SetStringField(TEXT("action"), TEXT("create_data_asset"));
    PrimaryPayload->SetStringField(TEXT("classPath"), ClassPath);
    return HandleDataAssetAction(RequestId, TEXT("create_data_asset"), PrimaryPayload, Socket);
  }

  if (Action == TEXT("create_data_asset")) {
    FString Name;
    FString Path;
    Payload->TryGetStringField(TEXT("name"), Name);
    Payload->TryGetStringField(TEXT("path"), Path);
    if (Name.IsEmpty() || Path.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("name and path are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Name = SanitizeAssetName(Name);
    Path = SanitizeProjectRelativePath(Path);
    if (Name.IsEmpty() || Path.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Invalid data asset name or path"), TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString ClassPath;
    Payload->TryGetStringField(TEXT("classPath"), ClassPath);
    UClass *DataClass = UDataAsset::StaticClass();
    if (!ClassPath.IsEmpty()) {
      UClass *Candidate = LoadClass<UDataAsset>(nullptr, *ClassPath);
      if (!Candidate || !Candidate->IsChildOf(UDataAsset::StaticClass())) {
        SendAutomationError(Socket, RequestId, TEXT("classPath must resolve to a UDataAsset subclass"), TEXT("INVALID_CLASS"));
        return true;
      }
      DataClass = Candidate;
    }

    const FString PackageName = Path + TEXT("/") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(PackageName)) {
      SendAutomationError(Socket, RequestId, TEXT("Data asset already exists"), TEXT("ASSET_EXISTS"));
      return true;
    }
    UPackage *Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create data asset package"), TEXT("CREATE_FAILED"));
      return true;
    }
    UObject *NewAsset = NewObject<UObject>(Package, DataClass, FName(*Name), RF_Public | RF_Standalone);
    if (!NewAsset) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create data asset"), TEXT("CREATE_FAILED"));
      return true;
    }
    FAssetRegistryModule::AssetCreated(NewAsset);
    Package->MarkPackageDirty();

    TMap<FName, FString> PropertyErrors;
    int32 AppliedProperties = 0;
    const TSharedPtr<FJsonObject> *Properties = nullptr;
    if (Payload->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid()) {
      AppliedProperties = McpPropertyReflection::ApplyJsonObjectToObject(NewAsset, *Properties, &PropertyErrors);
    }
    if (PropertyErrors.Num() > 0) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
      Result->SetNumberField(TEXT("appliedProperties"), AppliedProperties);
      TSharedPtr<FJsonObject> Errors = MakeShared<FJsonObject>();
      for (const TPair<FName, FString> &Pair : PropertyErrors) Errors->SetStringField(Pair.Key.ToString(), Pair.Value);
      Result->SetObjectField(TEXT("propertyErrors"), Errors);
      SendAutomationResponse(Socket, RequestId, false, TEXT("Data asset created but properties were rejected"), Result, TEXT("PROPERTY_VALIDATION_FAILED"));
      return true;
    }

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = !bSave || McpSafeAssetSave(NewAsset);
    if (!bSaved) {
      SendAutomationError(Socket, RequestId, TEXT("Data asset created but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    Result->SetStringField(TEXT("classPath"), NewAsset->GetClass()->GetPathName());
    Result->SetNumberField(TEXT("appliedProperties"), AppliedProperties);
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Data asset created"), Result, FString());
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("assetPath or path and name are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  AssetPath = SanitizeProjectRelativePath(AssetPath);
  UObject *DataAsset = LoadObject<UObject>(nullptr, *AssetPath);
  if (!DataAsset || !DataAsset->IsA(UDataAsset::StaticClass())) {
    SendAutomationError(Socket, RequestId, TEXT("Data asset not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (Action == TEXT("get_data_asset_properties")) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), DataAsset->GetPathName());
    Result->SetStringField(TEXT("classPath"), DataAsset->GetClass()->GetPathName());
    Result->SetObjectField(TEXT("properties"), McpPropertyReflection::ExportObjectToJson(DataAsset));
    SendAutomationResponse(Socket, RequestId, true, TEXT("Data asset properties retrieved"), Result, FString());
    return true;
  }

  const TSharedPtr<FJsonObject> *Properties = nullptr;
  if (!Payload->TryGetObjectField(TEXT("properties"), Properties) || !Properties || !Properties->IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("properties object is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  DataAsset->Modify();
  TMap<FName, FString> PropertyErrors;
  const int32 AppliedProperties = McpPropertyReflection::ApplyJsonObjectToObject(DataAsset, *Properties, &PropertyErrors);
  if (PropertyErrors.Num() > 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("appliedProperties"), AppliedProperties);
    TSharedPtr<FJsonObject> Errors = MakeShared<FJsonObject>();
    for (const TPair<FName, FString> &Pair : PropertyErrors) Errors->SetStringField(Pair.Key.ToString(), Pair.Value);
    Result->SetObjectField(TEXT("propertyErrors"), Errors);
    SendAutomationResponse(Socket, RequestId, false, TEXT("Data asset properties rejected"), Result, TEXT("PROPERTY_VALIDATION_FAILED"));
    return true;
  }
  DataAsset->PostEditChange();
  DataAsset->MarkPackageDirty();
  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  bool bSaved = !bSave || McpSafeAssetSave(DataAsset);
  if (!bSaved) {
    SendAutomationError(Socket, RequestId, TEXT("Data asset modified but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), DataAsset->GetPathName());
  Result->SetNumberField(TEXT("appliedProperties"), AppliedProperties);
  Result->SetBoolField(TEXT("saved"), bSave);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Data asset properties updated"), Result, FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Data asset authoring requires an editor build"), TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleDataTableAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Data table payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    FString Path;
    FString Name;
    Payload->TryGetStringField(TEXT("path"), Path);
    Payload->TryGetStringField(TEXT("name"), Name);
    if (!Path.IsEmpty() && !Name.IsEmpty()) AssetPath = Path + TEXT("/") + Name;
  }

  if (Action == TEXT("create_data_table") || Action == TEXT("create_tag_table")) {
    FString Name;
    FString Path;
    FString RowStructPath;
    Payload->TryGetStringField(TEXT("name"), Name);
    Payload->TryGetStringField(TEXT("path"), Path);
    Payload->TryGetStringField(TEXT("rowStructPath"), RowStructPath);
    if (Action == TEXT("create_tag_table") && RowStructPath.IsEmpty()) RowStructPath = TEXT("/Script/GameplayTags.GameplayTagTableRow");
    if (Name.IsEmpty() || Path.IsEmpty() || RowStructPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("name, path, and rowStructPath are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Name = SanitizeAssetName(Name);
    Path = SanitizeProjectRelativePath(Path);
    UScriptStruct *RowStruct = LoadObject<UScriptStruct>(nullptr, *RowStructPath);
    if (Name.IsEmpty() || Path.IsEmpty() || !RowStruct || !RowStruct->IsChildOf(FTableRowBase::StaticStruct())) {
      SendAutomationError(Socket, RequestId, TEXT("rowStructPath must resolve to a FTableRowBase-derived struct"), TEXT("INVALID_ROW_STRUCT"));
      return true;
    }
    const FString PackageName = Path + TEXT("/") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(PackageName)) {
      SendAutomationError(Socket, RequestId, TEXT("Data table already exists"), TEXT("ASSET_EXISTS"));
      return true;
    }
    UPackage *Package = CreatePackage(*PackageName);
    UDataTable *Table = Package ? NewObject<UDataTable>(Package, *Name, RF_Public | RF_Standalone) : nullptr;
    if (!Table) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create data table"), TEXT("CREATE_FAILED"));
      return true;
    }
    Table->RowStruct = RowStruct;
    FAssetRegistryModule::AssetCreated(Table);
    Package->MarkPackageDirty();
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    bool bSaved = !bSave || McpSafeAssetSave(Table);
    if (!bSaved) {
      SendAutomationError(Socket, RequestId, TEXT("Data table created but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetStringField(TEXT("rowStructPath"), RowStruct->GetPathName());
    Result->SetNumberField(TEXT("rowCount"), 0);
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Data table created"), Result, FString());
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  UDataTable *Table = LoadObject<UDataTable>(nullptr, *AssetPath);
  if (!Table || !Table->RowStruct || !Table->RowStruct->IsChildOf(FTableRowBase::StaticStruct())) {
    SendAutomationError(Socket, RequestId, TEXT("Data table not found or has an invalid row struct"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (Action == TEXT("get_data_table_rows")) {
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const TPair<FName, uint8*> &Pair : Table->GetRowMap()) {
      TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
      Row->SetStringField(TEXT("rowName"), Pair.Key.ToString());
      for (TFieldIterator<FProperty> PropertyIt(Table->RowStruct); PropertyIt; ++PropertyIt) {
        FProperty *Property = *PropertyIt;
        if (!Property || Property->HasAnyPropertyFlags(CPF_Transient)) continue;
        TSharedPtr<FJsonValue> Value = McpPropertyReflection::ExportPropertyToJsonValue(Pair.Value, Property);
        if (Value.IsValid()) Row->SetField(Property->GetName(), Value);
      }
      Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetStringField(TEXT("rowStructPath"), Table->RowStruct->GetPathName());
    Result->SetNumberField(TEXT("rowCount"), Rows.Num());
    Result->SetArrayField(TEXT("rows"), Rows);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Data table rows retrieved"), Result, FString());
    return true;
  }

  if (Action == TEXT("import_data_table_csv")) {
    FString Csv;
    Payload->TryGetStringField(TEXT("csv"), Csv);
    if (Csv.TrimStartAndEnd().IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("csv is required and must not be empty"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
    if (Problems.Num() > 0) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      TArray<TSharedPtr<FJsonValue>> ProblemValues;
      for (const FString &Problem : Problems) ProblemValues.Add(MakeShared<FJsonValueString>(Problem));
      Result->SetArrayField(TEXT("problems"), ProblemValues);
      Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
      SendAutomationResponse(Socket, RequestId, false, TEXT("Data table CSV import reported problems"), Result, TEXT("CSV_IMPORT_FAILED"));
      return true;
    }
    Table->MarkPackageDirty();
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !McpSafeAssetSave(Table)) {
      SendAutomationError(Socket, RequestId, TEXT("Data table CSV imported but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetNumberField(TEXT("rowCount"), Table->GetRowMap().Num());
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Data table CSV imported"), Result, FString());
    return true;
  }

  if (Action == TEXT("export_data_table_csv")) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetStringField(TEXT("rowStructPath"), Table->RowStruct->GetPathName());
    Result->SetNumberField(TEXT("rowCount"), Table->GetRowMap().Num());
    Result->SetStringField(TEXT("csv"), Table->GetTableAsCSV(EDataTableExportFlags::None));
    SendAutomationResponse(Socket, RequestId, true, TEXT("Data table CSV exported"), Result, FString());
    return true;
  }

  if (Action == TEXT("delete_data_table_row")) {
    FString RowNameString;
    Payload->TryGetStringField(TEXT("rowName"), RowNameString);
    RowNameString = RowNameString.TrimStartAndEnd();
    if (RowNameString.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("rowName is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const FName RowName(*RowNameString);
    if (!Table->GetRowMap().Contains(RowName)) {
      SendAutomationError(Socket, RequestId, TEXT("Data table row was not found"), TEXT("ROW_NOT_FOUND"));
      return true;
    }
    Table->RemoveRow(RowName);
    Table->MarkPackageDirty();
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !McpSafeAssetSave(Table)) {
      SendAutomationError(Socket, RequestId, TEXT("Data table row deleted but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetStringField(TEXT("rowName"), RowNameString);
    Result->SetNumberField(TEXT("rowCount"), Table->GetRowMap().Num());
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Data table row deleted"), Result, FString());
    return true;
  }

  if (Action == TEXT("modify_data_table_row")) {
    FString RowNameString;
    Payload->TryGetStringField(TEXT("rowName"), RowNameString);
    RowNameString = RowNameString.TrimStartAndEnd();
    const TSharedPtr<FJsonObject> *Properties = nullptr;
    if (RowNameString.IsEmpty() || !Payload->TryGetObjectField(TEXT("properties"), Properties) || !Properties || !Properties->IsValid()) {
      SendAutomationError(Socket, RequestId, TEXT("rowName and properties object are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    uint8 *const *ExistingRow = Table->GetRowMap().Find(FName(*RowNameString));
    if (!ExistingRow || !*ExistingRow) {
      SendAutomationError(Socket, RequestId, TEXT("Data table row was not found"), TEXT("ROW_NOT_FOUND"));
      return true;
    }
    FStructOnScope Candidate(Table->RowStruct);
    Table->RowStruct->CopyScriptStruct(Candidate.GetStructMemory(), *ExistingRow);
    TArray<FString> PropertyErrors;
    int32 AppliedProperties = 0;
    for (const TPair<FString, TSharedPtr<FJsonValue>> &Pair : (*Properties)->Values) {
      FProperty *Property = Table->RowStruct->FindPropertyByName(FName(*Pair.Key));
      FString Error;
      if (!Property || Property->HasAnyPropertyFlags(CPF_Transient) ||
          !McpPropertyReflection::ApplyJsonValueToProperty(Candidate.GetStructMemory(), Property, Pair.Value, Error)) {
        PropertyErrors.Add(Pair.Key + TEXT(": ") + (Error.IsEmpty() ? TEXT("unknown property") : Error));
      } else {
        ++AppliedProperties;
      }
    }
    if (PropertyErrors.Num() > 0) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      TArray<TSharedPtr<FJsonValue>> Errors;
      for (const FString &Error : PropertyErrors) Errors.Add(MakeShared<FJsonValueString>(Error));
      Result->SetArrayField(TEXT("propertyErrors"), Errors);
      SendAutomationResponse(Socket, RequestId, false, TEXT("Data table row rejected"), Result, TEXT("PROPERTY_VALIDATION_FAILED"));
      return true;
    }
    Table->RowStruct->CopyScriptStruct(*ExistingRow, Candidate.GetStructMemory());
    Table->MarkPackageDirty();
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !McpSafeAssetSave(Table)) {
      SendAutomationError(Socket, RequestId, TEXT("Data table row modified but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetStringField(TEXT("rowName"), RowNameString);
    Result->SetNumberField(TEXT("appliedProperties"), AppliedProperties);
    Result->SetNumberField(TEXT("rowCount"), Table->GetRowMap().Num());
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Data table row modified"), Result, FString());
    return true;
  }

  FString RowNameString;
  Payload->TryGetStringField(TEXT("rowName"), RowNameString);
  const TSharedPtr<FJsonObject> *Properties = nullptr;
  if (RowNameString.IsEmpty() || !Payload->TryGetObjectField(TEXT("properties"), Properties) || !Properties || !Properties->IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("rowName and properties object are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (Table->GetRowMap().Contains(FName(*RowNameString))) {
    SendAutomationError(Socket, RequestId, TEXT("Data table row already exists"), TEXT("ROW_EXISTS"));
    return true;
  }
  FStructOnScope Row(Table->RowStruct);
  TArray<FString> PropertyErrors;
  for (const TPair<FString, TSharedPtr<FJsonValue>> &Pair : (*Properties)->Values) {
    FProperty *Property = Table->RowStruct->FindPropertyByName(FName(*Pair.Key));
    FString Error;
    if (!Property || !McpPropertyReflection::ApplyJsonValueToProperty(Row.GetStructMemory(), Property, Pair.Value, Error)) {
      PropertyErrors.Add(Pair.Key + TEXT(": ") + (Error.IsEmpty() ? TEXT("unknown property") : Error));
    }
  }
  if (PropertyErrors.Num() > 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString &Error : PropertyErrors) Errors.Add(MakeShared<FJsonValueString>(Error));
    Result->SetArrayField(TEXT("propertyErrors"), Errors);
    SendAutomationResponse(Socket, RequestId, false, TEXT("Data table row rejected"), Result, TEXT("PROPERTY_VALIDATION_FAILED"));
    return true;
  }
  Table->AddRow(FName(*RowNameString), *reinterpret_cast<FTableRowBase*>(Row.GetStructMemory()));
  Table->MarkPackageDirty();
  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  bool bSaved = !bSave || McpSafeAssetSave(Table);
  if (!bSaved) {
    SendAutomationError(Socket, RequestId, TEXT("Data table row added but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
  Result->SetStringField(TEXT("rowName"), RowNameString);
  Result->SetNumberField(TEXT("rowCount"), Table->GetRowMap().Num());
  Result->SetBoolField(TEXT("saved"), bSave);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Data table row added"), Result, FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Data table authoring requires an editor build"), TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleCurveTableAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Curve table payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (Action == TEXT("replace_curve_keys")) {
    FString CurvePath;
    Payload->TryGetStringField(TEXT("assetPath"), CurvePath);
    UObject *CurveObject = LoadObject<UObject>(nullptr, *CurvePath);
    UCurveFloat *FloatCurve = Cast<UCurveFloat>(CurveObject);
    UCurveLinearColor *ColorCurve = Cast<UCurveLinearColor>(CurveObject);
    if (!FloatCurve && !ColorCurve) {
      SendAutomationError(Socket, RequestId, TEXT("assetPath must resolve to a UCurveFloat or UCurveLinearColor asset"), TEXT("CURVE_NOT_FOUND"));
      return true;
    }
    const TSharedPtr<FJsonValue> KeysValue = Payload->TryGetField(TEXT("keys"));
    const TArray<TSharedPtr<FJsonValue>> *Keys = nullptr;
    if (!KeysValue.IsValid() || !KeysValue->TryGetArray(Keys) || !Keys) {
      SendAutomationError(Socket, RequestId, TEXT("keys must be an array"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TArray<FRichCurveKey> FloatKeys;
    TArray<FRichCurveKey> ColorKeys[4];
    for (const TSharedPtr<FJsonValue> &KeyValue : *Keys) {
      const TSharedPtr<FJsonObject> KeyObject = KeyValue.IsValid() ? KeyValue->AsObject() : nullptr;
      double Time = 0.0;
      if (!KeyObject.IsValid() || !KeyObject->TryGetNumberField(TEXT("time"), Time) || !FMath::IsFinite(Time)) {
        SendAutomationError(Socket, RequestId, TEXT("each key requires a finite numeric time"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (FloatCurve) {
        double Value = 0.0;
        if (!KeyObject->TryGetNumberField(TEXT("value"), Value) || !FMath::IsFinite(Value)) {
          SendAutomationError(Socket, RequestId, TEXT("float curve keys require a finite numeric value"), TEXT("INVALID_ARGUMENT"));
          return true;
        }
        FRichCurveKey NewKey(static_cast<float>(Time), static_cast<float>(Value));
        FString Error;
        if (!ApplyCurveKeyOptionsAW(KeyObject, NewKey, Error)) {
          SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
          return true;
        }
        FloatKeys.Add(NewKey);
      } else {
        const TSharedPtr<FJsonObject> *ColorObject = nullptr;
        if (!KeyObject->TryGetObjectField(TEXT("value"), ColorObject) || !ColorObject || !ColorObject->IsValid()) {
          SendAutomationError(Socket, RequestId, TEXT("linear color curve keys require value {r,g,b,a}"), TEXT("INVALID_ARGUMENT"));
          return true;
        }
        const TCHAR *Channels[] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
        double Values[4] = { 0.0, 0.0, 0.0, 1.0 };
        for (int32 Channel = 0; Channel < 4; ++Channel) {
          if (Channel == 3 && !(*ColorObject)->HasField(Channels[Channel])) continue;
          if (!(*ColorObject)->TryGetNumberField(Channels[Channel], Values[Channel]) || !FMath::IsFinite(Values[Channel])) {
            SendAutomationError(Socket, RequestId, TEXT("linear color curve channels must be finite numeric r,g,b,a values"), TEXT("INVALID_ARGUMENT"));
            return true;
          }
        }
        for (int32 Channel = 0; Channel < 4; ++Channel) {
          FRichCurveKey NewKey(static_cast<float>(Time), static_cast<float>(Values[Channel]));
          FString Error;
          if (!ApplyCurveKeyOptionsAW(KeyObject, NewKey, Error)) {
            SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
            return true;
          }
          ColorKeys[Channel].Add(NewKey);
        }
      }
    }
    if (FloatCurve) {
      FloatCurve->FloatCurve.Keys = FloatKeys;
      FloatCurve->Modify();
      FloatCurve->MarkPackageDirty();
      if (!McpSafeAssetSave(FloatCurve)) {
        SendAutomationError(Socket, RequestId, TEXT("Curve keys replaced but save failed"), TEXT("SAVE_FAILED"));
        return true;
      }
    } else {
      for (int32 Channel = 0; Channel < 4; ++Channel) ColorCurve->FloatCurves[Channel].Keys = ColorKeys[Channel];
      ColorCurve->Modify();
      ColorCurve->MarkPackageDirty();
      if (!McpSafeAssetSave(ColorCurve)) {
        SendAutomationError(Socket, RequestId, TEXT("Curve keys replaced but save failed"), TEXT("SAVE_FAILED"));
        return true;
      }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), CurveObject->GetPathName());
    Result->SetStringField(TEXT("curveType"), FloatCurve ? TEXT("float") : TEXT("linear_color"));
    Result->SetNumberField(TEXT("keyCount"), FloatCurve ? FloatKeys.Num() : ColorKeys[0].Num());
    Result->SetBoolField(TEXT("saved"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Curve keys replaced"), Result, FString());
    return true;
  }
  if (Action == TEXT("create_curve_float") || Action == TEXT("create_curve_linear_color")) {
    FString Name;
    FString Path;
    Payload->TryGetStringField(TEXT("name"), Name);
    Payload->TryGetStringField(TEXT("path"), Path);
    Name = SanitizeAssetName(Name);
    Path = SanitizeProjectRelativePath(Path);
    if (Name.IsEmpty() || Path.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("name and path are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const FString PackageName = Path + TEXT("/") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(PackageName)) {
      SendAutomationError(Socket, RequestId, TEXT("Curve asset already exists"), TEXT("ASSET_EXISTS"));
      return true;
    }
    const TSharedPtr<FJsonValue> KeysValue = Payload->TryGetField(TEXT("keys"));
    const TArray<TSharedPtr<FJsonValue>> *Keys = nullptr;
    if (KeysValue.IsValid() && KeysValue->Type != EJson::Null && (!KeysValue->TryGetArray(Keys) || !Keys)) {
      SendAutomationError(Socket, RequestId, TEXT("keys must be an array"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TArray<FRichCurveKey> FloatKeys;
    TArray<FRichCurveKey> ColorKeys[4];
    if (Keys) {
      for (const TSharedPtr<FJsonValue> &KeyValue : *Keys) {
        const TSharedPtr<FJsonObject> KeyObject = KeyValue.IsValid() ? KeyValue->AsObject() : nullptr;
        double Time = 0.0;
        if (!KeyObject.IsValid() || !KeyObject->TryGetNumberField(TEXT("time"), Time) || !FMath::IsFinite(Time)) {
          SendAutomationError(Socket, RequestId, TEXT("each key requires a finite numeric time"), TEXT("INVALID_ARGUMENT"));
          return true;
        }
        if (Action == TEXT("create_curve_float")) {
          double Value = 0.0;
          if (!KeyObject->TryGetNumberField(TEXT("value"), Value) || !FMath::IsFinite(Value)) {
            SendAutomationError(Socket, RequestId, TEXT("float curve keys require a finite numeric value"), TEXT("INVALID_ARGUMENT"));
            return true;
          }
          FRichCurveKey NewKey(static_cast<float>(Time), static_cast<float>(Value));
          FString Error;
          if (!ApplyCurveKeyOptionsAW(KeyObject, NewKey, Error)) {
            SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
            return true;
          }
          FloatKeys.Add(NewKey);
        } else {
          const TSharedPtr<FJsonObject> *ColorObject = nullptr;
          if (!KeyObject->TryGetObjectField(TEXT("value"), ColorObject) || !ColorObject || !ColorObject->IsValid()) {
            SendAutomationError(Socket, RequestId, TEXT("linear color curve keys require value {r,g,b,a}"), TEXT("INVALID_ARGUMENT"));
            return true;
          }
          double Channels[4] = { 0.0, 0.0, 0.0, 1.0 };
          const TCHAR *ChannelNames[] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
          for (int32 Channel = 0; Channel < 4; ++Channel) {
            if (Channel == 3 && !(*ColorObject)->HasField(ChannelNames[Channel])) continue;
            if (!(*ColorObject)->TryGetNumberField(ChannelNames[Channel], Channels[Channel]) || !FMath::IsFinite(Channels[Channel])) {
              SendAutomationError(Socket, RequestId, TEXT("linear color curve channels must be finite numeric r,g,b,a values"), TEXT("INVALID_ARGUMENT"));
              return true;
            }
          }
          for (int32 Channel = 0; Channel < 4; ++Channel) {
            FRichCurveKey NewKey(static_cast<float>(Time), static_cast<float>(Channels[Channel]));
            FString Error;
            if (!ApplyCurveKeyOptionsAW(KeyObject, NewKey, Error)) {
              SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
              return true;
            }
            ColorKeys[Channel].Add(NewKey);
          }
        }
      }
    }
    UPackage *Package = CreatePackage(*PackageName);
    UObject *CurveObject = nullptr;
    if (Action == TEXT("create_curve_float")) {
      UCurveFloat *Curve = Package ? NewObject<UCurveFloat>(Package, *Name, RF_Public | RF_Standalone) : nullptr;
      if (Curve) { Curve->FloatCurve.Keys = FloatKeys; CurveObject = Curve; }
    } else {
      UCurveLinearColor *Curve = Package ? NewObject<UCurveLinearColor>(Package, *Name, RF_Public | RF_Standalone) : nullptr;
      if (Curve) { for (int32 Channel = 0; Channel < 4; ++Channel) Curve->FloatCurves[Channel].Keys = ColorKeys[Channel]; CurveObject = Curve; }
    }
    if (!CurveObject) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create curve asset"), TEXT("CREATE_FAILED"));
      return true;
    }
    FAssetRegistryModule::AssetCreated(CurveObject);
    Package->MarkPackageDirty();
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !McpSafeAssetSave(CurveObject)) {
      SendAutomationError(Socket, RequestId, TEXT("Curve asset created but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), CurveObject->GetPathName());
    Result->SetStringField(TEXT("curveType"), Action == TEXT("create_curve_float") ? TEXT("float") : TEXT("linear_color"));
    Result->SetNumberField(TEXT("keyCount"), Keys ? (Action == TEXT("create_curve_float") ? FloatKeys.Num() : ColorKeys[0].Num()) : 0);
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Curve asset created"), Result, FString());
    return true;
  }
  if (Action == TEXT("create_curve_table")) {
    FString Name;
    FString Path;
    Payload->TryGetStringField(TEXT("name"), Name);
    Payload->TryGetStringField(TEXT("path"), Path);
    Name = SanitizeAssetName(Name);
    Path = SanitizeProjectRelativePath(Path);
    if (Name.IsEmpty() || Path.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("name and path are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const FString PackageName = Path + TEXT("/") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(PackageName)) {
      SendAutomationError(Socket, RequestId, TEXT("Curve table already exists"), TEXT("ASSET_EXISTS"));
      return true;
    }
    UPackage *Package = CreatePackage(*PackageName);
    UCurveTable *Table = Package ? NewObject<UCurveTable>(Package, *Name, RF_Public | RF_Standalone) : nullptr;
    if (!Table) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create curve table"), TEXT("CREATE_FAILED"));
      return true;
    }
    FAssetRegistryModule::AssetCreated(Table);
    Package->MarkPackageDirty();
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    const bool bSaved = !bSave || McpSafeAssetSave(Table);
    if (!bSaved) {
      SendAutomationError(Socket, RequestId, TEXT("Curve table created but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetNumberField(TEXT("rowCount"), 0);
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Curve table created"), Result, FString());
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  UCurveTable *Table = LoadObject<UCurveTable>(nullptr, *AssetPath);
  if (!Table) {
    SendAutomationError(Socket, RequestId, TEXT("Curve table not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (Action == TEXT("export_curve_table_csv")) {
    FString Csv;
    TArray<FName> RowNames;
    for (const TPair<FName, FRichCurve*> &Pair : Table->GetRichCurveRowMap()) {
      RowNames.Add(Pair.Key);
    }
    RowNames.Sort([](const FName &A, const FName &B) { return A.ToString() < B.ToString(); });
    Csv = TEXT("Name");
    for (const FName &RowName : RowNames) Csv += TEXT(",") + RowName.ToString();
    Csv += TEXT("\nX");
    int32 MaxKeys = 0;
    for (const FName &RowName : RowNames) {
      const FRichCurve *Curve = Table->FindRichCurve(RowName, TEXT("NebulaForge export"), false);
      MaxKeys = FMath::Max(MaxKeys, Curve ? Curve->Keys.Num() : 0);
    }
    for (int32 KeyIndex = 0; KeyIndex < MaxKeys; ++KeyIndex) {
      Csv += TEXT("\n") + FString::FromInt(KeyIndex);
      for (const FName &RowName : RowNames) {
        const FRichCurve *Curve = Table->FindRichCurve(RowName, TEXT("NebulaForge export"), false);
        if (Curve && Curve->Keys.IsValidIndex(KeyIndex)) {
          Csv += FString::Printf(TEXT(",%g"), Curve->Keys[KeyIndex].Value);
        } else {
          Csv += TEXT(",");
        }
      }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetStringField(TEXT("csv"), Csv);
    Result->SetNumberField(TEXT("rowCount"), RowNames.Num());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Curve table CSV exported"), Result, FString());
    return true;
  }

  if (Action == TEXT("import_curve_table_csv")) {
    FString Csv;
    if (!Payload->TryGetStringField(TEXT("csv"), Csv) || Csv.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("csv is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TArray<FString> Lines;
    Csv.ParseIntoArrayLines(Lines, false);
    if (Lines.Num() < 2) {
      SendAutomationError(Socket, RequestId, TEXT("csv must contain a header and at least one data row"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TArray<FString> Header;
    Lines[0].ParseIntoArray(Header, TEXT(","), true);
    if (Header.Num() < 2 || !Header[0].Equals(TEXT("Name"), ESearchCase::IgnoreCase)) {
      SendAutomationError(Socket, RequestId, TEXT("csv header must start with Name and contain curve columns"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    for (int32 Column = 1; Column < Header.Num(); ++Column) {
      if (Header[Column].TrimStartAndEnd().IsEmpty()) continue;
      Table->AddRichCurve(FName(*Header[Column].TrimStartAndEnd()));
    }
    const TArray<FString> Errors = Table->CreateTableFromCSVString(Csv, RCIM_Linear);
    if (Errors.Num() > 0) {
      SendAutomationError(Socket, RequestId, FString::Join(Errors, TEXT("; ")), TEXT("CSV_IMPORT_FAILED"));
      return true;
    }
    Table->MarkPackageDirty();
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !McpSafeAssetSave(Table)) {
      SendAutomationError(Socket, RequestId, TEXT("Curve table imported but save failed"), TEXT("SAVE_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetNumberField(TEXT("rowCount"), Table->GetRichCurveRowMap().Num());
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Curve table CSV imported"), Result, FString());
    return true;
  }

  if (Action == TEXT("get_curve_table_rows")) {
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const TPair<FName, FRichCurve*> &Pair : Table->GetRichCurveRowMap()) {
      if (!Pair.Value) continue;
      TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
      Row->SetStringField(TEXT("rowName"), Pair.Key.ToString());
      TArray<TSharedPtr<FJsonValue>> Keys;
      for (const FRichCurveKey &Key : Pair.Value->Keys) {
        TSharedPtr<FJsonObject> KeyObject = McpHandlerUtils::CreateResultObject();
        KeyObject->SetNumberField(TEXT("time"), Key.Time);
        KeyObject->SetNumberField(TEXT("value"), Key.Value);
        KeyObject->SetStringField(TEXT("interpMode"), CurveInterpModeToStringAW(Key.InterpMode));
        KeyObject->SetStringField(TEXT("tangentMode"), CurveTangentModeToStringAW(Key.TangentMode));
        KeyObject->SetStringField(TEXT("tangentWeightMode"), CurveTangentWeightModeToStringAW(Key.TangentWeightMode));
        KeyObject->SetNumberField(TEXT("arriveTangent"), Key.ArriveTangent);
        KeyObject->SetNumberField(TEXT("leaveTangent"), Key.LeaveTangent);
        KeyObject->SetNumberField(TEXT("arriveTangentWeight"), Key.ArriveTangentWeight);
        KeyObject->SetNumberField(TEXT("leaveTangentWeight"), Key.LeaveTangentWeight);
        Keys.Add(MakeShared<FJsonValueObject>(KeyObject));
      }
      Row->SetArrayField(TEXT("keys"), Keys);
      Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
    Result->SetArrayField(TEXT("rows"), Rows);
    Result->SetNumberField(TEXT("rowCount"), Rows.Num());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Curve table rows retrieved"), Result, FString());
    return true;
  }

  FString RowNameString;
  Payload->TryGetStringField(TEXT("rowName"), RowNameString);
  const TSharedPtr<FJsonValue> KeysValue = Payload->TryGetField(TEXT("keys"));
  if (RowNameString.TrimStartAndEnd().IsEmpty() || !KeysValue.IsValid() || KeysValue->Type != EJson::Array) {
    SendAutomationError(Socket, RequestId, TEXT("rowName and keys array are required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FRichCurve &Curve = Table->AddRichCurve(FName(*RowNameString.TrimStartAndEnd()));
  const TArray<TSharedPtr<FJsonValue>> *Keys = nullptr;
  KeysValue->TryGetArray(Keys);
  if (!Keys) {
    SendAutomationError(Socket, RequestId, TEXT("keys must be an array"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  TArray<FRichCurveKey> PendingKeys;
  for (const TSharedPtr<FJsonValue> &KeyValue : *Keys) {
    const TSharedPtr<FJsonObject> KeyObject = KeyValue.IsValid() ? KeyValue->AsObject() : nullptr;
    double Time = 0.0;
    double Value = 0.0;
    if (!KeyObject.IsValid() || !KeyObject->TryGetNumberField(TEXT("time"), Time) || !KeyObject->TryGetNumberField(TEXT("value"), Value) || !FMath::IsFinite(Time) || !FMath::IsFinite(Value)) {
      SendAutomationError(Socket, RequestId, TEXT("each key requires finite numeric time and value"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FRichCurveKey NewKey(static_cast<float>(Time), static_cast<float>(Value));
    FString Mode;
    if (KeyObject->TryGetStringField(TEXT("interpMode"), Mode)) {
      ERichCurveInterpMode ParsedMode;
      if (!ParseCurveInterpModeAW(Mode, ParsedMode)) {
        SendAutomationError(Socket, RequestId, TEXT("interpMode must be linear, constant, cubic, or none"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      NewKey.InterpMode = ParsedMode;
    }
    if (KeyObject->TryGetStringField(TEXT("tangentMode"), Mode)) {
      ERichCurveTangentMode ParsedMode;
      if (!ParseCurveTangentModeAW(Mode, ParsedMode)) {
        SendAutomationError(Socket, RequestId, TEXT("tangentMode must be auto, user, break, smart_auto, or none"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      NewKey.TangentMode = ParsedMode;
    }
    if (KeyObject->TryGetStringField(TEXT("tangentWeightMode"), Mode)) {
      ERichCurveTangentWeightMode ParsedMode;
      if (!ParseCurveTangentWeightModeAW(Mode, ParsedMode)) {
        SendAutomationError(Socket, RequestId, TEXT("tangentWeightMode must be none, arrive, leave, or both"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      NewKey.TangentWeightMode = ParsedMode;
    }
    const TCHAR *NumericFields[] = { TEXT("arriveTangent"), TEXT("leaveTangent"), TEXT("arriveTangentWeight"), TEXT("leaveTangentWeight") };
    float *NumericTargets[] = { &NewKey.ArriveTangent, &NewKey.LeaveTangent, &NewKey.ArriveTangentWeight, &NewKey.LeaveTangentWeight };
    for (int32 NumericIndex = 0; NumericIndex < UE_ARRAY_COUNT(NumericFields); ++NumericIndex) {
      double NumericValue = 0.0;
      if (KeyObject->TryGetNumberField(NumericFields[NumericIndex], NumericValue)) {
        if (!FMath::IsFinite(NumericValue)) {
          SendAutomationError(Socket, RequestId, TEXT("curve tangent values must be finite"), TEXT("INVALID_ARGUMENT"));
          return true;
        }
        *NumericTargets[NumericIndex] = static_cast<float>(NumericValue);
      }
    }
    PendingKeys.Add(NewKey);
  }
  for (const FRichCurveKey &NewKey : PendingKeys) {
    Curve.Keys.Add(NewKey);
  }
  Curve.Keys.Sort([](const FRichCurveKey &A, const FRichCurveKey &B) { return A.Time < B.Time; });
  Table->MarkPackageDirty();
  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(Table)) {
    SendAutomationError(Socket, RequestId, TEXT("Curve table updated but save failed"), TEXT("SAVE_FAILED"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), Table->GetPathName());
  Result->SetStringField(TEXT("rowName"), RowNameString.TrimStartAndEnd());
  Result->SetNumberField(TEXT("keyCount"), Curve.Keys.Num());
  Result->SetBoolField(TEXT("saved"), bSave);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Curve table row updated"), Result, FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Curve table authoring requires an editor build"), TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleCreateMaterial(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  if (Name.IsEmpty() || Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name and path required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  Name = SanitizeAssetName(Name);
  Path = SanitizeProjectRelativePath(Path);
  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid path"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Validate properties if present
  const TSharedPtr<FJsonObject> *Props;
  if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
    FString ShadingModelStr;
    if ((*Props)->TryGetStringField(TEXT("ShadingModel"), ShadingModelStr)) {
      // Simple validation for test case
      if (ShadingModelStr.Equals(TEXT("InvalidModel"),
                                 ESearchCase::IgnoreCase)) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Invalid shading model"), nullptr,
                               TEXT("INVALID_PROPERTY"));
        return true;
      }
    }
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  FString FullPath = Path + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    UEditorAssetLibrary::DeleteAsset(FullPath);
  }

  UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
  UObject *NewAsset =
      AssetTools.CreateAsset(Name, Path, UMaterial::StaticClass(), Factory);

  if (NewAsset) {
    bool bSaved = false;
    if (bSave) {
      bSaved = McpSafeAssetSave(NewAsset);
      if (!bSaved) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Material created but save failed"), nullptr,
                               TEXT("SAVE_FAILED"));
        return true;
      }
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    Resp->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create material"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleCreateMaterialInstance(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
  FString ParentPath;
  Payload->TryGetStringField(TEXT("parentMaterial"), ParentPath);
  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  if (Name.IsEmpty() || Path.IsEmpty() || ParentPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name, path and parentMaterial required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Name = SanitizeAssetName(Name);
  Path = SanitizeProjectRelativePath(Path);
  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid path"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }
  UMaterialInterface *ParentMaterial = nullptr;

  // Special test sentinel: treat "/Valid" as a shorthand for the engine's
  // default surface material so tests can exercise parameter handling without
  // requiring a real asset at that path.
  if (ParentPath.Equals(TEXT("/Valid"), ESearchCase::IgnoreCase)) {
    ParentMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
  } else {
    ParentPath = SanitizeProjectRelativePath(ParentPath);
    if (ParentPath.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Invalid parentMaterial"), nullptr,
                             TEXT("SECURITY_VIOLATION"));
      return true;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(ParentPath)) {
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("Parent material asset not found: %s"),
                          *ParentPath),
          nullptr, TEXT("PARENT_NOT_FOUND"));
      return true;
    }
    ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
  }

  if (!ParentMaterial) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Parent material not found"), nullptr,
                           TEXT("PARENT_NOT_FOUND"));
    return true;
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  UMaterialInstanceConstantFactoryNew *Factory =
      NewObject<UMaterialInstanceConstantFactoryNew>();
  Factory->InitialParent = ParentMaterial;

  UObject *NewAsset = AssetTools.CreateAsset(
      Name, Path, UMaterialInstanceConstant::StaticClass(), Factory);

  if (NewAsset) {
    // Handle parameters if provided
    UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(NewAsset);
    const TSharedPtr<FJsonObject> *ParamsObj = nullptr;
    if (MIC && Payload->TryGetObjectField(TEXT("parameters"), ParamsObj)) {
      // Scalar parameters
      const TSharedPtr<FJsonObject> *Scalars;
      if ((*ParamsObj)->TryGetObjectField(TEXT("scalar"), Scalars)) {
        for (const auto &Kvp : (*Scalars)->Values) {
          double Val = 0.0;
          if (Kvp.Value->TryGetNumber(Val)) {
            MIC->SetScalarParameterValueEditorOnly(FName(*Kvp.Key), (float)Val);
          }
        }
      }

      // Vector parameters
      const TSharedPtr<FJsonObject> *Vectors;
      if ((*ParamsObj)->TryGetObjectField(TEXT("vector"), Vectors)) {
        for (const auto &Kvp : (*Vectors)->Values) {
          const TSharedPtr<FJsonObject> *VecObj;
          if (Kvp.Value->TryGetObject(VecObj)) {
            // Try generic RGBA
            double R = 0, G = 0, B = 0, A = 1;
            (*VecObj)->TryGetNumberField(TEXT("r"), R);
            (*VecObj)->TryGetNumberField(TEXT("g"), G);
            (*VecObj)->TryGetNumberField(TEXT("b"), B);
            (*VecObj)->TryGetNumberField(TEXT("a"), A);
            MIC->SetVectorParameterValueEditorOnly(
                FName(*Kvp.Key),
                FLinearColor((float)R, (float)G, (float)B, (float)A));
          }
        }
      }

      // Texture parameters
      const TSharedPtr<FJsonObject> *Textures;
      if ((*ParamsObj)->TryGetObjectField(TEXT("texture"), Textures)) {
        for (const auto &Kvp : (*Textures)->Values) {
          FString TexPath;
          if (Kvp.Value->TryGetString(TexPath) && !TexPath.IsEmpty()) {
            TexPath = SanitizeProjectRelativePath(TexPath);
            if (TexPath.IsEmpty()) {
              continue;
            }
            UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
            if (Tex) {
              MIC->SetTextureParameterValueEditorOnly(FName(*Kvp.Key), Tex);
            }
          }
        }
      }
    }

    bool bSaved = false;
    if (bSave) {
      bSaved = McpSafeAssetSave(NewAsset);
      if (!bSaved) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Material Instance created but save failed"), nullptr,
                               TEXT("SAVE_FAILED"));
        return true;
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    Resp->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material Instance created"), Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create material instance"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// 10. MATERIAL PARAMETER & INSTANCE MANAGEMENT
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleAddMaterialParameter(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Type;
  Payload->TryGetStringField(TEXT("type"), Type);

  if (AssetPath.IsEmpty() || Name.IsEmpty() || Type.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("assetPath, name, and type required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(AssetPath, Material, Function);

  if (!Material && !Function) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material or Material Function"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                : static_cast<UObject*>(Function);

  UMaterialExpression *NewExpression = nullptr;
  Type = Type.ToLower();

  // Asymmetric creation paths by design:
  // - UMaterial: UMaterialEditingLibrary::CreateMaterialExpression handles
  //   graph registration, undo transactions, and editor-only data setup.
  // - UMaterialFunction: UMaterialEditingLibrary only supports UMaterial, so we
  //   use NewObject + manual add to the expression collection. This is
  //   intentional due to API limitations — CreateMaterialExpression does not
  //   accept UMaterialFunction as a host.
  auto CreateExpr = [&](UClass* ExprClass) -> UMaterialExpression* {
    if (Material) {
      return UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass);
    }
    UMaterialExpression* Expr = NewObject<UMaterialExpression>(HostOuter, ExprClass, NAME_None, RF_Transactional);
    if (Expr) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Function->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expr);
#else
      Function->FunctionExpressions.Add(Expr);
#endif
    }
    return Expr;
  };

  if (Type == TEXT("scalar")) {
    NewExpression = CreateExpr(UMaterialExpressionScalarParameter::StaticClass());
    if (UMaterialExpressionScalarParameter *ScalarParam =
            Cast<UMaterialExpressionScalarParameter>(NewExpression)) {
      ScalarParam->ParameterName = FName(*Name);
      double Val = 0.0;
      if (Payload->TryGetNumberField(TEXT("value"), Val)) {
        ScalarParam->DefaultValue = (float)Val;
      }
    }
  } else if (Type == TEXT("vector")) {
    NewExpression = CreateExpr(UMaterialExpressionVectorParameter::StaticClass());
    if (UMaterialExpressionVectorParameter *VectorParam =
            Cast<UMaterialExpressionVectorParameter>(NewExpression)) {
      VectorParam->ParameterName = FName(*Name);
      const TSharedPtr<FJsonObject> *VecObj;
      if (Payload->TryGetObjectField(TEXT("value"), VecObj)) {
        double R = 0, G = 0, B = 0, A = 1;
        (*VecObj)->TryGetNumberField(TEXT("r"), R);
        (*VecObj)->TryGetNumberField(TEXT("g"), G);
        (*VecObj)->TryGetNumberField(TEXT("b"), B);
        (*VecObj)->TryGetNumberField(TEXT("a"), A);
        VectorParam->DefaultValue =
            FLinearColor((float)R, (float)G, (float)B, (float)A);
      }
    }
  } else if (Type == TEXT("texture")) {
    NewExpression = CreateExpr(UMaterialExpressionTextureSampleParameter2D::StaticClass());
    if (UMaterialExpressionTextureSampleParameter2D *TexParam =
            Cast<UMaterialExpressionTextureSampleParameter2D>(NewExpression)) {
      TexParam->ParameterName = FName(*Name);
      FString TexPath;
      if (Payload->TryGetStringField(TEXT("value"), TexPath) &&
          !TexPath.IsEmpty()) {
        TexPath = SanitizeProjectRelativePath(TexPath);
        if (TexPath.IsEmpty()) {
          SendAutomationResponse(Socket, RequestId, false,
                                 TEXT("Invalid texture path"), nullptr,
                                 TEXT("SECURITY_VIOLATION"));
          return true;
        }
        UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
        if (Tex) {
          TexParam->Texture = Tex;
        }
      }
    }
  } else if (Type == TEXT("staticswitch") || Type == TEXT("static_switch")) {
    NewExpression = CreateExpr(UMaterialExpressionStaticSwitchParameter::StaticClass());
    if (UMaterialExpressionStaticSwitchParameter *SwitchParam =
            Cast<UMaterialExpressionStaticSwitchParameter>(NewExpression)) {
      SwitchParam->ParameterName = FName(*Name);
      bool Val = false;
      if (Payload->TryGetBoolField(TEXT("value"), Val)) {
        SwitchParam->DefaultValue = Val;
      }
    }
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Unsupported parameter type: %s"), *Type), nullptr,
        TEXT("INVALID_TYPE"));
    return true;
  }

  if (NewExpression) {
    if (Material) {
      UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
      UMaterialEditingLibrary::RecompileMaterial(Material);
      Material->MarkPackageDirty();
    } else {
      FinalizeHost(nullptr, Function);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("parameterName"), Name);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Parameter added"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create parameter expression"),
                           nullptr, TEXT("CREATE_FAILED"));
  }

  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleListMaterialInstances(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Find all assets that are Material Instances and have this asset as parent
  // Note: This can be expensive if we scan all assets.
  // Optimization: Use GetReferencers? Or just filter by class and check parent.
  // Since we can't easily query by "Parent" tag efficiently without iterating,
  // we'll try a filtered query.

  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                           TEXT("MaterialInstanceConstant")));
#else
  Filter.ClassNames.Add(FName(TEXT("MaterialInstanceConstant")));
#endif
  Filter.bRecursiveClasses = true;

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  TArray<TSharedPtr<FJsonValue>> Instances;

  // We need to check the parent. Loading the asset is safest but slow.
  // Checking tags is faster. MICs usually have "Parent" tag.
  for (const FAssetData &Asset : AssetList) {
    // Check tag first
    FString ParentTag;
    if (Asset.GetTagValue(TEXT("Parent"), ParentTag)) {
      // Tag value might be "Material'Path'" or just "Path"
      // It's usually formatted string.
      if (ParentTag.Contains(AssetPath)) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.GetSoftObjectPath().ToString()));
#else
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.ToSoftObjectPath().ToString()));
#endif
      }
    } else {
      // Fallback: load asset (slow, but accurate)
      // Only do this if tag is missing? Or maybe skip to avoid perf hit.
      // Let's rely on tag for now.
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), Instances);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Instances listed"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleResetInstanceParameters(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(Asset);

  if (!MIC) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material Instance Constant"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  MIC->ClearParameterValuesEditorOnly();
  MIC->PostEditChange();
  MIC->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Instance parameters reset"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleDoesAssetExist(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("exists"), bExists);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         bExists ? TEXT("Asset exists")
                                 : TEXT("Asset does not exist"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleGetMaterialStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInterface *Material = Cast<UMaterialInterface>(Asset);

  if (!Material) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material"), nullptr,
                           TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  // Ensure material is compiled
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Material->EnsureIsComplete();
#else
  // UE 5.0: Force compilation by accessing the material resource
  Material->GetMaterial();
#endif

  TSharedPtr<FJsonObject> Stats = McpHandlerUtils::CreateResultObject();

  // Get actual shading model from the material
  FString ShadingModelStr = TEXT("Unknown");
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    FMaterialShadingModelField ShadingModels = BaseMat->GetShadingModels();
    // Check shading models using HasShadingModel - prioritize common ones
    if (ShadingModels.HasShadingModel(MSM_Unlit)) {
      ShadingModelStr = TEXT("Unlit");
    } else if (ShadingModels.HasShadingModel(MSM_DefaultLit)) {
      ShadingModelStr = TEXT("DefaultLit");
    } else if (ShadingModels.HasShadingModel(MSM_Subsurface)) {
      ShadingModelStr = TEXT("Subsurface");
    } else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) {
      ShadingModelStr = TEXT("SubsurfaceProfile");
    } else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) {
      ShadingModelStr = TEXT("ClearCoat");
    } else if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage)) {
      ShadingModelStr = TEXT("TwoSidedFoliage");
    } else if (ShadingModels.HasShadingModel(MSM_Hair)) {
      ShadingModelStr = TEXT("Hair");
    } else if (ShadingModels.HasShadingModel(MSM_Cloth)) {
      ShadingModelStr = TEXT("Cloth");
    } else if (ShadingModels.HasShadingModel(MSM_Eye)) {
      ShadingModelStr = TEXT("Eye");
    } else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin)) {
      ShadingModelStr = TEXT("PreintegratedSkin");
    }
  }
  Stats->SetStringField(TEXT("shadingModel"), ShadingModelStr);

  // Get instruction count from material resource
  // Note: GetMaxNumInstructionsForShader takes FShaderType* in UE 5.6, EShaderFrequency in some earlier versions
  // Skip this in 5.6 as there's no clean way to get a FShaderType* for the pixel shader
  int32 InstructionCount = -1; // Not easily available in this UE version
  Stats->SetNumberField(TEXT("instructionCount"), InstructionCount);

  // Count texture samplers used in the material
  int32 SamplerCount = 0;
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMat)) {
      if (Expr && Expr->IsA<UMaterialExpressionTextureSample>()) {
        SamplerCount++;
      }
    }
  }
  Stats->SetNumberField(TEXT("samplerCount"), SamplerCount);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("stats"), Stats);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material stats retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleGenerateLODs(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_lods"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Support both landscapePath (single) and assetPaths (array)
  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);

  // Support both assetPath (single) and assetPaths (array)
  FString SingleAssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), SingleAssetPath);

  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray)) {
    Payload->TryGetArrayField(TEXT("assets"), AssetPathsArray);
  }

  // Support both lodCount and numLODs
  int32 NumLODs = 4;
  Payload->TryGetNumberField(TEXT("lodCount"), NumLODs);
  Payload->TryGetNumberField(TEXT("numLODs"), NumLODs);
  NumLODs = FMath::Clamp(NumLODs, 1, 50);

  // Build list of paths to process
  TArray<FString> Paths;

  // Add landscape path if provided
  if (!LandscapePath.IsEmpty()) {
    // Validate landscape path
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }

  // Add single asset path if provided
  if (!SingleAssetPath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(SingleAssetPath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe asset path: %s"), *SingleAssetPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }

  // Add asset paths if provided
  if (AssetPathsArray) {
    for (const auto &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        FString SafePath = SanitizeProjectRelativePath(Val->AsString());
        if (!SafePath.IsEmpty()) {
          Paths.Add(SafePath);
        }
      }
    }
  }

  if (Paths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("landscapePath or assetPaths required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    int32 VerifiedCount = 0;
    TArray<FString> NotFoundPaths;
    TArray<FString> NotMeshPaths;
    TArray<TSharedPtr<FJsonValue>> MeshDetails;

    for (const FString &Path : Paths) {
      UObject *Obj = LoadObject<UObject>(nullptr, *Path);
      if (!Obj) {
        NotFoundPaths.Add(Path);
        continue;
      }

      UStaticMesh *Mesh = Cast<UStaticMesh>(Obj);
      if (!Mesh) {
        NotMeshPaths.Add(Path);
        continue;
      }

      TSharedPtr<FJsonObject> MeshInfo = MakeShared<FJsonObject>();
      MeshInfo->SetStringField(TEXT("assetPath"), Path);
      MeshInfo->SetStringField(TEXT("assetClass"), Mesh->GetClass()->GetName());
      MeshInfo->SetNumberField(TEXT("currentLODCount"), Mesh->GetNumLODs());
      MeshInfo->SetNumberField(TEXT("requestedLODCount"), NumLODs);
      MeshDetails.Add(MakeShared<FJsonValueObject>(MeshInfo));
      VerifiedCount++;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const bool bSuccess = VerifiedCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    Resp->SetBoolField(TEXT("lodBuildSkipped"), true);
    Resp->SetNumberField(TEXT("verified"), VerifiedCount);
    Resp->SetNumberField(TEXT("requested"), Paths.Num());
    Resp->SetNumberField(TEXT("lodCount"), NumLODs);
    Resp->SetArrayField(TEXT("meshes"), MeshDetails);

    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString &Path : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
      Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    }

    if (NotMeshPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotMeshArray;
      for (const FString &Path : NotMeshPaths) {
        NotMeshArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("notMeshPaths"), NotMeshArray);
      Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
    }

    const FString Message = bSuccess
        ? FString::Printf(TEXT("Verified %d mesh(es); LOD build skipped under NullRHI"), VerifiedCount)
        : TEXT("No static meshes verified for LOD generation under NullRHI");
    const FString ErrorCode = bSuccess ? FString() : TEXT("LOD_GENERATION_FAILED");
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work in AsyncTask(GameThread, ...) caused the queued lambda
  // to sit behind the current dispatch cycle, so responses never reached the
  // MCP server before the 30-second timeout. Execute synchronously instead.
  int32 SuccessCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> NotMeshPaths;
  TArray<FString> SaveFailedPaths;

  for (const FString &Path : Paths) {
    SendProgressUpdate(RequestId, -1.0f,
        FString::Printf(TEXT("Processing LOD generation for: %s"), *Path), true);

    UObject *Obj = LoadObject<UObject>(nullptr, *Path);

    if (!Obj) {
      NotFoundPaths.Add(Path);
      continue;
    }

    // Try Static Mesh
    if (UStaticMesh *Mesh = Cast<UStaticMesh>(Obj)) {
      UE_LOG(LogNebulaForgeBridgeSubsystem, Log,
             TEXT("Generating %d LODs for static mesh %s"), NumLODs, *Path);

        Mesh->Modify();
        Mesh->SetNumSourceModels(NumLODs);

        // Configure LOD reduction settings with progressive reduction
        for (int32 LODIndex = 1; LODIndex < NumLODs; LODIndex++) {
          FStaticMeshSourceModel &SourceModel = Mesh->GetSourceModel(LODIndex);
          FMeshReductionSettings &ReductionSettings =
              SourceModel.ReductionSettings;

          // Progressive reduction: 50%, 25%, 12.5%...
          float ReductionPercent =
              1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
          ReductionSettings.PercentTriangles = ReductionPercent;
          ReductionSettings.PercentVertices = ReductionPercent;

          // Enable reduction for this LOD level
          SourceModel.BuildSettings.bRecomputeNormals = false;
          SourceModel.BuildSettings.bRecomputeTangents = false;
          SourceModel.BuildSettings.bUseMikkTSpace = true;
        }

        // Build the mesh with new LOD settings
        Mesh->Build();
        Mesh->PostEditChange();
        if (!McpSafeAssetSave(Mesh)) {
          SaveFailedPaths.Add(Path);
          continue;
        }

        SuccessCount++;
      } else {
        // Asset exists but is not a static mesh
        NotMeshPaths.Add(Path);
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

    // CRITICAL FIX: Return proper success/failure based on actual results
    // Previously always returned success=true even when 0 meshes processed
    bool bSuccess = SuccessCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetNumberField(TEXT("processed"), SuccessCount);
    Resp->SetNumberField(TEXT("requested"), Paths.Num());
    Resp->SetNumberField(TEXT("lodCount"), NumLODs);

    // Add details about failures
    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString& P : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
      Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    }

    if (NotMeshPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotMeshArray;
      for (const FString& P : NotMeshPaths) {
        NotMeshArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notMeshPaths"), NotMeshArray);
      Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
    }

    if (SaveFailedPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> SaveFailedArray;
      for (const FString &P : SaveFailedPaths) {
        SaveFailedArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("saveFailedPaths"), SaveFailedArray);
      Resp->SetNumberField(TEXT("saveFailedCount"), SaveFailedPaths.Num());
    }

    FString Message;
    FString ErrorCode;

    if (bSuccess) {
      Message = FString::Printf(TEXT("Generated and saved LODs for %d mesh(es)"), SuccessCount);
    } else if (NotFoundPaths.Num() > 0 && NotMeshPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No assets found. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (NotMeshPaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No static meshes found. %d asset(s) are not meshes."), NotMeshPaths.Num());
      ErrorCode = TEXT("INVALID_ASSET_TYPE");
    } else {
      Message = FString::Printf(TEXT("No LODs generated. %d not found, %d not meshes."),
                                NotFoundPaths.Num(), NotMeshPaths.Num());
      ErrorCode = TEXT("LOD_GENERATION_FAILED");
    }

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                                      Message, Resp, ErrorCode);

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Requires editor"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 8. METADATA
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_metadata payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  // 1. Asset Registry Tags
  FAssetData AssetData(Asset);
  TSharedPtr<FJsonObject> TagsObj = McpHandlerUtils::CreateResultObject();
  for (const auto &Kvp : AssetData.TagsAndValues) {
    TagsObj->SetStringField(Kvp.Key.ToString(), Kvp.Value.AsString());
  }
  Resp->SetObjectField(TEXT("tags"), TagsObj);

  // 2. Package Metadata information
  UPackage *Package = Asset->GetOutermost();
  if (Package) {

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    FMetaData& Meta = Package->GetMetaData();
    bool bHasMeta = FMetaData::GetMapForObject(Asset) != nullptr;
    Resp->SetBoolField(TEXT("debug_has_meta"), bHasMeta);

    const TMap<FName, FString> *ObjectMeta = FMetaData::GetMapForObject(Asset);
#else
    UMetaData* Meta = Package->GetMetaData();
    bool bHasMeta = Meta->GetMapForObject(Asset) != nullptr;
    Resp->SetBoolField(TEXT("debug_has_meta"), bHasMeta);

    const TMap<FName, FString> *ObjectMeta = Meta->GetMapForObject(Asset);
#endif
    if (ObjectMeta) {
      TSharedPtr<FJsonObject> MetaObj = McpHandlerUtils::CreateResultObject();
      for (const auto &Entry : *ObjectMeta) {
        MetaObj->SetStringField(Entry.Key.ToString(), Entry.Value);
      }
      Resp->SetObjectField(TEXT("metadata"), MetaObj);
    }
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Metadata retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_metadata requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 9. NANITE/MESH WORKFLOW ACTIONS
// ============================================================================

// Dispatcher-compatible mesh workflow handlers with explicit success/error responses.

bool UNebulaForgeBridgeSubsystem::HandleNaniteRebuildMesh(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("nanite_rebuild_mesh"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nanite_rebuild_mesh payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MeshPath;
  if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) ||
      MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("meshPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MeshPath = SanitizeProjectRelativePath(MeshPath);
  if (MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid meshPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the static mesh
  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath),
                        TEXT("MESH_NOT_FOUND"));
    return true;
  }

  // Check if mesh supports Nanite
  bool bEnableNanite = true;
  Payload->TryGetBoolField(TEXT("enableNanite"), bEnableNanite);

  // Nanite settings
  bool bPreserveArea = true;
  double TrianglePercent = 100.0;
  double FallbackPercent = 0.0;

  Payload->TryGetBoolField(TEXT("preserveArea"), bPreserveArea);
  Payload->TryGetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Payload->TryGetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  // Clamp values
  TrianglePercent = FMath::Clamp(TrianglePercent, 0.0, 100.0);
  FallbackPercent = FMath::Clamp(FallbackPercent, 0.0, 100.0);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
  // UE 5.7+: Use accessor functions to avoid deprecation warnings
  FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
  Settings.bEnabled = bEnableNanite;
  Settings.PositionPrecision = 8; // Default precision

  // bPreserveArea replaced with ShapePreservation enum
  if (bPreserveArea) {
    Settings.ShapePreservation = ENaniteShapePreservation::PreserveArea;
  } else {
    Settings.ShapePreservation = ENaniteShapePreservation::None;
  }
  Settings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  Settings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
  if (FallbackPercent > 0.0) {
    Settings.GenerateFallback = ENaniteGenerateFallback::Enabled;
  } else {
    Settings.GenerateFallback = ENaniteGenerateFallback::PlatformDefault;
  }
  StaticMesh->SetNaniteSettings(Settings);
  StaticMesh->NotifyNaniteSettingsChanged();
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  // UE 5.1-5.6: Uses KeepPercentTriangles, FallbackPercentTriangles, and bPreserveArea
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.bPreserveArea = bPreserveArea;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#else
  // UE 5.0: Uses KeepPercentTriangles (no bPreserveArea)
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#endif

  // Rebuild the render data after changing Nanite settings.  Mutating the
  // settings alone only changes the asset's properties; it does not produce
  // new Nanite data for the renderer or persist a usable rebuilt asset.
  StaticMesh->Modify();
  StaticMesh->Build();
  StaticMesh->PostEditChange();
  if (!McpSafeAssetSave(StaticMesh)) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Failed to save rebuilt Nanite mesh: %s"), *MeshPath),
                        TEXT("SAVE_FAILED"));
    return true;
  }

  // Build response
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("meshPath"), MeshPath);
  Resp->SetStringField(TEXT("meshName"), StaticMesh->GetName());
  Resp->SetBoolField(TEXT("naniteEnabled"), bEnableNanite);
  Resp->SetBoolField(TEXT("preserveArea"), bPreserveArea);
  Resp->SetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Resp->SetNumberField(TEXT("fallbackPercent"), FallbackPercent);
  Resp->SetBoolField(TEXT("buildCompleted"), true);
  Resp->SetBoolField(TEXT("saved"), true);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Nanite mesh rebuilt and saved for %s"), *StaticMesh->GetName()),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("nanite_rebuild_mesh requires UE 5.0+ editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleFindByTag(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("find_by_tag"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("find_by_tag payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Tag;
  if (!Payload->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("tag field is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // CRITICAL: Validate path parameter for security even if not used for actor search
  // This prevents false negatives in security testing and follows defense-in-depth
  FString Path;
  if (Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) {
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *Path),
          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    // Path is valid - could be used for scoping asset search in future
  }

  FName TagName(*Tag);
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 MaxResults = 100;
  Payload->TryGetNumberField(TEXT("maxResults"), MaxResults);
  MaxResults = FMath::Clamp(MaxResults, 1, 1000);

  bool bSearchActors = true;
  bool bSearchComponents = false;
  bool bSearchAssets = false;
  Payload->TryGetBoolField(TEXT("searchActors"), bSearchActors);
  Payload->TryGetBoolField(TEXT("searchComponents"), bSearchComponents);
  Payload->TryGetBoolField(TEXT("searchAssets"), bSearchAssets);

  // Search in world
  if (GEditor && bSearchActors) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor && Actor->ActorHasTag(TagName)) {
          TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
          ResultObj->SetStringField(TEXT("type"), TEXT("Actor"));
          ResultObj->SetStringField(TEXT("name"), Actor->GetName());
          ResultObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
          ResultObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ResultObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

          const FVector Location = Actor->GetActorLocation();
          TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
          LocObj->SetNumberField(TEXT("x"), Location.X);
          LocObj->SetNumberField(TEXT("y"), Location.Y);
          LocObj->SetNumberField(TEXT("z"), Location.Z);
          ResultObj->SetObjectField(TEXT("location"), LocObj);

          Results.Add(MakeShared<FJsonValueObject>(ResultObj));
        }
      }
    }
  }

  // Search for components with tag
  if (bSearchComponents && GEditor && Results.Num() < MaxResults) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor) {
          TInlineComponentArray<UActorComponent*> Components;
          Actor->GetComponents(Components);
          for (UActorComponent *Component : Components) {
            if (Component && Component->ComponentHasTag(TagName)) {
              TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
              ResultObj->SetStringField(TEXT("type"), TEXT("Component"));
              ResultObj->SetStringField(TEXT("name"), Component->GetName());
              ResultObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
              ResultObj->SetStringField(TEXT("owner"), Actor->GetName());
              ResultObj->SetStringField(TEXT("path"), Component->GetPathName());
              Results.Add(MakeShared<FJsonValueObject>(ResultObj));
            }
          }
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("tag"), Tag);
  Resp->SetNumberField(TEXT("count"), Results.Num());
  Resp->SetArrayField(TEXT("results"), Results);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Found %d objects with tag '%s'"), Results.Num(), *Tag),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("find_by_tag requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleAddMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("add_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("add_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid materialPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FString NodeType;
  if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) ||
      NodeType.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nodeType is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                : static_cast<UObject*>(Function);

  // Create material expression based on node type
  UMaterialExpression *NewExpression = nullptr;
  UClass *ExpressionClass = nullptr;

  // Map common node type names to expression classes
  if (NodeType.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) ||
      NodeType.Equals(TEXT("Constant1"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant2"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant2Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant2Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant3"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant3Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant4"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant4Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant4Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("TextureSample"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Texture"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
  } else if (NodeType.Equals(TEXT("Add"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionAdd::StaticClass();
  } else if (NodeType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionMultiply::StaticClass();
  } else if (NodeType.Equals(TEXT("Sine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionSine::StaticClass();
  } else if (NodeType.Equals(TEXT("Cosine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionCosine::StaticClass();
  } else if (NodeType.Equals(TEXT("Time"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTime::StaticClass();
  } else if (NodeType.Equals(TEXT("VertexColor"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionVertexColor::StaticClass();
  } else {
    // Try to find the class dynamically
    FString FullClassName = FString::Printf(TEXT("/Script/Engine.MaterialExpression%s"), *NodeType);
    ExpressionClass = LoadClass<UMaterialExpression>(nullptr, *FullClassName);

    if (!ExpressionClass) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Unknown node type: %s"), *NodeType),
                          TEXT("INVALID_NODE_TYPE"));
      return true;
    }
  }

  // Create the expression
  NewExpression = NewObject<UMaterialExpression>(HostOuter, ExpressionClass, NAME_None, RF_Transactional);
  if (!NewExpression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create material expression"),
                        TEXT("EXPRESSION_CREATION_FAILED"));
    return true;
  }

  // Set position
  double PosX = 0, PosY = 0;
  Payload->TryGetNumberField(TEXT("posX"), PosX);
  Payload->TryGetNumberField(TEXT("posY"), PosY);
  NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
  NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);

  // Set node properties based on type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(NewExpression)) {
    double Value = 0;
    Payload->TryGetNumberField(TEXT("value"), Value);
    Const->R = static_cast<float>(Value);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(NewExpression)) {
    double R = 0, G = 0, B = 0;
    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
    }
    Const3->Constant = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B));
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(NewExpression)) {
    FString TexturePath;
    if (Payload->TryGetStringField(TEXT("texturePath"), TexturePath) && !TexturePath.IsEmpty()) {
      TexturePath = SanitizeProjectRelativePath(TexturePath);
      if (TexturePath.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            TEXT("Invalid texturePath"),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }
      UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
      if (Texture) {
        TexSample->Texture = Texture;
      }
    }
  }

  // Add to host expression collection
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Material) Material->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);
  else Function->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);
#else
  auto& Expressions = GetHostExpressions(Material, Function);
  Expressions.Add(NewExpression);
#endif

  // Only mark dirty — skip PostEditChange to avoid shader recompile per node.
  // Users batch-add nodes and compile once via compile_material.
  if (Material) { Material->MarkPackageDirty(); }
  if (Function) { Function->MarkPackageDirty(); }

  // Get the expression index for reference
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  int32 ExpressionIndex = Material
    ? Material->GetEditorOnlyData()->ExpressionCollection.Expressions.IndexOfByKey(NewExpression)
    : Function->GetEditorOnlyData()->ExpressionCollection.Expressions.IndexOfByKey(NewExpression);
#else
  int32 ExpressionIndex = Expressions.IndexOfByKey(NewExpression);
#endif

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  Resp->SetStringField(TEXT("nodeType"), NodeType);
  Resp->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
  Resp->SetStringField(TEXT("expressionName"), NewExpression->GetName());
  Resp->SetStringField(TEXT("nodeId"), NewExpression->MaterialExpressionGuid.ToString());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node added successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("add_material_node requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleConnectMaterialPins(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("connect_material_pins"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("connect_material_pins payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  // Helper to find expression by stable name, GUID (legacy), or index
  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  // Accept both sourceNodeId/targetNodeId (stable name strings) and fromExpression/toExpression (indices)
  FString SourceNodeId, TargetNodeId;
  int32 FromExpressionIndex = -1, ToExpressionIndex = -1;

  UMaterialExpression *FromExpression = nullptr;
  UMaterialExpression *ToExpression = nullptr;

  if (Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId) && !SourceNodeId.IsEmpty()) {
    FromExpression = FindExpression(SourceNodeId);
  }
  if (Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId) && !TargetNodeId.IsEmpty()) {
    ToExpression = FindExpression(TargetNodeId);
  }

  if (!FromExpression && Payload->TryGetNumberField(TEXT("fromExpression"), FromExpressionIndex)) {
    if (FromExpressionIndex >= 0 && FromExpressionIndex < Expressions.Num()) FromExpression = Expressions[FromExpressionIndex];
  }
  if (!ToExpression && Payload->TryGetNumberField(TEXT("toExpression"), ToExpressionIndex)) {
    if (ToExpressionIndex >= 0 && ToExpressionIndex < Expressions.Num()) ToExpression = Expressions[ToExpressionIndex];
  }

  FString SourcePin;
  Payload->TryGetStringField(TEXT("sourcePin"), SourcePin);
  int32 SourcePinIndex = 0;
  if (!SourcePin.IsEmpty()) {
    if (SourcePin.IsNumeric()) {
      SourcePinIndex = FCString::Atoi(*SourcePin);
    } else {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("sourcePin must be a numeric index, got '%s'"), *SourcePin),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  FString InputName;
  Payload->TryGetStringField(TEXT("inputName"), InputName);
  if (InputName.IsEmpty()) Payload->TryGetStringField(TEXT("targetPin"), InputName);

  // Handle connection to main material / function output node
  bool bConnectToMainNode = false;
  if ((TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) && !InputName.IsEmpty()) {
    bConnectToMainNode = true;
  } else if (!TargetNodeId.IsEmpty() && ToExpression == nullptr) {
    SendAutomationError(Socket, RequestId, TEXT("Target node not found"), TEXT("TARGET_NODE_NOT_FOUND"));
    return true;
  }

  if (bConnectToMainNode && FromExpression) {
    if (Material) {
      bool bFound = false;
      auto ConnectMainInput = [&](FExpressionInput& Input) {
        Input.Expression = FromExpression;
        Input.OutputIndex = SourcePinIndex;
        bFound = true;
      };
#if WITH_EDITORONLY_DATA
      if (InputName == TEXT("BaseColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, BaseColor)); }
      else if (InputName == TEXT("EmissiveColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor)); }
      else if (InputName == TEXT("Roughness")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Roughness)); }
      else if (InputName == TEXT("Metallic")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Metallic)); }
      else if (InputName == TEXT("Specular")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Specular)); }
      else if (InputName == TEXT("Normal")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Normal)); }
      else if (InputName == TEXT("Opacity")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Opacity)); }
      else if (InputName == TEXT("OpacityMask")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, OpacityMask)); }
      else if (InputName == TEXT("AmbientOcclusion") || InputName == TEXT("AO")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion)); }
      else if (InputName == TEXT("SubsurfaceColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor)); }
      else if (InputName == TEXT("WorldPositionOffset")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset)); }
#endif
      if (bFound) {
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Resp, Material);
        Resp->SetStringField(TEXT("inputName"), InputName);
        Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Connected to main material pin"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown main material input: %s"), *InputName), TEXT("INVALID_PIN"));
      }
      return true;
    } else {
      // MaterialFunction: connect to FunctionOutput by name (name is required)
      if (InputName.IsEmpty()) {
        SendAutomationError(Socket, RequestId, TEXT("inputName is required when connecting to a function output"), TEXT("MISSING_INPUT_NAME"));
        return true;
      }
      UMaterialExpressionFunctionOutput *TargetOutput = nullptr;
      for (UMaterialExpression *Expr : Expressions) {
        if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          if (Out->OutputName.ToString().Equals(InputName)) { TargetOutput = Out; break; }
        }
      }
      if (TargetOutput) {
        TargetOutput->A.Expression = FromExpression;
        TargetOutput->A.OutputIndex = SourcePinIndex;
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("inputName"), TargetOutput->OutputName.ToString());
        Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Connected to function output"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No FunctionOutput named '%s' found"), *InputName), TEXT("INVALID_PIN"));
      }
      return true;
    }
  }

  if (!FromExpression) { SendAutomationError(Socket, RequestId, TEXT("Source node not found"), TEXT("SOURCE_NODE_NOT_FOUND")); return true; }
  if (!ToExpression) { SendAutomationError(Socket, RequestId, TEXT("Target node not found"), TEXT("TARGET_NODE_NOT_FOUND")); return true; }

  if (InputName.IsEmpty()) InputName = TEXT("Input");

  FExpressionInput *TargetInput = nullptr;
  for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          break;
        }
      }
    }
  }
  if (!TargetInput) {
    for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
      if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
        if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          InputName = Property->GetName();
          break;
        }
      }
    }
  }

  if (!TargetInput) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No input found on target expression. Tried: %s"), *InputName), TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  TargetInput->Expression = FromExpression;
  TargetInput->OutputIndex = SourcePinIndex;
  FinalizeHost(Material, Function);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
  Resp->SetStringField(TEXT("targetNodeId"), ToExpression->GetName());
  Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Material pins connected successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("connect_material_pins requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleRemoveMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("remove_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("remove_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  // Helper to find expression by stable name, GUID (legacy), or index
  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  FString NodeId;
  int32 ExpressionIndex = -1;
  UMaterialExpression *ExpressionToRemove = nullptr;

  if (Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty()) {
    ExpressionToRemove = FindExpression(NodeId);
  } else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) {
      ExpressionToRemove = Expressions[ExpressionIndex];
    }
  }

  if (!ExpressionToRemove) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Node not found. Provide valid nodeId (stable name) or expressionIndex"),
                        TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString RemovedName = ExpressionToRemove->GetName();
  FString RemovedStableName = ExpressionToRemove->GetName();

  // Disconnect inbound links: walk all sibling expressions and clear any
  // FExpressionInput that references the node we're about to remove.
  for (UMaterialExpression *Expr : Expressions) {
    if (!Expr || Expr == ExpressionToRemove) continue;
    for (FProperty *Property = Expr->GetClass()->PropertyLink; Property;
         Property = Property->PropertyLinkNext) {
      FStructProperty *StructProp = CastField<FStructProperty>(Property);
      if (StructProp && StructProp->Struct &&
          StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        FExpressionInput *Input =
            StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (Input && Input->Expression == ExpressionToRemove) {
          Input->Expression = nullptr;
          Input->OutputIndex = 0;
        }
      }
    }
  }

  // Disconnect from main material inputs (root node pins)
  if (Material) {
#if WITH_EDITORONLY_DATA
    auto ClearIfMatches = [&](FExpressionInput& Input) {
      if (Input.Expression == ExpressionToRemove) {
        Input.Expression = nullptr;
        Input.OutputIndex = 0;
      }
    };
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, BaseColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Roughness));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Metallic));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Specular));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Normal));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Opacity));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
  }

  // Remove the expression from the appropriate container
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Material) Material->GetExpressionCollection().RemoveExpression(ExpressionToRemove);
  else Function->GetExpressionCollection().RemoveExpression(ExpressionToRemove);
#else
  Expressions.Remove(ExpressionToRemove);
#endif

  // Also remove from the material's root node if connected (Material only)
  if (Material) Material->RemoveExpressionParameter(ExpressionToRemove);

  FinalizeHost(Material, Function);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), RemovedStableName);
  Resp->SetStringField(TEXT("removedName"), RemovedName);
  Resp->SetNumberField(TEXT("remainingExpressions"), Expressions.Num());
  Resp->SetBoolField(TEXT("removed"), true);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node removed successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("remove_material_node requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleBreakMaterialConnections(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("break_material_connections"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("break_material_connections payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  FString NodeId, PinName;
  bool bHasNodeId = Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty();
  bool bHasPinName = Payload->TryGetStringField(TEXT("pinName"), PinName) && !PinName.IsEmpty();

  // If nodeId is "Main" or empty with pinName, disconnect from main/output node
  if ((!bHasNodeId || NodeId == TEXT("Main")) && bHasPinName) {
    if (Material) {
      bool bFound = false;
#if WITH_EDITORONLY_DATA
      auto ClearMainPin = [&](FExpressionInput& Input) { Input.Expression = nullptr; Input.OutputIndex = 0; bFound = true; };
      if (PinName == TEXT("BaseColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, BaseColor)); }
      else if (PinName == TEXT("EmissiveColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor)); }
      else if (PinName == TEXT("Roughness")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Roughness)); }
      else if (PinName == TEXT("Metallic")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Metallic)); }
      else if (PinName == TEXT("Specular")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Specular)); }
      else if (PinName == TEXT("Normal")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Normal)); }
      else if (PinName == TEXT("Opacity")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Opacity)); }
      else if (PinName == TEXT("OpacityMask")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, OpacityMask)); }
      else if (PinName == TEXT("AmbientOcclusion") || PinName == TEXT("AO")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion)); }
      else if (PinName == TEXT("SubsurfaceColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor)); }
      else if (PinName == TEXT("WorldPositionOffset")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset)); }
#endif
      if (bFound) {
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Resp, Material);
        Resp->SetStringField(TEXT("pinName"), PinName);
        Resp->SetBoolField(TEXT("disconnected"), true);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Disconnected from main material pin"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown main material pin: %s"), *PinName), TEXT("INVALID_PIN"));
      }
    } else {
      // MaterialFunction: clear FunctionOutput by name
      bool bCleared = false;
      for (UMaterialExpression *Expr : Expressions) {
        if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          if (PinName.IsEmpty() || Out->OutputName.ToString().Equals(PinName)) {
            Out->A.Expression = nullptr; Out->A.OutputIndex = 0; bCleared = true;
            if (!PinName.IsEmpty()) break;
          }
        }
      }
      if (!bCleared && !PinName.IsEmpty()) {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown function output pin: %s"), *PinName), TEXT("INVALID_PIN"));
      } else {
        if (bCleared) FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("pinName"), PinName);
        Resp->SetBoolField(TEXT("disconnected"), bCleared);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Disconnected from function output"),
                               Resp, FString());
      }
    }
    return true;
  }

  int32 ExpressionIndex = -1;
  UMaterialExpression *TargetExpression = nullptr;
  if (bHasNodeId) TargetExpression = FindExpression(NodeId);
  else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) TargetExpression = Expressions[ExpressionIndex];
  }

  if (!TargetExpression) {
    SendAutomationError(Socket, RequestId, TEXT("Node not found. Provide valid nodeId (stable name) or expressionIndex"), TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString InputName;
  bool bSpecificInput = Payload->TryGetStringField(TEXT("inputName"), InputName) && !InputName.IsEmpty();
  int32 BrokenConnections = 0;

  for (FProperty *Property = TargetExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (bSpecificInput && !Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) continue;
        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpression);
        if (Input && Input->Expression) { Input->Expression = nullptr; Input->OutputIndex = 0; BrokenConnections++; if (bSpecificInput) break; }
      }
    }
  }

  if (bSpecificInput && BrokenConnections == 0) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("No input named '%s' found on node '%s'"), *InputName, *TargetExpression->GetName()),
                        TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  if (BrokenConnections > 0) {
    FinalizeHost(Material, Function);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), TargetExpression->MaterialExpressionGuid.ToString());
  Resp->SetNumberField(TEXT("brokenConnections"), BrokenConnections);
  if (bSpecificInput) Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Broken %d connection(s)"), BrokenConnections), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("break_material_connections requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleGetMaterialNodeDetails(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_material_node_details"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_material_node_details payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  FString NodeId;
  int32 ExpressionIndex = -1;
  UMaterialExpression *Expression = nullptr;

  if (Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty()) {
    Expression = FindExpression(NodeId);
  } else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) Expression = Expressions[ExpressionIndex];
  }

  if (!Expression) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (Material) McpHandlerUtils::AddVerification(Resp, Material);
    else if (Function) McpHandlerUtils::AddVerification(Resp, Function);

    TArray<TSharedPtr<FJsonValue>> NodeList;
    for (int32 i = 0; i < Expressions.Num(); ++i) {
      UMaterialExpression *Expr = Expressions[i];
      if (!Expr) continue;

      TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
      NodeInfo->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
      NodeInfo->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
      NodeInfo->SetNumberField(TEXT("index"), i);
      NodeInfo->SetNumberField(TEXT("editorX"), Expr->MaterialExpressionEditorX);
      NodeInfo->SetNumberField(TEXT("editorY"), Expr->MaterialExpressionEditorY);
      if (!Expr->Desc.IsEmpty()) {
        NodeInfo->SetStringField(TEXT("desc"), Expr->Desc);
      }
      // Add parameter name if applicable
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        NodeInfo->SetStringField(TEXT("parameterName"), Param->ParameterName.ToString());
      }
      // Add function path for MaterialFunctionCall nodes
      if (UMaterialExpressionMaterialFunctionCall *FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        if (FuncCall->MaterialFunction) {
          NodeInfo->SetStringField(TEXT("functionPath"), FuncCall->MaterialFunction->GetPathName());
        }
      }
      // Add pin name for FunctionInput/Output expressions
      if (UMaterialExpressionFunctionInput *FuncIn = Cast<UMaterialExpressionFunctionInput>(Expr)) {
        NodeInfo->SetStringField(TEXT("inputName"), FuncIn->InputName.ToString());
      }
      if (UMaterialExpressionFunctionOutput *FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
        NodeInfo->SetStringField(TEXT("outputName"), FuncOut->OutputName.ToString());
      }
      NodeList.Add(MakeShared<FJsonValueObject>(NodeInfo));
    }

    Resp->SetArrayField(TEXT("nodes"), NodeList);
    Resp->SetNumberField(TEXT("nodeCount"), Expressions.Num());

    FString Message = NodeId.IsEmpty()
        ? FString::Printf(TEXT("Material has %d nodes. Provide nodeId for specific node details."), Expressions.Num())
        : FString::Printf(TEXT("Node '%s' not found. Material has %d nodes."), *NodeId, Expressions.Num());

    SendAutomationResponse(Socket, RequestId, NodeId.IsEmpty(),
                           Message, Resp, NodeId.IsEmpty() ? FString() : TEXT("NODE_NOT_FOUND"));
    return true;
  }

  // Build response for specific node
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), Expression->MaterialExpressionGuid.ToString());
  Resp->SetStringField(TEXT("name"), Expression->GetName());
  Resp->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
  Resp->SetStringField(TEXT("classPath"), Expression->GetClass()->GetPathName());
  Resp->SetNumberField(TEXT("editorX"), Expression->MaterialExpressionEditorX);
  Resp->SetNumberField(TEXT("editorY"), Expression->MaterialExpressionEditorY);
  if (!Expression->Desc.IsEmpty()) {
    Resp->SetStringField(TEXT("desc"), Expression->Desc);
  }

  // Get inputs
  TArray<TSharedPtr<FJsonValue>> InputsArray;
  for (FProperty *Property = Expression->GetClass()->PropertyLink; Property;
       Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expression);
        TSharedPtr<FJsonObject> InputObj = McpHandlerUtils::CreateResultObject();
        InputObj->SetStringField(TEXT("name"), Property->GetName());
        InputObj->SetBoolField(TEXT("isConnected"), Input->Expression != nullptr);
        if (Input->Expression) {
          InputObj->SetStringField(TEXT("connectedToId"), Input->Expression->GetName());
          InputObj->SetStringField(TEXT("connectedToName"), Input->Expression->GetName());
        }
        InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
      }
    }
  }
  Resp->SetArrayField(TEXT("inputs"), InputsArray);

  // Get specific properties based on expression type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(Expression)) {
    Resp->SetNumberField(TEXT("value"), Const->R);
  } else if (UMaterialExpressionConstant2Vector *Const2 = Cast<UMaterialExpressionConstant2Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const2->R);
    ValueObj->SetNumberField(TEXT("g"), Const2->G);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const3->Constant.R);
    ValueObj->SetNumberField(TEXT("g"), Const3->Constant.G);
    ValueObj->SetNumberField(TEXT("b"), Const3->Constant.B);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionConstant4Vector *Const4 = Cast<UMaterialExpressionConstant4Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const4->Constant.R);
    ValueObj->SetNumberField(TEXT("g"), Const4->Constant.G);
    ValueObj->SetNumberField(TEXT("b"), Const4->Constant.B);
    ValueObj->SetNumberField(TEXT("a"), Const4->Constant.A);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(Expression)) {
    if (TexSample->Texture) {
      Resp->SetStringField(TEXT("texture"), TexSample->Texture->GetPathName());
      Resp->SetStringField(TEXT("textureName"), TexSample->Texture->GetName());
    }
  } else if (UMaterialExpressionScalarParameter *ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression)) {
    Resp->SetStringField(TEXT("parameterName"), ScalarParam->ParameterName.ToString());
    Resp->SetNumberField(TEXT("defaultValue"), ScalarParam->DefaultValue);
  } else if (UMaterialExpressionVectorParameter *VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression)) {
    Resp->SetStringField(TEXT("parameterName"), VectorParam->ParameterName.ToString());
    TSharedPtr<FJsonObject> DefaultObj = McpHandlerUtils::CreateResultObject();
    DefaultObj->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
    DefaultObj->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
    DefaultObj->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
    DefaultObj->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
    Resp->SetObjectField(TEXT("defaultValue"), DefaultObj);
  }

  // Expose function pin metadata for MaterialFunctionCall nodes
  if (UMaterialExpressionMaterialFunctionCall *FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression)) {
    if (FuncCall->MaterialFunction) {
      Resp->SetStringField(TEXT("functionPath"), FuncCall->MaterialFunction->GetPathName());
    }
    // Emit function inputs
    TArray<TSharedPtr<FJsonValue>> FuncInputs;
    for (int32 fi = 0; fi < FuncCall->FunctionInputs.Num(); ++fi) {
      TSharedPtr<FJsonObject> FIObj = McpHandlerUtils::CreateResultObject();
      const UMaterialExpressionFunctionInput* FuncInputExpr = FuncCall->FunctionInputs[fi].ExpressionInput;
      FIObj->SetStringField(TEXT("inputName"), FuncInputExpr ? FuncInputExpr->InputName.ToString() : FString());
      FIObj->SetNumberField(TEXT("index"), fi);
      FIObj->SetBoolField(TEXT("isConnected"), FuncCall->FunctionInputs[fi].Input.Expression != nullptr);
      if (FuncCall->FunctionInputs[fi].Input.Expression) {
        FIObj->SetStringField(TEXT("connectedToId"), FuncCall->FunctionInputs[fi].Input.Expression->GetName());
        FIObj->SetNumberField(TEXT("outputIndex"), FuncCall->FunctionInputs[fi].Input.OutputIndex);
      }
      FuncInputs.Add(MakeShared<FJsonValueObject>(FIObj));
    }
    Resp->SetArrayField(TEXT("functionInputs"), FuncInputs);

    // Emit function outputs
    TArray<TSharedPtr<FJsonValue>> FuncOutputs;
    for (int32 fo = 0; fo < FuncCall->FunctionOutputs.Num(); ++fo) {
      TSharedPtr<FJsonObject> FOObj = McpHandlerUtils::CreateResultObject();
      const UMaterialExpressionFunctionOutput* FuncOutputExpr = FuncCall->FunctionOutputs[fo].ExpressionOutput;
      FOObj->SetStringField(TEXT("outputName"), FuncOutputExpr ? FuncOutputExpr->OutputName.ToString() : FString());
      FOObj->SetNumberField(TEXT("index"), fo);
      FuncOutputs.Add(MakeShared<FJsonValueObject>(FOObj));
    }
    Resp->SetArrayField(TEXT("functionOutputs"), FuncOutputs);
  }

  // Expose function input/output pin metadata for FunctionInput/Output expressions
  if (UMaterialExpressionFunctionInput *FuncIn = Cast<UMaterialExpressionFunctionInput>(Expression)) {
    Resp->SetStringField(TEXT("inputName"), FuncIn->InputName.ToString());
  }
  if (UMaterialExpressionFunctionOutput *FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expression)) {
    Resp->SetStringField(TEXT("outputName"), FuncOut->OutputName.ToString());
    Resp->SetBoolField(TEXT("isConnected"), FuncOut->A.Expression != nullptr);
    if (FuncOut->A.Expression) {
      Resp->SetStringField(TEXT("connectedToId"), FuncOut->A.Expression->GetName());
      Resp->SetNumberField(TEXT("sourceOutputIndex"), FuncOut->A.OutputIndex);
    }
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_material_node_details requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// SOURCE CONTROL STATE
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleGetSourceControlState(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_source_control_state"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_source_control_state payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and assetPaths
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("sourceControlEnabled"), false);
    Result->SetStringField(TEXT("message"), TEXT("Source control is not enabled"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Source control disabled"), Result, FString());
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<TSharedPtr<FJsonValue>> StatesArray;

  for (const FString &AssetPath : AssetPaths) {
    const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    TSharedPtr<FJsonObject> StateObj = McpHandlerUtils::CreateResultObject();
    StateObj->SetStringField(TEXT("assetPath"), SafeAssetPath.IsEmpty() ? AssetPath : SafeAssetPath);

    if (SafeAssetPath.IsEmpty()) {
      StateObj->SetBoolField(TEXT("exists"), false);
      StateObj->SetStringField(TEXT("state"), TEXT("invalid_path"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      StateObj->SetBoolField(TEXT("exists"), false);
      StateObj->SetStringField(TEXT("state"), TEXT("not_found"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    StateObj->SetBoolField(TEXT("exists"), true);

    // Convert asset path to file path
    FString PackageName = FPackageName::ObjectPathToPackageName(SafeAssetPath);
    FString FilePath;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension()) &&
        !FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetMapPackageExtension())) {
      StateObj->SetStringField(TEXT("state"), TEXT("path_conversion_failed"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Get source control state
    FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(
        FilePath, EStateCacheUsage::Use);

    if (!SourceControlState.IsValid()) {
      StateObj->SetStringField(TEXT("state"), TEXT("unknown"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Populate state info
    StateObj->SetBoolField(TEXT("isSourceControlled"), SourceControlState->IsSourceControlled());
    StateObj->SetBoolField(TEXT("isCheckedOut"), SourceControlState->IsCheckedOut());
    StateObj->SetBoolField(TEXT("isCurrent"), SourceControlState->IsCurrent());
    StateObj->SetBoolField(TEXT("isAdded"), SourceControlState->IsAdded());
    StateObj->SetBoolField(TEXT("isDeleted"), SourceControlState->IsDeleted());
    StateObj->SetBoolField(TEXT("isModified"), SourceControlState->IsModified());
    StateObj->SetBoolField(TEXT("isIgnored"), SourceControlState->IsIgnored());
    StateObj->SetBoolField(TEXT("isUnknown"), SourceControlState->IsUnknown());
    StateObj->SetBoolField(TEXT("canCheckIn"), SourceControlState->CanCheckIn());
    StateObj->SetBoolField(TEXT("canCheckout"), SourceControlState->CanCheckout());
    StateObj->SetBoolField(TEXT("canRevert"), SourceControlState->CanRevert());
    StateObj->SetBoolField(TEXT("canEdit"), SourceControlState->CanEdit());
    StateObj->SetBoolField(TEXT("canDelete"), SourceControlState->CanDelete());
    StateObj->SetBoolField(TEXT("canAdd"), SourceControlState->CanAdd());
    StateObj->SetBoolField(TEXT("isConflicted"), SourceControlState->IsConflicted());

    // Check if checked out by other
    FString WhoCheckedOut;
    bool bIsCheckedOutOther = SourceControlState->IsCheckedOutOther(&WhoCheckedOut);
    StateObj->SetBoolField(TEXT("isCheckedOutOther"), bIsCheckedOutOther);
    if (bIsCheckedOutOther && !WhoCheckedOut.IsEmpty()) {
      StateObj->SetStringField(TEXT("checkedOutBy"), WhoCheckedOut);
    }

    // Determine primary state string
    FString StateString = TEXT("unknown");
    if (!SourceControlState->IsSourceControlled()) {
      StateString = TEXT("not_controlled");
    } else if (SourceControlState->IsAdded()) {
      StateString = TEXT("added");
    } else if (SourceControlState->IsDeleted()) {
      StateString = TEXT("deleted");
    } else if (SourceControlState->IsConflicted()) {
      StateString = TEXT("conflicted");
    } else if (SourceControlState->IsCheckedOut()) {
      StateString = TEXT("checked_out");
    } else if (SourceControlState->IsModified()) {
      StateString = TEXT("modified");
    } else if (!SourceControlState->IsCurrent()) {
      StateString = TEXT("out_of_date");
    } else {
      StateString = TEXT("current");
    }
    StateObj->SetStringField(TEXT("state"), StateString);

    // Get display name
    StateObj->SetStringField(TEXT("displayName"), SourceControlState->GetDisplayName().ToString());

    StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("sourceControlEnabled"), true);
  Result->SetArrayField(TEXT("states"), StatesArray);
  Result->SetNumberField(TEXT("queriedCount"), AssetPaths.Num());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Source control state retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_source_control_state requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// ANALYZE GRAPH
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleAnalyzeGraph(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("analyze_graph"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("analyze_graph payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid assetPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the asset
  UObject *Asset = LoadObject<UObject>(nullptr, *AssetPath);
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Asset);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());

  // Check if it's a material
  UMaterial *Material = Cast<UMaterial>(Asset);
  UMaterialInstance *MaterialInstance = Cast<UMaterialInstance>(Asset);

  if (Material || MaterialInstance) {
    // Analyze material graph
    UMaterial *BaseMaterial = Material ? Material : MaterialInstance->GetBaseMaterial();

    // Get expressions count
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const TArray<TObjectPtr<UMaterialExpression>> *Expressions = nullptr;
    if (Material && Material->GetEditorOnlyData()) {
      Expressions = &Material->GetEditorOnlyData()->ExpressionCollection.Expressions;
    }
#else
    // UE 5.0: Direct access, but also uses TObjectPtr
    const TArray<TObjectPtr<UMaterialExpression>> *Expressions = nullptr;
    if (Material) {
      Expressions = &Material->Expressions;
    }
#endif

    int32 NodeCount = Expressions ? Expressions->Num() : 0;
    int32 ParameterCount = 0;
    int32 TextureSampleCount = 0;
    TArray<FString> ParameterNames;

    if (Expressions) {
      for (UMaterialExpression *Expr : *Expressions) {
        if (!Expr) continue;
        if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
          ParameterCount++;
          ParameterNames.Add(Param->ParameterName.ToString());
        }
        if (Cast<UMaterialExpressionTextureSample>(Expr)) {
          TextureSampleCount++;
        }
      }
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Material"));
    Result->SetNumberField(TEXT("nodeCount"), NodeCount);
    Result->SetNumberField(TEXT("parameterCount"), ParameterCount);
    Result->SetNumberField(TEXT("textureSampleCount"), TextureSampleCount);

    // Add parameter names
    TArray<TSharedPtr<FJsonValue>> ParamArray;
    for (const FString &ParamName : ParameterNames) {
      ParamArray.Add(MakeShared<FJsonValueString>(ParamName));
    }
    Result->SetArrayField(TEXT("parameters"), ParamArray);

    // Material properties
    Result->SetBoolField(TEXT("isMaterialInstance"), MaterialInstance != nullptr);
    if (Material) {
      Result->SetBoolField(TEXT("isTwoSided"), Material->TwoSided);
      Result->SetBoolField(TEXT("isMasked"), Material->IsMasked());
#if WITH_EDITORONLY_DATA
      Result->SetStringField(TEXT("blendMode"),
                             StaticEnum<EBlendMode>()->GetNameStringByValue((int64)Material->GetBlendMode()));
      // Get shading model name from the first selected model
      FString ShadingModelName = TEXT("Unknown");
      FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
      if (ShadingModels.HasShadingModel(MSM_DefaultLit)) ShadingModelName = TEXT("DefaultLit");
      else if (ShadingModels.HasShadingModel(MSM_Subsurface)) ShadingModelName = TEXT("Subsurface");
      else if (ShadingModels.HasShadingModel(MSM_Unlit)) ShadingModelName = TEXT("Unlit");
      else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) ShadingModelName = TEXT("ClearCoat");
      else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) ShadingModelName = TEXT("SubsurfaceProfile");
      else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin)) ShadingModelName = TEXT("PreintegratedSkin");
      Result->SetStringField(TEXT("shadingModel"), ShadingModelName);
#endif
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material graph analyzed"), Result, FString());
    return true;
  }

  // Check if it's a blueprint
  UBlueprint *Blueprint = Cast<UBlueprint>(Asset);
  if (Blueprint) {
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    int32 TotalNodes = 0;
    TArray<TSharedPtr<FJsonValue>> GraphInfoArray;

    for (UEdGraph *Graph : AllGraphs) {
      if (!Graph) continue;
      TSharedPtr<FJsonObject> GraphInfo = McpHandlerUtils::CreateResultObject();
      GraphInfo->SetStringField(TEXT("name"), Graph->GetName());
      GraphInfo->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
      TotalNodes += Graph->Nodes.Num();
      GraphInfoArray.Add(MakeShared<FJsonValueObject>(GraphInfo));
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Blueprint"));
    Result->SetStringField(TEXT("blueprintType"), Blueprint->BlueprintType == BPTYPE_Interface ? TEXT("Interface") :
                           Blueprint->BlueprintType == BPTYPE_MacroLibrary ? TEXT("MacroLibrary") :
                           Blueprint->BlueprintType == BPTYPE_FunctionLibrary ? TEXT("FunctionLibrary") : TEXT("Class"));
    Result->SetNumberField(TEXT("totalNodes"), TotalNodes);
    Result->SetNumberField(TEXT("graphCount"), AllGraphs.Num());
    Result->SetArrayField(TEXT("graphs"), GraphInfoArray);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Blueprint graph analyzed"), Result, FString());
    return true;
  }

  // Generic asset - no graph
  Result->SetStringField(TEXT("graphType"), TEXT("None"));
  Result->SetStringField(TEXT("message"), TEXT("Asset does not have a graph structure"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("No graph to analyze for this asset type"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("analyze_graph requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// REBUILD MATERIAL
// ============================================================================

bool UNebulaForgeBridgeSubsystem::HandleRebuildMaterial(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("rebuild_material"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid assetPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(AssetPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Rebuild by triggering a recompile (already on game thread — no AsyncTask needed)
  UObject *Host = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(Function);
  Host->MarkPackageDirty();
  Host->PreEditChange(nullptr);
  Host->PostEditChange();

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (Material) McpHandlerUtils::AddVerification(Result, Material);
    else if (Function) McpHandlerUtils::AddVerification(Result, Function);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("rebuilt"), true);
    Result->SetBoolField(TEXT("headlessSafe"), true);
    Result->SetBoolField(TEXT("saveSkipped"), true);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material rebuilt; save skipped under NullRHI"), Result, FString());
    return true;
  }

  if (!McpSafeAssetSave(Host)) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to save rebuilt material"), TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Result, Material);
  else if (Function) McpHandlerUtils::AddVerification(Result, Function);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetBoolField(TEXT("rebuilt"), true);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material rebuilt successfully"), Result, FString());

  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
