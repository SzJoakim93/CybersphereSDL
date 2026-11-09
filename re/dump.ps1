param([int]$seg, [int]$off, [int]$w, [int]$h, [int]$stride = 0)
if ($stride -eq 0) { $stride = $w }
$b = [IO.File]::ReadAllBytes('C:\Program Files (x86)\Cybersphere\CYBER.EXE')
$base = 0x200 + ($seg - 0x1000) * 16 + $off
for ($y = 0; $y -lt $h; $y++) {
    $line = ''
    for ($x = 0; $x -lt $w; $x++) {
        $v = $b[$base + $y * $stride + $x]
        $line += if ($v -eq 0) { ' .' } else { '{0,2:x}' -f $v }
    }
    $line
}
