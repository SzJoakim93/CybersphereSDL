param([string]$hex)
$b = [IO.File]::ReadAllBytes('C:\Program Files (x86)\Cybersphere\CYBER.EXE')
$pat = $hex -split ' ' | ForEach-Object { [Convert]::ToByte($_, 16) }
$hdr = [BitConverter]::ToUInt16($b, 8) * 16
for ($i = $hdr; $i -le $b.Length - $pat.Length; $i++) {
    $ok = $true
    for ($k = 0; $k -lt $pat.Length; $k++) { if ($b[$i + $k] -ne $pat[$k]) { $ok = $false; break } }
    if ($ok) { $lin = [int]($i - $hdr); '{0:x5}' -f $lin }
}
