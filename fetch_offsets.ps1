# fetch_offsets.ps1 — runs before every compile to pull fresh CS2 offsets from a2x/cs2-dumper
#
# offsets.hpp   → saved as src/cs2_offsets_ref.hpp (reference; our hand-crafted offsets.hpp
#                 stays intact because cs2-dumper uses cs2_dumper:: namespaces, we use client::)
# client.dll.hpp → saved as src/client.dll.hpp     (used directly for chams offsets)

$base = "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output"
$dest = Join-Path $PSScriptRoot "src"

$downloads = @(
    @{ Url = "$base/offsets.hpp";    Out = Join-Path $dest "cs2_offsets_ref.hpp" },
    @{ Url = "$base/client.dll.hpp"; Out = Join-Path $dest "client.dll.hpp" }
)

foreach ($dl in $downloads) {
    try {
        Invoke-WebRequest -Uri $dl.Url -OutFile $dl.Out -UseBasicParsing -ErrorAction Stop
        Write-Host "[offsets] fetched $($dl.Url | Split-Path -Leaf) -> $($dl.Out | Split-Path -Leaf)"
    } catch {
        Write-Warning "[offsets] failed to fetch $($dl.Url | Split-Path -Leaf) — using cached copy if present"
    }
}
