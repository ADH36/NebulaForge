param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [switch]$Engine
)

$ErrorActionPreference = 'Stop'
$data = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json

if ($Engine) {
    '{0}.{1}' -f $data.MajorVersion, $data.MinorVersion
} else {
    $data.VersionName
}
