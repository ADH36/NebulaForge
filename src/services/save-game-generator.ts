import { writeProjectFile } from './project-file-service.js';

const ALLOWED_TYPES = new Set(['bool', 'int32', 'int64', 'float', 'double', 'FString', 'FName']);
const IDENTIFIER = /^[A-Za-z_][A-Za-z0-9_]*$/;

export interface SaveGameVariable {
  name: string;
  type: string;
  defaultValue?: string | number | boolean;
}

function validateIdentifier(value: string, field: string): void {
  if (!IDENTIFIER.test(value)) {
    throw new Error(`${field} must be a valid C++ identifier.`);
  }
}

function cppDefault(value: string | number | boolean | undefined, type: string): string {
  if (value === undefined) return '';
  if (type === 'bool') return value === true || value === 'true' ? ' = true' : ' = false';
  if (type === 'FString') return ` = TEXT("${String(value).replace(/"/g, '\\"')}")`;
  if (type === 'FName') return ` = FName(TEXT("${String(value).replace(/"/g, '\\"')}"))`;
  if (!/^-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/.test(String(value))) {
    throw new Error(`Invalid numeric default value for ${type}.`);
  }
  return ` = ${String(value)}`;
}

export async function generateSaveGameClass(options: {
  projectPath?: string;
  className: string;
  headerPath: string;
  sourcePath: string;
  variables: SaveGameVariable[];
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  validateIdentifier(options.className, 'className');
  if (!options.className.startsWith('U')) throw new Error('className must use the Unreal U-prefixed class naming convention.');
  if (!Array.isArray(options.variables) || options.variables.length > 128) throw new Error('variables must be an array with at most 128 entries.');
  const seen = new Set<string>();
  const declarations: string[] = [];
  for (const variable of options.variables) {
    if (!variable || typeof variable.name !== 'string' || typeof variable.type !== 'string') throw new Error('Each SaveGame variable requires name and type.');
    validateIdentifier(variable.name, 'variable name');
    if (seen.has(variable.name)) throw new Error(`Duplicate SaveGame variable: ${variable.name}`);
    seen.add(variable.name);
    if (!ALLOWED_TYPES.has(variable.type)) throw new Error(`Unsupported SaveGame property type: ${variable.type}`);
    declarations.push(`\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Game")\n\t${variable.type} ${variable.name}${cppDefault(variable.defaultValue, variable.type)};`);
  }

  const header = `#pragma once\n\n#include "CoreMinimal.h"\n#include "GameFramework/SaveGame.h"\n#include "${options.className}.generated.h"\n\nUCLASS(BlueprintType)\nclass ${options.className} : public USaveGame\n{\n\tGENERATED_BODY()\n\npublic:\n${declarations.join('\n\n')}\n};\n`;
  const source = `#include "${options.className}.h"\n`;
  const headerResult = await writeProjectFile(options.projectPath, options.headerPath, header, options.backup !== false);
  const sourceResult = await writeProjectFile(options.projectPath, options.sourcePath, source, options.backup !== false);
  return { success: true, className: options.className, header: headerResult, source: sourceResult, variableCount: options.variables.length };
}
