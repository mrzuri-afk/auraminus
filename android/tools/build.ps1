$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (Test-Path "$root/.tools/jdk") { $env:JAVA_HOME = "$root/.tools/jdk" }
if (Test-Path "$root/.tools/android-sdk") { $env:ANDROID_HOME = "$root/.tools/android-sdk" }
$env:GRADLE_USER_HOME = "$root/.tools/gradle-cache"
if (Test-Path "$root/.tools/gradle-8.11.1/bin/gradle.bat") {
    & "$root/.tools/gradle-8.11.1/bin/gradle.bat" -p $root :mobile:testDebugUnitTest :mobile:lintDebug :mobile:assembleDebug --console=plain
} else {
    & "$root/gradlew.bat" -p $root :mobile:testDebugUnitTest :mobile:lintDebug :mobile:assembleDebug --console=plain
}
if ($LASTEXITCODE -ne 0) { throw 'Build or checks failed' }

$destination = Join-Path $root "output"
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item -LiteralPath "$root/mobile/build/outputs/apk/debug/mobile-debug.apk" -Destination "$destination/Auraminus-Android-debug.apk" -Force
Write-Host "APK ready: $destination/Auraminus-Android-debug.apk" -ForegroundColor Green
