$content = Get-Content src/rule.cpp -Raw
$content = $content -replace "}`n        auto", "}`n        auto"
Set-Content src/rule.cpp $content -NoNewline
