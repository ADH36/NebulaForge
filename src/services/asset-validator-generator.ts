import path from 'node:path';
import { writeProjectFile } from './project-file-service.js';

const IDENTIFIER = /^[A-Za-z_][A-Za-z0-9_]*$/;

function validateIdentifier(value: string, field: string): void {
  if (!IDENTIFIER.test(value)) throw new Error(`${field} must be a valid C++ identifier.`);
}

export async function generateAssetValidator(options: {
  projectPath?: string;
  className: string;
  headerPath: string;
  sourcePath: string;
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  validateIdentifier(options.className, 'className');
  if (!options.className.startsWith('U')) throw new Error('className must use the U-prefixed UObject naming convention.');
  if (!options.headerPath.trim() || !options.sourcePath.trim()) throw new Error('headerPath and sourcePath are required.');
  if (path.normalize(options.headerPath) === path.normalize(options.sourcePath)) throw new Error('headerPath and sourcePath must be different files.');
  const headerName = path.basename(options.headerPath).replace(/\.h$/i, '');
  if (!IDENTIFIER.test(headerName)) throw new Error('headerPath must end in a header filename with a valid C++ identifier.');

  const header = `#pragma once\n\n#include "CoreMinimal.h"\n#include "EditorValidatorBase.h"\n#include "${headerName}.generated.h"\n\nUCLASS()\nclass ${options.className} : public UEditorValidatorBase\n{\n\tGENERATED_BODY()\n\nprotected:\n\tvirtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;\n\tvirtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& Context) override;\n};\n`;
  const source = `#include "${headerName}.h"\n\n#include "Misc/DataValidation.h"\n\n\nbool ${options.className}::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const\n{\n\treturn InObject != nullptr;\n}\n\nEDataValidationResult ${options.className}::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& Context)\n{\n\tAssetPasses(InObject);\n\treturn GetValidationResult();\n}\n`;
  const headerResult = await writeProjectFile(options.projectPath, options.headerPath, header, options.backup !== false);
  const sourceResult = await writeProjectFile(options.projectPath, options.sourcePath, source, options.backup !== false);
  return { success: true, className: options.className, header: headerResult, source: sourceResult };
}
