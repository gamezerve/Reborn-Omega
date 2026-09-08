
#PrepareRebornOmegaData.ps1

param(
    [string]$DisplayName
)

$Source = "D:\OneDrive\Documents\GitHub\Reborn-Omega\build\shared"

$Version = $DisplayName -replace '^Reborn Omega ', ''
$Target = "D:\TEMP\Zero Hour Reborn Omega v$Version"

Write-Host "DisplayName: $DisplayName"
Write-Host "Source: $Source"
Write-Host "Target: $Target"

# Prepare The Target Folder ------------------------------------------------

if (Test-Path $Target)
{
    Write-Host "Removing existing target directory..."
    Remove-Item $Target -Recurse -Force
}

Write-Host "Creating target directory..."
New-Item -ItemType Directory -Force -Path $Target | Out-Null

# Create RebornOmega_Art.pak -----------------------------------------------

Write-Host "Creating RebornOmega_Art.pak..."

$MakeBig = "$PSScriptRoot\..\tools\MakeBig.exe"
$BigTemp = "$Target\_bigtemp"

New-Item -ItemType Directory -Force -Path "$BigTemp\Art" | Out-Null

robocopy `
    "$Source\Art" `
    "$BigTemp\Art" `
    /E `
    /NFL `
    /XD `
    "$Source\Art\Textures\Buff-Nerf Icons" `
    "$Source\Art\Textures\ModDB" `
    "$Source\Art\Textures\Discord Server" `
    "$Source\Art\Textures\User Interface"

if ($LASTEXITCODE -ge 8)
{
    throw "Robocopy failed while preparing RebornOmega_Art.pak with exit code $LASTEXITCODE"
}

& $MakeBig `
    -f "$BigTemp" `
    -o:"$Target\RebornOmega_Art.pak"

if ($LASTEXITCODE -ne 0)
{
    throw "MakeBig failed while creating RebornOmega_Art.pak with exit code $LASTEXITCODE"
}

Remove-Item $BigTemp -Recurse -Force

Write-Host "RebornOmega_Art.pak created successfully."

# Copy Data Folder ---------------------------------------------------------

Write-Host "Copying Data..."

robocopy `
    "$Source\Data" `
    "$Target\Data" `
    /E `
    /NFL `
    /XD `
    "$Source\Data\Editor" `
    "$Source\Data\English\Movies" `
    /XF `
    "AI Mod Changelog.txt" `
    "AI Mod Changelog.txt.bak" `
    "GC_Background.bik" `
    "VS_small.bik" `
    "*.ani"

Write-Host "Copying selected loose cursors..."

New-Item -ItemType Directory -Force -Path "$Target\Data\Cursors" | Out-Null

Copy-Item "$Source\Data\Cursors\SCCCantPower.ani" "$Target\Data\Cursors\" -Force
Copy-Item "$Source\Data\Cursors\SCCPowerMode.ani" "$Target\Data\Cursors\" -Force

Write-Host "Selected loose cursors copied successfully."

if ($LASTEXITCODE -ge 8)
{
    throw "Robocopy failed while copying Data with exit code $LASTEXITCODE"
}

Write-Host "Data copied successfully."

# Create RebornOmega_Data.pak ----------------------------------------------

Write-Host "Creating RebornOmega_Data.pak..."

$DataBigTemp = "$Target\_databigtemp"

New-Item -ItemType Directory -Force -Path "$DataBigTemp\Data" | Out-Null

