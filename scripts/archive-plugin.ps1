param(
    [Parameter(Mandatory = $true)]
    [string]$PluginDirectory,

    [Parameter(Mandatory = $true)]
    [string]$ZipPath
)

$ErrorActionPreference = 'Stop'
Get-ChildItem -LiteralPath $PluginDirectory -Recurse -Filter '*.pdb' | Remove-Item -Force
Push-Location (Split-Path -Parent $PluginDirectory)
try {
    Compress-Archive -LiteralPath (Split-Path -Leaf $PluginDirectory) -DestinationPath $ZipPath -Force
} finally {
    Pop-Location
}
