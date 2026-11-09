param([string]$Exe = 'Cybersphere-SDL.exe', [string]$Shots = '300', [string]$Extra = '')
# Deterministic autoplay captures: .\cap.ps1 -Exe lose.exe -Shots 418,1292
$bin = Join-Path $PSScriptRoot '..\bin'
Add-Type -AssemblyName System.Drawing
Push-Location $bin
foreach ($n in ($Shots -split ',')) {
    $a = @('--auto') + ($Extra -split ' ' | Where-Object { $_ }) + @('--shot', "cap_$n.bmp", $n)
    Start-Process -Wait -FilePath (Join-Path $bin $Exe) -ArgumentList $a
    $img = [System.Drawing.Image]::FromFile((Join-Path $bin "cap_$n.bmp"))
    $img.Save((Join-Path $bin "cap_$n.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    Remove-Item (Join-Path $bin "cap_$n.bmp")
}
Pop-Location
