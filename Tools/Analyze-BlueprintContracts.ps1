param(
    [string]$LogPath = "Saved/Logs/CompileAllBlueprints-5.8-compat.log",
    [string]$OutputDirectory = "Docs/BlueprintContractInventory"
)

$ErrorActionPreference = "Stop"

$resolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$resolvedOutput = Join-Path $projectRoot $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutput | Out-Null

function Get-Subsystem([string]$assetPath) {
    switch -Regex ($assetPath.Replace("\", "/")) {
        "/UI/Attribute/" { return "GAS.Attributes" }
        "/UI/SpellMenu/" { return "GAS.Spells" }
        "/UI/HUD/SpellGlobes/" { return "GAS.Spells" }
        "/UI/HUD/" { return "HUD" }
        "/UI/InventorySystem/" { return "Inventory.Equipment.Vendors" }
        "/UI/CraftingSystem/" { return "Crafting" }
        "/UI/QuestSystem/" { return "Quests" }
        "/UI/World/" { return "WorldWidgets.Effects" }
        "/UI/Menu/" { return "Menus" }
        "/Animations/Anims/AnimBP_" { return "LegacyAnimation" }
        default { return "Other" }
    }
}

function Join-Unique([object[]]$values) {
    return (($values | Where-Object { $_ } | Sort-Object -Unique) -join "; ")
}

$lines = Get-Content -LiteralPath $resolvedLog
$diagnostics = [System.Collections.Generic.List[object]]::new()
$failures = [System.Collections.Generic.List[object]]::new()

foreach ($line in $lines) {
    if ($line -match "LogBlueprint:\s+(?<severity>Error|Warning): \[AssetLog\] (?<asset>.+?\.uasset): \[Compiler\] (?<message>.*)$") {
        $asset = $Matches.asset
        $relativeAsset = $asset
        $contentPrefix = Join-Path $projectRoot "Content"
        if ($asset.StartsWith($contentPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $relativeAsset = "Content" + $asset.Substring($contentPrefix.Length)
        }
        $diagnostics.Add([pscustomobject]@{
            Severity = $Matches.severity
            Subsystem = Get-Subsystem $relativeAsset
            Asset = $relativeAsset.Replace("\", "/")
            Message = $Matches.message.Trim()
        })
        continue
    }

    if ($line -match "Compile of (?<name>.+?) failed\. (?<fatal>\d+) Fatal Issue\(s\) (?<warnings>\d+) Warning\(s\).*\((?<object>/Game/.+)\)$") {
        $failures.Add([pscustomobject]@{
            Subsystem = Get-Subsystem $Matches.object
            Blueprint = $Matches.name
            ObjectPath = $Matches.object
            FatalIssues = [int]$Matches.fatal
            Warnings = [int]$Matches.warnings
        })
    }
}

$functionRefs = foreach ($diagnostic in $diagnostics) {
    if ($diagnostic.Message -match 'Could not find a function named "(?<name>[^"]+)"') {
        [pscustomobject]@{ Name = $Matches.name; Subsystem = $diagnostic.Subsystem; Asset = $diagnostic.Asset; Evidence = $diagnostic.Message }
    } elseif ($diagnostic.Message -match "Missing function (?<name>[^ ]+) from class") {
        [pscustomobject]@{ Name = $Matches.name; Subsystem = $diagnostic.Subsystem; Asset = $diagnostic.Asset; Evidence = $diagnostic.Message }
    }
}

$propertyRefs = foreach ($diagnostic in $diagnostics) {
    if ($diagnostic.Message -match "The property associated with\s+(?<name>.+?)\s+could not be found") {
        [pscustomobject]@{ Name = $Matches.name.Trim(); Subsystem = $diagnostic.Subsystem; Asset = $diagnostic.Asset; Evidence = $diagnostic.Message }
    }
}

$delegateRefs = foreach ($diagnostic in $diagnostics) {
    if ($diagnostic.Message -match "^(?<name>[A-Za-z0-9_ ]+?)(?:_Event(?:_\d+)?)?\s+(?:Signature Error:|:) unable to determine expected signature") {
        [pscustomobject]@{ Name = $Matches.name.Trim(); Subsystem = $diagnostic.Subsystem; Asset = $diagnostic.Asset; Evidence = $diagnostic.Message }
    } elseif ($diagnostic.Message -match "Event Dispatcher has no property\s+(?:Assign |Bind Event to |Unbind all Events from )?(?<name>.+)$") {
        [pscustomobject]@{ Name = $Matches.name.Trim(); Subsystem = $diagnostic.Subsystem; Asset = $diagnostic.Asset; Evidence = $diagnostic.Message }
    }
}

$structPins = foreach ($diagnostic in $diagnostics) {
    if ($diagnostic.Message -match "In use pin\s+(?<pin>.+?)\s+no longer exists on node\s+Break <unknown struct>") {
        [pscustomobject]@{ Pin = $Matches.pin.Trim(); Subsystem = $diagnostic.Subsystem; Asset = $diagnostic.Asset; Evidence = $diagnostic.Message }
    }
}

$nodePins = foreach ($diagnostic in $diagnostics) {
    if ($diagnostic.Message -match "In use pin\s+(?<pin>.+?)\s+no longer exists on node\s+(?<node>.+?)\s+\. Please refresh") {
        [pscustomobject]@{ Node = $Matches.node.Trim(); Pin = $Matches.pin.Trim(); Subsystem = $diagnostic.Subsystem; Asset = $diagnostic.Asset }
    }
}

function Collapse-References([object[]]$references, [string]$keyName) {
    return $references | Group-Object -Property $keyName | ForEach-Object {
        $items = $_.Group
        [pscustomobject]@{
            Name = $_.Name
            Occurrences = $items.Count
            Subsystems = Join-Unique $items.Subsystem
            Assets = Join-Unique $items.Asset
            Evidence = Join-Unique $items.Evidence
        }
    } | Sort-Object -Property @{ Expression = "Occurrences"; Descending = $true }, Name
}

$functionSummary = Collapse-References $functionRefs "Name"
$propertySummary = Collapse-References $propertyRefs "Name"
$delegateSummary = Collapse-References $delegateRefs "Name"
$structSummary = Collapse-References $structPins "Pin"
$pinSummary = $nodePins | Group-Object Node, Pin | ForEach-Object {
    $items = $_.Group
    [pscustomobject]@{
        Node = $items[0].Node
        Pin = $items[0].Pin
        Occurrences = $items.Count
        Subsystems = Join-Unique $items.Subsystem
        Assets = Join-Unique $items.Asset
    }
} | Sort-Object -Property @{ Expression = "Occurrences"; Descending = $true }, Node, Pin

$serializedTypeRefs = [System.Collections.Generic.List[object]]::new()
$contentRoot = Join-Path $projectRoot "Content"
$rgOutput = & rg -a -o --with-filename '/Script/(DuTraveller|DT)\.[A-Za-z0-9_]+' $contentRoot
foreach ($entry in $rgOutput) {
    if ($entry -match "^(?<asset>.+?\.uasset):(?<type>/Script/(?:DuTraveller|DT)\.[A-Za-z0-9_]+)$") {
        $asset = $Matches.asset
        $relativeAsset = "Content" + $asset.Substring($contentRoot.Length)
        $serializedTypeRefs.Add([pscustomobject]@{
            Type = $Matches.type
            Module = ($Matches.type -split '[/.]')[2]
            Asset = $relativeAsset.Replace("\", "/")
            Subsystem = Get-Subsystem $relativeAsset
        })
    }
}
$serializedTypeSummary = $serializedTypeRefs | Group-Object Type | ForEach-Object {
    [pscustomobject]@{
        Type = $_.Name
        Assets = ($_.Group | Select-Object -ExpandProperty Asset | Sort-Object -Unique).Count
        Subsystems = Join-Unique $_.Group.Subsystem
        ReferencingAssets = Join-Unique $_.Group.Asset
    }
} | Sort-Object Type

$diagnostics | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $resolvedOutput "raw-blueprint-diagnostics.csv")
$failures | Sort-Object Subsystem, Blueprint | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $resolvedOutput "failing-blueprints.csv")
$functionSummary | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $resolvedOutput "missing-functions.csv")
$propertySummary | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $resolvedOutput "missing-properties.csv")
$delegateSummary | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $resolvedOutput "missing-delegates.csv")
$structSummary | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $resolvedOutput "unknown-struct-pins.csv")
$pinSummary | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $resolvedOutput "missing-node-pins.csv")
$serializedTypeSummary | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $resolvedOutput "serialized-native-types.csv")