robocopy "$Target\Data\Audio" "$DataBigTemp\Data\Audio" /E /NFL
robocopy "$Target\Data\English" "$DataBigTemp\Data\English" /E /NFL
robocopy "$Target\Data\INI" "$DataBigTemp\Data\INI" /E /NFL
robocopy "$Target\Data\INIold" "$DataBigTemp\Data\INIold" /E /NFL
New-Item -ItemType Directory -Force -Path "$DataBigTemp\Data\Scripts" | Out-Null
robocopy `
    "$Target\Data\Scripts" `
    "$DataBigTemp\Data\Scripts" `
    /E `
    /NFL `
    /XF `
    "SkirmishScripts.scb" `
    "MultiplayerScripts.scb"
robocopy "$Target\Data\WaterPlane" "$DataBigTemp\Data\WaterPlane" /E /NFL

Copy-Item `
    "$Target\Data\Generals.str" `
    "$DataBigTemp\Data\Generals.str" `
    -Force

& $MakeBig `
    -f "$DataBigTemp" `
    -o:"$Target\RebornOmega_Data.pak"

if ($LASTEXITCODE -ne 0)
{
    throw "MakeBig failed while creating RebornOmega_Data.pak with exit code $LASTEXITCODE"
}

Remove-Item $DataBigTemp -Recurse -Force

Write-Host "RebornOmega_Data.pak created successfully."

Write-Host "Removing packed Data folders..."

Remove-Item "$Target\Data\Audio" -Recurse -Force
Remove-Item "$Target\Data\English" -Recurse -Force
Remove-Item "$Target\Data\INI" -Recurse -Force
Remove-Item "$Target\Data\INIold" -Recurse -Force
Get-ChildItem "$Target\Data\Scripts" -Recurse | Where-Object {
    $_.Name -ne "SkirmishScripts.scb" -and
    $_.Name -ne "MultiplayerScripts.scb"
} | Remove-Item -Force
Remove-Item "$Target\Data\WaterPlane" -Recurse -Force
Remove-Item "$Target\Data\Generals.str" -Force

Write-Host "Packed Data folders removed."

# Create RebornOmega_Maps.pak ----------------------------------------------

Write-Host "Creating RebornOmega_Maps.pak..."

$MapsBigTemp = "$Target\_mapsbigtemp"

New-Item -ItemType Directory -Force -Path "$MapsBigTemp\Maps" | Out-Null

# Copy MapPreviews

robocopy `
    "$Source\MapPreviews" `
    "$MapsBigTemp\MapPreviews" `
    /E `
    /NFL

# Copy Maps

$MapFolders = @(
    "Alpine Assault",
    "CHI01",
    "CHI02",
    "CHI03",
    "CHI04",
    "CHI05",
    "CHI06",
    "CHI07",
    "GLA01",
    "GLA02",
    "GLA03",
    "GLA04",
    "GLA05",
    "GLA06",
    "GLA07",
    "GLA08",
    "MD_CHI01",
    "ShellMap1",
    "ShellMapMD",
    "Training01"
)

foreach ($MapFolder in $MapFolders)
{
    Write-Host "Adding map: $MapFolder"

    robocopy `
        "$Source\Maps\$MapFolder" `
        "$MapsBigTemp\Maps\$MapFolder" `
        /E `
        /NFL

    if ($LASTEXITCODE -ge 8)
    {
        throw "Robocopy failed while copying map $MapFolder with exit code $LASTEXITCODE"
    }
}

# Build pak

& $MakeBig `
    -f "$MapsBigTemp" `
    -o:"$Target\RebornOmega_Maps.pak"

if ($LASTEXITCODE -ne 0)
{
    throw "MakeBig failed while creating RebornOmega_Maps.pak with exit code $LASTEXITCODE"
}

Remove-Item $MapsBigTemp -Recurse -Force

Write-Host "RebornOmega_Maps.pak created successfully."

# Create RebornOmega_Window.pak --------------------------------------------

Write-Host "Creating RebornOmega_Window.pak..."

$WindowBigTemp = "$Target\_windowbigtemp"

New-Item -ItemType Directory -Force -Path "$WindowBigTemp\Window" | Out-Null

robocopy `
    "$Source\Window" `
    "$WindowBigTemp\Window" `
    /E `
    /NFL

if ($LASTEXITCODE -ge 8)
{
    throw "Robocopy failed while preparing RebornOmega_Window.pak with exit code $LASTEXITCODE"
}

