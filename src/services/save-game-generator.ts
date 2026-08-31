import { writeProjectFile } from './project-file-service.js';

const ALLOWED_TYPES = new Set(['bool', 'int32', 'int64', 'float', 'double', 'FString', 'FName']);
const IDENTIFIER = /^[A-Za-z_][A-Za-z0-9_]*$/;

export interface SaveGameVariable {
  name: string;
  type: string;
  defaultValue?: string | number | boolean;
}

export interface SaveGameMigration {
  fromVersion: number;
  toVersion: number;
  description: string;
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
  schemaVersion?: number;
  migrationManifestPath?: string;
  migrations?: SaveGameMigration[];
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  validateIdentifier(options.className, 'className');
  if (!options.className.startsWith('U')) throw new Error('className must use the Unreal U-prefixed class naming convention.');
  if (!Array.isArray(options.variables) || options.variables.length > 128) throw new Error('variables must be an array with at most 128 entries.');
  const seen = new Set<string>();
  const declarations: string[] = [];
  const schemaVersion = options.schemaVersion ?? 1;
  if (!Number.isInteger(schemaVersion) || schemaVersion < 1 || schemaVersion > 1000000) throw new Error('schemaVersion must be an integer between 1 and 1000000.');
  if (seen.has('SaveSchemaVersion')) throw new Error('SaveSchemaVersion is reserved.');
  const migrations = options.migrations ?? [];
  if (!Array.isArray(migrations) || migrations.length > 128) throw new Error('migrations must be an array with at most 128 entries.');
  const migrationPairs = new Set<string>();
  for (const migration of migrations) {
    if (!migration || !Number.isInteger(migration.fromVersion) || !Number.isInteger(migration.toVersion) || migration.fromVersion < 1 || migration.fromVersion >= migration.toVersion || migration.toVersion > schemaVersion || typeof migration.description !== 'string' || migration.description.trim().length === 0 || migration.description.length > 1024) {
      throw new Error('Each migration requires integer fromVersion < toVersion <= schemaVersion and a bounded description.');
    }
    const pair = `${migration.fromVersion}->${migration.toVersion}`;
    if (migrationPairs.has(pair)) throw new Error(`Duplicate SaveGame migration: ${pair}`);
    migrationPairs.add(pair);
  }
  if (options.migrationManifestPath !== undefined && typeof options.migrationManifestPath !== 'string') throw new Error('migrationManifestPath must be a project-relative path.');
  const hasSchemaManifest = options.migrationManifestPath !== undefined || options.schemaVersion !== undefined || options.migrations !== undefined;
  if (hasSchemaManifest) declarations.push('\tUPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save Game")\n\tint32 SaveSchemaVersion = ' + String(schemaVersion) + ';');
  for (const variable of options.variables) {
    if (!variable || typeof variable.name !== 'string' || typeof variable.type !== 'string') throw new Error('Each SaveGame variable requires name and type.');
    validateIdentifier(variable.name, 'variable name');
    if (variable.name === 'SaveSchemaVersion') throw new Error('SaveSchemaVersion is reserved.');
    if (seen.has(variable.name)) throw new Error(`Duplicate SaveGame variable: ${variable.name}`);
    seen.add(variable.name);
    if (!ALLOWED_TYPES.has(variable.type)) throw new Error(`Unsupported SaveGame property type: ${variable.type}`);
    declarations.push(`\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Game")\n\t${variable.type} ${variable.name}${cppDefault(variable.defaultValue, variable.type)};`);
  }

  const header = `#pragma once\n\n#include "CoreMinimal.h"\n#include "GameFramework/SaveGame.h"\n#include "${options.className}.generated.h"\n\nUCLASS(BlueprintType)\nclass ${options.className} : public USaveGame\n{\n\tGENERATED_BODY()\n\npublic:\n${declarations.join('\n\n')}\n};\n`;
  const source = `#include "${options.className}.h"\n`;
  const headerResult = await writeProjectFile(options.projectPath, options.headerPath, header, options.backup !== false);
  const sourceResult = await writeProjectFile(options.projectPath, options.sourcePath, source, options.backup !== false);
  let migrationManifest: Record<string, unknown> | undefined;
  if (hasSchemaManifest) {
    const manifestPath = options.migrationManifestPath ?? `Config/SaveGame/${options.className}.schema.json`;
    const manifest = {
      schemaVersion,
      className: options.className,
      variables: options.variables.map(variable => ({ name: variable.name, type: variable.type, defaultValue: variable.defaultValue })),
      migrations: migrations.map(migration => ({ ...migration, description: migration.description.trim() }))
    };
    const manifestResult = await writeProjectFile(options.projectPath, manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, options.backup !== false);
    migrationManifest = { path: manifestPath, ...manifestResult, schemaVersion, migrationCount: migrations.length };
  }
  return { success: true, className: options.className, header: headerResult, source: sourceResult, variableCount: options.variables.length, schemaVersion: hasSchemaManifest ? schemaVersion : undefined, migrationManifest };
}
