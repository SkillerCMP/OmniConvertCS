[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$BuildDir = 'build\Win7-x64',

    [string]$DistDir = '',

    [ValidateSet('auto', 'v143', 'v142', 'v141')]
    [string]$Toolset = 'auto',

    [string]$VisualStudioPath = $env:OMNI_VS_PATH,

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$IdentityPath = Join-Path $Root 'cmake\OmniIdentity.cmake'
$IdentityText = Get-Content $IdentityPath -Raw
if ($IdentityText -notmatch 'set\(OMNI_PRODUCT_NAME "([^"]+)"\)') {
    throw 'Could not read OMNI_PRODUCT_NAME from cmake\OmniIdentity.cmake.'
}
$ProductName = $Matches[1]
if ($IdentityText -notmatch 'set\(OMNI_EXECUTABLE_NAME "([^"]+)"\)') {
    throw 'Could not read OMNI_EXECUTABLE_NAME from cmake\OmniIdentity.cmake.'
}
$ExecutableName = $Matches[1]
if ($IdentityText -notmatch 'set\(OMNI_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"\)') {
    throw 'Could not read OMNI_VERSION from cmake\OmniIdentity.cmake.'
}
$Version = $Matches[1]
$ExecutableFileName = "$ExecutableName.exe"

if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path '0-Finished\dist\Win7-x64' $Configuration
}

$BuildDir = [System.IO.Path]::GetFullPath((Join-Path $Root $BuildDir))
$DistDir = [System.IO.Path]::GetFullPath((Join-Path $Root $DistDir))
$LogDir = Join-Path $Root '0-Finished\logs'
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$Timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$LogPath = Join-Path $LogDir "build-win7-$Timestamp.log"
$SummaryPath = Join-Path $LogDir "build-win7-$Timestamp-warnings-errors.log"

function Write-Section([string]$Text) {
    Write-Host ''
    Write-Host ('=' * 72)
    Write-Host $Text
    Write-Host ('=' * 72)
}

function Repair-FutureProjectTimestamps {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string[]]$ExcludedDirectories
    )

    # ZIP archives do not store a time-zone offset. Files created in another
    # time zone can therefore be extracted with timestamps several hours in
    # the future. CMake then treats CMakeLists.txt as permanently newer than
    # its generated dependency stamp and may start overlapping regeneration
    # processes during a parallel Visual Studio build.
    $NowUtc = [DateTime]::UtcNow
    $FutureCutoffUtc = $NowUtc.AddSeconds(2)
    $ExcludedPaths = @()

    foreach ($Directory in $ExcludedDirectories) {
        $ExcludedPaths += [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $Directory))
    }

    $Adjusted = 0
    $Files = Get-ChildItem -LiteralPath $ProjectRoot -Recurse -File -Force -ErrorAction SilentlyContinue
    foreach ($File in $Files) {
        $Skip = $false
        foreach ($ExcludedPath in $ExcludedPaths) {
            if ($File.FullName.StartsWith($ExcludedPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                $Skip = $true
                break
            }
        }

        if (-not $Skip -and $File.LastWriteTimeUtc -gt $FutureCutoffUtc) {
            $File.LastWriteTimeUtc = $NowUtc
            $Adjusted++
        }
    }

    if ($Adjusted -gt 0) {
        Write-Host "Normalized $Adjusted project file timestamp(s) that were in the future."
    }
}

function Invoke-LoggedNative {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Description
    )

    Write-Host "> $Description"
    Write-Host ("  {0} {1}" -f $FilePath, ($Arguments -join ' '))
    & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $LogPath -Append
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne 0) {
        throw "$Description failed with exit code $ExitCode."
    }
}

function Get-GeneratorName([int]$VisualStudioMajor) {
    switch ($VisualStudioMajor) {
        15 { return 'Visual Studio 15 2017' }
        16 { return 'Visual Studio 16 2019' }
        17 { return 'Visual Studio 17 2022' }
        18 { return 'Visual Studio 18 2026' }
        default { return $null }
    }
}