$subsystemRows = $failures | Group-Object Subsystem | ForEach-Object {
    [pscustomobject]@{
        Subsystem = $_.Name
        Blueprints = $_.Count
        FatalIssues = ($_.Group | Measure-Object FatalIssues -Sum).Sum
        Warnings = ($_.Group | Measure-Object Warnings -Sum).Sum
    }
} | Sort-Object FatalIssues -Descending

$report = [System.Collections.Generic.List[string]]::new()
$report.Add("# Blueprint Contract Inventory")
$report.Add("")
$report.Add("Generated from ``$($LogPath.Replace('\', '/'))`` by ``Tools/Analyze-BlueprintContracts.ps1``. This is evidence extraction only; it does not infer missing C++ signatures or runtime behavior.")
$report.Add("")
$report.Add("## Baseline")
$report.Add("")
$report.Add("- Failed Blueprints: $($failures.Count)")
$report.Add("- Fatal Blueprint issues: $(($failures | Measure-Object FatalIssues -Sum).Sum)")
$report.Add("- Blueprint warnings on failed assets: $(($failures | Measure-Object Warnings -Sum).Sum)")
$report.Add("- Parsed compiler diagnostic records: $($diagnostics.Count)")
$report.Add("- Distinct missing function names: $($functionSummary.Count)")
$report.Add("- Distinct missing property names: $($propertySummary.Count)")
$report.Add("- Distinct delegate/event names with lost signatures: $($delegateSummary.Count)")
$report.Add("- Distinct pins recovered from unknown structs: $($structSummary.Count)")
$report.Add("- Distinct serialized project-native types: $($serializedTypeSummary.Count)")
$report.Add("")
$report.Add("## Failure clusters")
$report.Add("")
$report.Add("| Subsystem | Blueprints | Fatal issues | Warnings |")
$report.Add("|---|---:|---:|---:|")
foreach ($row in $subsystemRows) {
    $report.Add("| $($row.Subsystem) | $($row.Blueprints) | $($row.FatalIssues) | $($row.Warnings) |")
}
$report.Add("")
$report.Add("## Highest-frequency missing functions")
$report.Add("")
$report.Add("| Function | Occurrences | Subsystems |")
$report.Add("|---|---:|---|")
foreach ($row in ($functionSummary | Select-Object -First 30)) {
    $report.Add("| ``$($row.Name)`` | $($row.Occurrences) | $($row.Subsystems) |")
}
$report.Add("")
$report.Add("## Inventory files")
$report.Add("")
$report.Add("- ``failing-blueprints.csv``: authoritative per-Blueprint fatal/warning counts emitted by the commandlet.")
$report.Add("- ``missing-functions.csv``: missing reflected function names and referencing assets.")
$report.Add("- ``missing-properties.csv``: missing reflected properties and referencing assets.")
$report.Add("- ``missing-delegates.csv``: delegate/event nodes whose signatures can no longer be resolved.")
$report.Add("- ``unknown-struct-pins.csv``: serialized field names retained by broken struct nodes.")
$report.Add("- ``missing-node-pins.csv``: serialized function/node pin names for later signature reconstruction.")
$report.Add("- ``raw-blueprint-diagnostics.csv``: normalized raw Blueprint compiler evidence.")
$report.Add("- ``serialized-native-types.csv``: exact ``/Script/DT`` and ``/Script/DuTraveller`` type identities and referencing assets.")
$report.Add("")
$report.Add("Names in these files are contract candidates, not proof of owning class, parameter type, replication policy, or behavior. Those details must be corroborated before implementation.")

$report | Set-Content -Encoding utf8 -Path (Join-Path $resolvedOutput "README.md")

Write-Output "Wrote Blueprint contract inventory to $resolvedOutput"
Write-Output "Failed Blueprints: $($failures.Count)"
Write-Output "Fatal issues: $(($failures | Measure-Object FatalIssues -Sum).Sum)"
Write-Output "Parsed diagnostics: $($diagnostics.Count)"
