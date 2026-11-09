param([int]$seg, [int]$off, [int]$len, [int]$cols = 16)
$b = [IO.File]::ReadAllBytes('C:\Program Files (x86)\Cybersphere\CYBER.EXE')
$base = 0x200 + ($seg - 0x1000) * 16 + $off
for ($i = 0; $i -lt $len; $i += $cols) {
    $n = [Math]::Min($cols, $len - $i)
    ('{0:x4}: ' -f ($off + $i)) + (($b[($base + $i)..($base + $i + $n - 1)] | ForEach-Object { '{0:x2}' -f $_ }) -join ' ')
}