function Get-ToolsetMoniker([version]$Version) {
    if ($Version.Major -ne 14) {
        return $null
    }
    if ($Version.Minor -ge 30 -and $Version.Minor -lt 50) {
        return 'v143'
    }
    if ($Version.Minor -ge 20 -and $Version.Minor -lt 30) {
        return 'v142'
    }
    if ($Version.Minor -ge 10 -and $Version.Minor -lt 20) {
        return 'v141'
    }
    return $null
}

function Get-ToolsetRank([string]$Moniker) {
    switch ($Moniker) {
        'v143' { return 3 }
        'v142' { return 2 }
        'v141' { return 1 }
        default { return 0 }
    }
}

function Get-MinimumVisualStudioMajor([string]$Moniker) {
    switch ($Moniker) {
        'v143' { return 17 }
        'v142' { return 16 }
        'v141' { return 15 }
        default { return 999 }
    }
}

function Get-ObjectPropertyValue {
    param(
        [Parameter(Mandatory = $true)][object]$InputObject,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $Property = $InputObject.PSObject.Properties[$Name]
    if ($null -eq $Property) {
        return $null
    }
    return $Property.Value
}

function ConvertFrom-VsWhereJson {
    param([Parameter(Mandatory = $true)][string[]]$JsonLines)

    # Windows PowerShell 5.1 and different vswhere releases do not always
    # expose JSON arrays through the pipeline in the same shape. Parse the
    # complete JSON document once, then flatten nested arrays explicitly.
    $JsonText = (($JsonLines | ForEach-Object { [string]$_ }) -join [Environment]::NewLine).Trim()
    if ([string]::IsNullOrWhiteSpace($JsonText)) {
        return @()
    }

    try {
        $Parsed = ConvertFrom-Json -InputObject $JsonText
    }
    catch {
        throw "Could not parse vswhere JSON output: $($_.Exception.Message)"
    }

    $Results = New-Object 'System.Collections.Generic.List[object]'
    $Visit = $null
    $Visit = {
        param([object]$Item)

        if ($null -eq $Item) {
            return
        }

        if ($Item -is [System.Array]) {
            foreach ($Child in $Item) {
                & $Visit $Child
            }
            return
        }

        $InstallPath = Get-ObjectPropertyValue -InputObject $Item -Name 'installationPath'
        if (-not [string]::IsNullOrWhiteSpace([string]$InstallPath)) {
            $Results.Add($Item)
            return
        }

        # Tolerate an additional enumerable wrapper while never treating a
        # string or dictionary as a collection of Visual Studio instances.
        if (($Item -is [System.Collections.IEnumerable]) -and
            -not ($Item -is [string]) -and
            -not ($Item -is [System.Collections.IDictionary])) {
            foreach ($Child in $Item) {
                & $Visit $Child
            }
        }
    }

    & $Visit $Parsed
    return @($Results.ToArray())
}

function Get-InferredVisualStudioMajor {
    param(
        [string]$InstallPath,
        [string]$InstallationVersionText
    )

    $ParsedVersion = $null
    if (-not [string]::IsNullOrWhiteSpace($InstallationVersionText) -and
        [version]::TryParse($InstallationVersionText, [ref]$ParsedVersion)) {
        return $ParsedVersion.Major
    }

    if ($InstallPath -match '(?i)[\\/](18|2026)[\\/]') { return 18 }
    if ($InstallPath -match '(?i)[\\/]2022[\\/]') { return 17 }
    if ($InstallPath -match '(?i)[\\/]2019[\\/]') { return 16 }
    if ($InstallPath -match '(?i)[\\/]2017[\\/]') { return 15 }
    return 0
}

function Get-ManualVisualStudioPaths {
    $Roots = @($env:ProgramFiles, ${env:ProgramFiles(x86)}) |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Select-Object -Unique
    $Versions = @('18', '2026', '2022', '2019', '2017')
    $Editions = @('Insiders', 'Enterprise', 'Professional', 'Community', 'BuildTools')

    $Paths = New-Object 'System.Collections.Generic.List[string]'
    foreach ($Base in $Roots) {
        foreach ($Version in $Versions) {
            foreach ($Edition in $Editions) {
                $Candidate = Join-Path $Base "Microsoft Visual Studio\$Version\$Edition"
                if ((Test-Path (Join-Path $Candidate 'VC\Tools\MSVC')) -or
                    (Test-Path (Join-Path $Candidate 'Common7\Tools\VsDevCmd.bat'))) {
                    $Paths.Add($Candidate)
                }
            }
        }
    }
    return @($Paths.ToArray())
}

function Find-CompatibleVisualStudio {
    param(
        [Parameter(Mandatory = $true)][string]$CMakePath,
        [Parameter(Mandatory = $true)][string]$RequestedToolset,
        [string]$RequestedInstallPath
    )

    $InstallRecords = New-Object 'System.Collections.Generic.List[object]'
    $SeenPaths = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)

    function Add-InstallRecord {
        param(
            [string]$Path,
            [string]$DisplayName,
            [string]$InstallationVersionText,
            [string]$Source
        )

        if ([string]::IsNullOrWhiteSpace($Path)) {
            return
        }
        try {
            $FullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
        }
        catch {
            return
        }
        if (-not (Test-Path $FullPath)) {
            return
        }
        if (-not $SeenPaths.Add($FullPath)) {
            return
        }

        if ([string]::IsNullOrWhiteSpace($DisplayName)) {
            $DisplayName = "Visual Studio ($Source)"
        }
        $InstallRecords.Add([pscustomobject]@{
            InstallPath = $FullPath
            DisplayName = $DisplayName
            InstallationVersionText = $InstallationVersionText
            Source = $Source
        })
    }

    if (-not [string]::IsNullOrWhiteSpace($RequestedInstallPath)) {
        Add-InstallRecord -Path $RequestedInstallPath -DisplayName 'Requested Visual Studio' `
            -InstallationVersionText '' -Source 'OMNI_VS_PATH'
    }

    $VsWhereCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    )
    $VsWhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
    if ($VsWhereCommand) {
        $VsWhereCandidates += $VsWhereCommand.Source
    }
    $VsWhere = $VsWhereCandidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1

    if (-not [string]::IsNullOrWhiteSpace($VsWhere)) {
        # Do not filter with "-requires" here. Some prerelease/Insiders and
        # older installer records omit workload metadata even though cl.exe is
        # installed. Enumerate everything, then verify the actual compiler and
        # dumpbin files on disk, like the known-good native Xploder builder.
        $RawJson = & $VsWhere -all -prerelease -products * -format json -utf8
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "vswhere.exe failed with exit code $LASTEXITCODE; trying known installation folders."
        }
        else {
            $VsWhereInstallations = @(ConvertFrom-VsWhereJson -JsonLines $RawJson)
            Write-Host "vswhere installation records found: $($VsWhereInstallations.Count)"
            foreach ($Installation in $VsWhereInstallations) {
                $Path = [string](Get-ObjectPropertyValue -InputObject $Installation -Name 'installationPath')
                $Name = [string](Get-ObjectPropertyValue -InputObject $Installation -Name 'displayName')
                $VersionText = [string](Get-ObjectPropertyValue -InputObject $Installation -Name 'installationVersion')
                Add-InstallRecord -Path $Path -DisplayName $Name `
                    -InstallationVersionText $VersionText -Source 'vswhere'
            }
        }
    }
    else {
        Write-Warning 'vswhere.exe was not found; trying known Visual Studio installation folders.'
    }

    foreach ($ManualPath in (Get-ManualVisualStudioPaths)) {
        Add-InstallRecord -Path $ManualPath -DisplayName "Visual Studio at $ManualPath" `
            -InstallationVersionText '' -Source 'known folder'
    }

    if ($InstallRecords.Count -eq 0) {
        throw 'No Visual Studio or Build Tools installation was found. Install Visual Studio with Desktop development with C++, or set OMNI_VS_PATH to the installation folder.'
    }

    Write-Host "Visual Studio installation folders to inspect: $($InstallRecords.Count)"
    $CMakeHelp = (& $CMakePath --help 2>&1 | Out-String)
    $Candidates = @()
    $DetectedToolsets = New-Object 'System.Collections.Generic.List[string]'

    foreach ($Installation in $InstallRecords) {
        $InstallPath = $Installation.InstallPath
        $VsMajor = Get-InferredVisualStudioMajor `
            -InstallPath $InstallPath `
            -InstallationVersionText $Installation.InstallationVersionText
        $Generator = Get-GeneratorName $VsMajor
        $ToolRoot = Join-Path $InstallPath 'VC\Tools\MSVC'

        if (-not (Test-Path $ToolRoot)) {
            Write-Host "Skipping (no VC\Tools\MSVC): $InstallPath"
            continue
        }

        foreach ($Directory in (Get-ChildItem -Path $ToolRoot -Directory -ErrorAction SilentlyContinue)) {
            $ParsedToolVersion = $null
            if (-not [version]::TryParse($Directory.Name, [ref]$ParsedToolVersion)) {
                continue
            }

            $Cl = Join-Path $Directory.FullName 'bin\Hostx64\x64\cl.exe'
            $Dumpbin = Join-Path $Directory.FullName 'bin\Hostx64\x64\dumpbin.exe'
            if (-not (Test-Path $Cl) -or -not (Test-Path $Dumpbin)) {
                continue
            }

            $Moniker = Get-ToolsetMoniker $ParsedToolVersion
            $DetectedToolsets.Add("$($Directory.Name) at $InstallPath")
            if ([string]::IsNullOrWhiteSpace($Moniker)) {
                continue
            }
            if ($RequestedToolset -ne 'auto' -and $Moniker -ne $RequestedToolset) {
                continue
            }
            if ($VsMajor -lt (Get-MinimumVisualStudioMajor $Moniker)) {
                continue
            }
            if ([string]::IsNullOrWhiteSpace($Generator) -or
                $CMakeHelp -notmatch [regex]::Escape($Generator)) {
                continue
            }

            $InstallationVersionObject = [version]'0.0'
            [void][version]::TryParse($Installation.InstallationVersionText, [ref]$InstallationVersionObject)
            $Candidates += [pscustomobject]@{
                InstallPath = $InstallPath
                DisplayName = $Installation.DisplayName
                InstallationVersion = $InstallationVersionObject
                VsMajor = $VsMajor
                Generator = $Generator
                Toolset = $Moniker
                ToolsetRank = Get-ToolsetRank $Moniker
                ToolsetVersion = $Directory.Name
                ToolsetVersionObject = $ParsedToolVersion
                ToolsetArgument = "$Moniker,version=$($Directory.Name)"
                Cl = $Cl
                Dumpbin = $Dumpbin
                Source = $Installation.Source
            }
        }
    }

    if ($Candidates.Count -eq 0) {
        $RequestedText = if ($RequestedToolset -eq 'auto') { 'v141, v142, or v143' } else { $RequestedToolset }
        $FoundText = if ($DetectedToolsets.Count -gt 0) {
            ($DetectedToolsets | Sort-Object -Unique) -join [Environment]::NewLine
        }
        else {
            '(no x64 cl.exe/dumpbin.exe pairs found)'
        }
        throw ("Visual Studio was found, but no compatible Windows 7 C++ toolset was available. " +
            "Install $RequestedText through the Visual Studio Installer. " +
            "The Xploder script may build with a newer default compiler, but OmniConvert's Win7 build must use v141-v143." +
            [Environment]::NewLine + "Detected compiler folders:" + [Environment]::NewLine + $FoundText)
    }

    return $Candidates |
        Sort-Object `
            @{ Expression = 'ToolsetRank'; Descending = $true },
            @{ Expression = 'VsMajor'; Descending = $true },
            @{ Expression = 'ToolsetVersionObject'; Descending = $true } |
        Select-Object -First 1
}