& $MakeBig `
    -f "$WindowBigTemp" `
    -o:"$Target\RebornOmega_Window.pak"

if ($LASTEXITCODE -ne 0)
{
    throw "MakeBig failed while creating RebornOmega_Window.pak with exit code $LASTEXITCODE"
}

Remove-Item $WindowBigTemp -Recurse -Force

Write-Host "RebornOmega_Window.pak created successfully."

# Copy Install_Final_RO.bmp -----------------------------------------------

Write-Host "Copying Install_Final_RO.bmp..."

Copy-Item `
    "$Source\RebornOmegaData\Install_Final_RO.bmp" `
    "$Target\Install_Final_RO.bmp" `
    -Force

Write-Host "Install_Final_RO.bmp copied successfully."

# Copy Executables ---------------------------------------------------------

$ReleaseDir = "D:\OneDrive\Documents\GitHub\Reborn-Omega\build\win32-reborn\GeneralsMD\Release"

$Executables = @(
    "$DisplayName.exe",
    "$DisplayName WorldBuilder.exe"
)

foreach ($Exe in $Executables)
{
    $ExeSource = Join-Path $ReleaseDir $Exe

    Write-Host "Copying $Exe..."

    if (!(Test-Path $ExeSource))
    {
        throw "Executable not found: $ExeSource"
    }

    Copy-Item $ExeSource "$Target\$Exe" -Force

    Write-Host "$Exe copied successfully."
}

# Copy Dependencies Installer ----------------------------------------------

$VCRedistSource = "$PSScriptRoot\..\tools\VC_redist.x86.exe"
$ToolsDir = "$Target\Tools"

Write-Host "Copying VC_redist.x86.exe..."

if (!(Test-Path $VCRedistSource))
{
    throw "VC_redist.x86.exe not found: $VCRedistSource"
}

New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null
Copy-Item $VCRedistSource "$ToolsDir\VC_redist.x86.exe" -Force

Write-Host "VC_redist.x86.exe copied successfully."

# Convert BIG4 archives to BIGR --------------------------------------------

Write-Host "Converting BIG4 archives to BIGR..."

Get-ChildItem $Target -Filter *.pak | ForEach-Object {

    $Bytes = [System.IO.File]::ReadAllBytes($_.FullName)

    if ($Bytes[0] -eq 0x42 -and
        $Bytes[1] -eq 0x49 -and
        $Bytes[2] -eq 0x47 -and
        $Bytes[3] -eq 0x34)
    {
        $Bytes[3] = 0x52
        [System.IO.File]::WriteAllBytes($_.FullName, $Bytes)

        Write-Host "Converted $($_.Name)"
    }
}

Write-Host "Archive conversion complete."

# Move Files Into RebornOmegaData ------------------------------------------

Write-Host "Creating RebornOmegaData folder..."

$RebornOmegaDataDir = "$Target\RebornOmegaData"

New-Item -ItemType Directory -Force -Path $RebornOmegaDataDir | Out-Null

# Reborn: Copy the private GO runtime from the build output into RebornOmegaData.
$GeneralsOnlineRuntimeDir = Join-Path $ReleaseDir "RebornOmegaData"
if (!(Test-Path $GeneralsOnlineRuntimeDir))
{
    throw "Generals Online runtime directory not found: $GeneralsOnlineRuntimeDir"
}

Copy-Item "$GeneralsOnlineRuntimeDir\*.dll" $RebornOmegaDataDir -Force

Write-Host "Moving files into RebornOmegaData..."

Get-ChildItem $Target | Where-Object {
    $_.Name -ne "RebornOmegaData" -and
    $_.Extension -ne ".exe"
} | Move-Item -Destination $RebornOmegaDataDir -Force

Write-Host "RebornOmegaData folder prepared successfully."

# End Job ------------------------------------------------------------------

Write-Host "PrepareRebornOmegaData Done."
