param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
$data = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
$data | Add-Member -Force -NotePropertyName Installed -NotePropertyValue $true
$data | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $Path -Encoding UTF8
