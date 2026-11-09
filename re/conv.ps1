$src = 'c:\Users\Szabo Joakim\AppData\Roaming\Code\User\workspaceStorage\243a133bd6eb8145b8477a1ab85d2ae3\GitHub.copilot-chat\chat-session-resources\3396f34f-5059-4a9b-b5ec-fb95c08998f6'
Get-ChildItem $src -Recurse -Filter content.json | ForEach-Object {
    $j = Get-Content $_.FullName -Raw | ConvertFrom-Json
    if ($j.items -and $j.items[0].instruction) {
        $name = ($j.items[0].address -replace ':', '_') + '.asm'
        $out = Join-Path $PSScriptRoot $name
        if (-not (Test-Path $out)) {
            $j.items | ForEach-Object { "$($_.address) $($_.instruction)" } | Set-Content $out
            Write-Output "$name $($j.items.Count)"
        }
    }
}
