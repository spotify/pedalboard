<#
.SYNOPSIS
Builds pedalboard's Windows test plugins from source.

.DESCRIPTION
Builds the VST3 plugins used by tests/test_external_plugins.py from pinned
commits of their source code, against pedalboard's JUCE submodule:

  - CHOWTapeModel 2.7.0 (https://github.com/jatinchowdhury18/AnalogTapeModel, GPLv3)
  - Magical8bitPlug2 1.0.1 (https://github.com/yokemura/Magical8bitPlug2, MIT)

Each plugin is copied into its VST3 bundle in tests/plugins/*/Windows, at
<Name>.vst3/Contents/<arch>-win/<Name>.vst3, replacing any existing build for
that architecture. Each plugin's CMake target (its settings, modules, and source
and resource files) is generated from its Projucer (.jucer) project.

Run this on a Windows machine with Visual Studio 2022 (or newer), CMake 3.22+
and Git, after initializing pedalboard's JUCE submodule. This script is run by
the "Build Windows Test Plugins" GitHub Actions workflow, which builds each
architecture on a native runner.

.PARAMETER Architecture
The CPU architecture to build for: ARM64 or x64. Defaults to the current machine's.

.PARAMETER BuildDir
Where to check out the plugin sources and build them.
#>
#Requires -Version 7.4

param(
    [ValidateSet("ARM64", "x64")]
    [string]$Architecture = $(if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "ARM64" } else { "x64" }),
    [string]$BuildDir = (Join-Path $PSScriptRoot "build")
)

$ErrorActionPreference = "Stop"
# Throw if a native command (git, cmake) exits with a non-zero exit code:
$PSNativeCommandUseErrorActionPreference = $true
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$SourceDir = Join-Path $BuildDir "src"
$CMakeBuildDir = Join-Path $BuildDir $Architecture

# Folder names from https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Locations+Format/Plugin+Format.html
$BundleArchitecture = @{ "ARM64" = "arm64-win"; "x64" = "x86_64-win" }[$Architecture]

$Plugins = @(
    @{
        Name = "CHOWTapeModel"
        Type = "effect"
        Url = "https://github.com/jatinchowdhury18/AnalogTapeModel.git"
        Commit = "6e98855db14759d7952ec7298b661853679fb03f" # v2.7.0
        Jucer = "Plugin/CHOWTapeModel.jucer"
        # Originally built with JUCE 6.0, which allowed modal loops by default:
        Definitions = @("JUCE_MODAL_LOOPS_PERMITTED=1")
        Submodules = @("Plugin/foleys_gui_magic")
        # Source changes needed to build against JUCE 7:
        Patches = @(
            @{
                File = "Plugin/foleys_gui_magic/General/foleys_MagicGUIState.cpp"
                Find = "visualiserThread.startThread (3);"
                Replace = "visualiserThread.startThread (juce::Thread::Priority::low);"
            }
        )
    },
    @{
        Name = "Magical8bitPlug2"
        Type = "instrument"
        Url = "https://github.com/yokemura/Magical8bitPlug2.git"
        Commit = "10be0196618732750aba95b35c31624b1a146ec2" # v1.0.1
        Jucer = "Magical8bitPlug2.jucer"
        Definitions = @()
        Submodules = @()
        Patches = @(
            @{
                # The synth adds to the output buffer, which (unlike older JUCE
                # versions) JUCE 7's VST3 wrapper doesn't clear first:
                File = "Source/PluginProcessor.cpp"
                Find = "synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples()); // [5]"
                Replace = "buffer.clear(); synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples()); // [5]"
            }
        )
    }
)

foreach ($Plugin in $Plugins) {
    $Checkout = Join-Path $SourceDir $Plugin.Name
    if (-not (Test-Path (Join-Path $Checkout ".git"))) {
        git init --quiet $Checkout
        git -C $Checkout remote add origin $Plugin.Url
    }
    git -C $Checkout fetch --quiet --depth 1 origin $Plugin.Commit
    git -C $Checkout checkout --quiet --force $Plugin.Commit
    foreach ($Submodule in $Plugin.Submodules) {
        git -C $Checkout submodule update --init --force --depth 1 $Submodule
    }
    foreach ($Patch in $Plugin.Patches) {
        $Path = Join-Path $Checkout $Patch.File
        $Source = [IO.File]::ReadAllText($Path)
        if (-not $Source.Contains($Patch.Find)) {
            throw "Couldn't find '$($Patch.Find)' in $Path"
        }
        [IO.File]::WriteAllText($Path, $Source.Replace($Patch.Find, $Patch.Replace))
    }
    $Plugin.Checkout = $Checkout
}

# Generate a pedalboard_add_test_plugin() call (see CMakeLists.txt) for each plugin
# from its Projucer project, using Projucer's defaults for any missing settings:
function ConvertTo-CMakeArgument([string]$Value) {
    '"' + ($Value -replace '([\\"$;])', '\$1') + '"'
}

function Get-JucerPluginDefinition($Plugin) {
    $JucerPath = Join-Path $Plugin.Checkout $Plugin.Jucer
    $JucerDir = Split-Path $JucerPath
    $Jucer = [xml](Get-Content -Raw $JucerPath)
    $Project = $Jucer.JUCERPROJECT
    if ($Project.projectType -ne "audioplug") {
        throw "$JucerPath isn't an audio plugin project"
    }
    $Attribute = { param($Name, $Default) $Value = $Project.GetAttribute($Name); if ($Value) { $Value } else { $Default } }
    $ToCMakePath = { param($Path) [IO.Path]::GetFullPath((Join-Path $JucerDir $Path)).Replace('\', '/') }

    # From Project::initialiseAudioPluginValues: the default plugin code is derived
    # from the project ID, and must match for the VST3 class ID (and presets) to match.
    $Identifier = (($Project.id + $Project.id) -replace '[.,;:/@]', '_') -replace '[^A-Za-z0-9_]', ''
    if ($Identifier -match '^\d') { $Identifier = "_$Identifier" }
    $Identifier += "xxxx"
    $DefaultPluginCode = $Identifier.Substring(0, 1).ToUpper() + $Identifier.Substring(1, 3).ToLower()

    $Options = [ordered]@{
        PRODUCT_NAME = $Project.name
        VERSION = & $Attribute version "1.0.0"
        COMPANY_NAME = & $Attribute pluginManufacturer (& $Attribute companyName "yourcompany")
        COMPANY_EMAIL = & $Attribute companyEmail $null
        COMPANY_WEBSITE = & $Attribute companyWebsite $null
        PLUGIN_MANUFACTURER_CODE = & $Attribute pluginManufacturerCode "Manu"
        PLUGIN_CODE = & $Attribute pluginCode $DefaultPluginCode
        PLUGIN_NAME = & $Attribute pluginName $Project.name
        DESCRIPTION = & $Attribute pluginDesc $Project.name
        BUNDLE_ID = & $Attribute bundleIdentifier $null
    }
    $Characteristics = (& $Attribute pluginCharacteristicsValue "") -split ","
    $Flags = [ordered]@{
        pluginIsSynth = "IS_SYNTH"
        pluginWantsMidiIn = "NEEDS_MIDI_INPUT"
        pluginProducesMidiOut = "NEEDS_MIDI_OUTPUT"
        pluginIsMidiEffectPlugin = "IS_MIDI_EFFECT"
        pluginEditorRequiresKeys = "EDITOR_WANTS_KEYBOARD_FOCUS"
    }
    foreach ($Flag in $Flags.Keys) {
        if ($Characteristics -contains $Flag) {
            $Options[$Flags[$Flag]] = "TRUE"
        }
    }

    $ChannelConfigs = & $Attribute pluginChannelConfigs $null
    $Definitions = @(
        foreach ($Option in $Jucer.SelectNodes("/JUCERPROJECT/JUCEOPTIONS/@*")) {
            switch ($Option.Value) {
                { $_ -in "1", "enabled" } { "$($Option.Name)=1" }
                { $_ -in "0", "disabled" } { "$($Option.Name)=0" }
            }
        }
        if ($ChannelConfigs) {
            "JucePlugin_PreferredChannelConfigurations=$($ChannelConfigs -replace '\s', '')"
        }
        $Plugin.Definitions
    )

    # Use the Visual Studio exporter's compiler flags, and its paths to the plugin's own modules:
    $Exporter = $Jucer.SelectNodes("/JUCERPROJECT/EXPORTFORMATS/*[starts-with(local-name(), 'VS')]") |
        Sort-Object LocalName | Select-Object -Last 1
    if (-not $Exporter) {
        throw "No Visual Studio exporter found in $JucerPath"
    }
    $CompileOptions = @($Exporter.GetAttribute("extraCompilerFlags") -split '\s+' | Where-Object { $_ })
    $Modules = foreach ($Module in $Jucer.SelectNodes("/JUCERPROJECT/MODULES/MODULE")) {
        if ($Module.id -eq "juce_audio_plugin_client") {
            continue # Added by juce_add_plugin
        }
        if ($Module.id -like "juce_*") {
            $Module.id
            continue
        }
        $ModulePath = $Exporter.SelectSingleNode("MODULEPATHS/MODULEPATH[@id='$($Module.id)']")
        if (-not $ModulePath) {
            throw "No path for module $($Module.id) in the $($Exporter.LocalName) exporter in $JucerPath"
        }
        & $ToCMakePath (Join-Path $ModulePath.path $Module.id)
    }

    $Sources = $Jucer.SelectNodes("//FILE[@compile='1']") | ForEach-Object { & $ToCMakePath $_.file }
    $Resources = $Jucer.SelectNodes("//FILE[@resource='1']") | ForEach-Object { & $ToCMakePath $_.file }
    if (-not $Sources) {
        throw "No source files found in $JucerPath"
    }

    "pedalboard_add_test_plugin($($Plugin.Name)"
    "    PLUGIN_OPTIONS"
    foreach ($Name in $Options.Keys) {
        if ($null -ne $Options[$Name]) {
            "        $Name $(ConvertTo-CMakeArgument $Options[$Name])"
        }
    }
    foreach ($List in @(
            @{ Name = "MODULES"; Values = $Modules },
            @{ Name = "SOURCES"; Values = $Sources },
            @{ Name = "RESOURCES"; Values = $Resources },
            @{ Name = "DEFINITIONS"; Values = $Definitions },
            @{ Name = "COMPILE_OPTIONS"; Values = $CompileOptions })) {
        if ($List.Values) {
            "    $($List.Name)"
            $List.Values | ForEach-Object { "        $(ConvertTo-CMakeArgument $_)" }
        }
    }
    ")"
}

$PluginsFile = Join-Path $BuildDir "plugins.cmake"
$PluginDefinitions = foreach ($Plugin in $Plugins) { Get-JucerPluginDefinition $Plugin }
Set-Content -Path $PluginsFile -Value $PluginDefinitions -Encoding utf8NoBOM

cmake -S $PSScriptRoot -B $CMakeBuildDir -A $Architecture `
    "-DPEDALBOARD_JUCE_DIR=$RepoRoot\JUCE" `
    "-DPLUGINS_FILE=$PluginsFile"
$Targets = $Plugins | ForEach-Object { "$($_.Name)_VST3" }
cmake --build $CMakeBuildDir --config Release --parallel --target @Targets

foreach ($Plugin in $Plugins) {
    $Name = $Plugin.Name
    $BuiltBundle = Join-Path $CMakeBuildDir "${Name}_artefacts\Release\VST3\$Name.vst3"
    $Built = Get-ChildItem -Path (Join-Path $BuiltBundle "Contents\*-win\$Name.vst3") -File
    if (@($Built).Count -ne 1) {
        throw "Expected exactly one built plugin in $BuiltBundle, found: $($Built -join ', ')"
    }

    $Destination = Join-Path $RepoRoot "tests\plugins\$($Plugin.Type)\Windows\$Name.vst3\Contents\$BundleArchitecture"
    New-Item -ItemType Directory -Force $Destination | Out-Null
    Copy-Item $Built.FullName (Join-Path $Destination "$Name.vst3") -Force
    Write-Host "Updated $Destination\$Name.vst3"
}
