import path from 'node:path';
import { writeProjectFile } from './project-file-service.js';

const IDENTIFIER = /^[A-Za-z_][A-Za-z0-9_]*$/;
const TEST_NAME = /^[A-Za-z][A-Za-z0-9_.-]{2,127}$/;

function validateIdentifier(value: string, field: string): void {
  if (!IDENTIFIER.test(value)) throw new Error(`${field} must be a valid C++ identifier.`);
}

export async function generateAutomationTest(options: {
  projectPath?: string;
  className: string;
  testName: string;
  headerPath: string;
  sourcePath: string;
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  validateIdentifier(options.className, 'className');
  if (!options.className.startsWith('F')) throw new Error('className must use the F-prefixed automation-test naming convention.');
  if (!TEST_NAME.test(options.testName)) throw new Error('testName must be a 3-128 character dot-separated automation test name.');
  if (!options.headerPath.trim() || !options.sourcePath.trim()) throw new Error('headerPath and sourcePath are required.');
  if (path.normalize(options.headerPath) === path.normalize(options.sourcePath)) throw new Error('headerPath and sourcePath must be different files.');

  const headerName = path.basename(options.headerPath).replace(/\.h$/i, '');
  if (!IDENTIFIER.test(headerName)) throw new Error('headerPath must end in a header filename with a valid C++ identifier.');
  const header = '#pragma once\n\n#include "CoreMinimal.h"\n#include "Misc/AutomationTest.h"\n';
  const source = `#include "${headerName}.h"\n\nIMPLEMENT_SIMPLE_AUTOMATION_TEST(${options.className}, "${options.testName}", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)\n\nbool ${options.className}::RunTest(const FString& Parameters)\n{\n\tTestTrue(TEXT("Generated automation test is wired"), true);\n\treturn !HasAnyErrors();\n}\n`;
  const headerResult = await writeProjectFile(options.projectPath, options.headerPath, header, options.backup !== false);
  const sourceResult = await writeProjectFile(options.projectPath, options.sourcePath, source, options.backup !== false);
  return { success: true, className: options.className, testName: options.testName, header: headerResult, source: sourceResult };
}