try {
    Write-Section "$ProductName v$Version - Windows 7 SP1 x64 unified build"
    Write-Host "Root:          $Root"
    Write-Host "Configuration: $Configuration"
    Write-Host "Build folder:  $BuildDir"
    Write-Host "Output folder: $DistDir"
    Write-Host "Toolset choice: $Toolset"
    Write-Host "Full log:      $LogPath"

    $CMake = (Get-Command cmake.exe -ErrorAction Stop).Source
    $CTest = (Get-Command ctest.exe -ErrorAction Stop).Source
    $Vs = Find-CompatibleVisualStudio -CMakePath $CMake -RequestedToolset $Toolset -RequestedInstallPath $VisualStudioPath

    Write-Host "Visual Studio: $($Vs.DisplayName)"
    Write-Host "Install path:  $($Vs.InstallPath)"
    Write-Host "Generator:     $($Vs.Generator)"
    Write-Host "MSVC toolset:  $($Vs.Toolset) $($Vs.ToolsetVersion)"

    Write-Section 'Checking project timestamps'
    Repair-FutureProjectTimestamps `
        -ProjectRoot $Root `
        -ExcludedDirectories @('.git', 'build', '0-Finished', '~Legacy~')

    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Section 'Cleaning previous build'
        Remove-Item -Recurse -Force $BuildDir
    }
    if (Test-Path $DistDir) {
        Remove-Item -Recurse -Force $DistDir
    }
    New-Item -ItemType Directory -Force -Path $BuildDir, $DistDir | Out-Null

    Write-Section "Configuring with $($Vs.Generator) and $($Vs.Toolset)"
    $ConfigureArgs = @(
        '-S', '.',
        '-B', $BuildDir,
        '-G', $Vs.Generator,
        '-A', 'x64',
        '-T', $Vs.ToolsetArgument,
        "-DCMAKE_GENERATOR_INSTANCE=$($Vs.InstallPath)",
        '-DOMNI_BUILD_GUI=ON',
        '-DOMNI_BUILD_CLI=OFF',
        '-DOMNI_BUILD_TESTS=ON',
        '-DOMNI_WINDOWS7_COMPAT=ON',
        '-DOMNI_STATIC_MSVC_RUNTIME=ON',
        '-DCMAKE_SUPPRESS_REGENERATION=ON'
    )
    Invoke-LoggedNative $CMake $ConfigureArgs 'CMake configure'

    Write-Section 'Building'
    Invoke-LoggedNative $CMake @('--build', $BuildDir, '--config', $Configuration, '--parallel') 'CMake build'

    $CompilerWarnings = Select-String -Path $LogPath -Pattern '\bwarning (C|LNK)[0-9]{4}\b' -AllMatches
    if ($CompilerWarnings) {
        $CompilerWarnings | ForEach-Object { $_.Line } | Set-Content -Encoding UTF8 $SummaryPath
        throw "Compiler or linker warnings were detected. See $SummaryPath"
    }

    Write-Section 'Running tests'
    Invoke-LoggedNative $CTest @('--test-dir', $BuildDir, '-C', $Configuration, '--output-on-failure') 'CTest'

    $ExeCandidates = @(
        (Join-Path $BuildDir "$Configuration\$ExecutableFileName"),
        (Join-Path $BuildDir $ExecutableFileName)
    )
    $Exe = $ExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($Exe)) {
        throw "$ExecutableFileName was not produced under $BuildDir"
    }

    Write-Section 'Validating the Windows 7 executable'
    $ImportsPath = Join-Path $DistDir "$ExecutableName-imports.txt"
    $HeadersPath = Join-Path $DistDir "$ExecutableName-headers.txt"
    $DependentsPath = Join-Path $DistDir "$ExecutableName-dependents.txt"

    $Imports = (& $Vs.Dumpbin /nologo /imports $Exe 2>&1 | Out-String)
    $Headers = (& $Vs.Dumpbin /nologo /headers $Exe 2>&1 | Out-String)
    $Dependents = (& $Vs.Dumpbin /nologo /dependents $Exe 2>&1 | Out-String)
    $Imports | Set-Content -Encoding UTF8 $ImportsPath
    $Headers | Set-Content -Encoding UTF8 $HeadersPath
    $Dependents | Set-Content -Encoding UTF8 $DependentsPath

    $ForbiddenImports = @(
        'CreateFile2',
        'CopyFile2',
        'GetTempPath2W',
        'GetSystemTimePreciseAsFileTime'
    )
    foreach ($Import in $ForbiddenImports) {
        if ($Imports -match [regex]::Escape($Import)) {
            throw "Windows 8+ API import detected: $Import"
        }
    }

    if ($Headers -notmatch '(?im)^\s*6\.01 subsystem version\s*$') {
        throw 'The PE subsystem version is not Windows 7 (6.01).'
    }
    if ($Headers -notmatch '(?im)^\s*8664 machine \(x64\)\s*$') {
        throw 'The produced executable is not x64.'
    }
    if ($Dependents -match '(?i)\b(VCRUNTIME|MSVCP|api-ms-win-crt)') {
        throw 'Dynamic Visual C++ runtime dependencies were found; static /MT linkage was not applied.'
    }

    Copy-Item -Force $Exe (Join-Path $DistDir $ExecutableFileName)
    Copy-Item -Force (Join-Path $Root 'LICENSE') (Join-Path $DistDir 'LICENSE.txt')
    $Windows7BuildDoc = Join-Path $Root 'docs\WINDOWS7_BUILD.md'
    if (Test-Path -LiteralPath $Windows7BuildDoc) {
        Copy-Item -Force -LiteralPath $Windows7BuildDoc -Destination (Join-Path $DistDir 'WINDOWS7_BUILD.txt')
    }
    else {
        Write-Host 'Optional docs\WINDOWS7_BUILD.md was not found; skipping it.'
    }

    $ReleaseExe = Join-Path $DistDir $ExecutableFileName
    $Hash = (Get-FileHash -Algorithm SHA256 $ReleaseExe).Hash.ToLowerInvariant()
    "$Hash  $ExecutableFileName" | Set-Content -Encoding ASCII (Join-Path $DistDir "$ExecutableFileName.sha256")

    @(
        "$ProductName v$Version",
        'Target: Windows 7 SP1 x64',
        "Configuration: $Configuration",
        "Visual Studio: $($Vs.DisplayName)",
        "Generator: $($Vs.Generator)",
        "MSVC toolset: $($Vs.Toolset) $($Vs.ToolsetVersion)",
        'Runtime: static /MT',
        'PE subsystem: 6.01',
        'Unified EXE: no arguments opens GUI; arguments run CLI',
        "Built: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss K'))"
    ) | Set-Content -Encoding UTF8 (Join-Path $DistDir 'BUILD-INFO.txt')

    if (-not (Test-Path $SummaryPath)) {
        'No compiler or linker warnings detected.' | Set-Content -Encoding UTF8 $SummaryPath
    }

    Write-Section 'SUCCESS'
    Write-Host "Windows 7-compatible executable: $ReleaseExe"
    Write-Host "SHA-256: $Hash"
    Write-Host "Release folder: $DistDir"
    Write-Host "Build log: $LogPath"
    Write-Host "Warning/error summary: $SummaryPath"
}
catch {
    $Message = $_.Exception.Message
    Write-Host ''
    Write-Host "ERROR: $Message" -ForegroundColor Red

    $SummaryLines = @("ERROR: $Message")
    if (Test-Path $LogPath) {
        $RelevantLogLines = Select-String -Path $LogPath `
            -Pattern '\bwarning (C|LNK)[0-9]{4}\b|\berror (C|LNK)[0-9]{4}\b|fatal error|CMake Error' `
            -AllMatches | ForEach-Object { $_.Line }
        if ($RelevantLogLines) {
            $SummaryLines += ''
            $SummaryLines += $RelevantLogLines
        }
    }
    $LastTestLog = Join-Path $BuildDir 'Testing\Temporary\LastTest.log'
    $LastTestsFailed = Join-Path $BuildDir 'Testing\Temporary\LastTestsFailed.log'
    if (Test-Path $LastTestsFailed) {
        $SummaryLines += ''
        $SummaryLines += 'Failed CTest cases:'
        $SummaryLines += Get-Content $LastTestsFailed
    }
    if (Test-Path $LastTestLog) {
        $SummaryLines += ''
        $SummaryLines += 'CTest LastTest.log:'
        $SummaryLines += Get-Content $LastTestLog
    }

    $SummaryLines += ''
    $SummaryLines += "Full log: $LogPath"
    $SummaryLines | Set-Content -Encoding UTF8 $SummaryPath

    Write-Host "Full log: $LogPath"
    Write-Host "Warning/error summary: $SummaryPath"
    exit 1
}
