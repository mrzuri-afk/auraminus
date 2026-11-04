$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path $PSScriptRoot -Parent
$toolDir = Join-Path $root '.tools'
New-Item -ItemType Directory -Force $toolDir | Out-Null
if (!(Test-Path "$toolDir/jdk")) {
    $assets = Invoke-RestMethod 'https://api.adoptium.net/v3/assets/latest/17/hotspot?architecture=x64&image_type=jdk&os=windows&vendor=eclipse'
    $pkg = $assets[0].binary.package
    Invoke-WebRequest $pkg.link -OutFile "$toolDir/jdk.zip"
    if ((Get-FileHash "$toolDir/jdk.zip" -Algorithm SHA256).Hash.ToLower() -ne $pkg.checksum) { throw 'JDK checksum mismatch' }
    Expand-Archive "$toolDir/jdk.zip" "$toolDir/jdk-extract" -Force
    $extracted = Get-ChildItem "$toolDir/jdk-extract" -Directory | Select-Object -First 1
    Move-Item -LiteralPath $extracted.FullName -Destination "$toolDir/jdk"
}
if (!(Test-Path "$toolDir/gradle-8.11.1")) {
    Invoke-WebRequest 'https://services.gradle.org/distributions/gradle-8.11.1-bin.zip' -OutFile "$toolDir/gradle.zip"
    $checksum = (Invoke-RestMethod 'https://services.gradle.org/distributions/gradle-8.11.1-bin.zip.sha256').Trim()
    if ((Get-FileHash "$toolDir/gradle.zip" -Algorithm SHA256).Hash.ToLower() -ne $checksum) { throw 'Gradle checksum mismatch' }
    Expand-Archive "$toolDir/gradle.zip" $toolDir -Force
}
if (!(Test-Path "$toolDir/android-sdk/cmdline-tools/latest/bin/sdkmanager.bat")) {
    Invoke-WebRequest 'https://dl.google.com/android/repository/commandlinetools-win-13114758_latest.zip' -OutFile "$toolDir/android-tools.zip"
    Expand-Archive "$toolDir/android-tools.zip" "$toolDir/sdk-extract" -Force
    New-Item -ItemType Directory -Force "$toolDir/android-sdk/cmdline-tools" | Out-Null
    Move-Item -LiteralPath "$toolDir/sdk-extract/cmdline-tools" -Destination "$toolDir/android-sdk/cmdline-tools/latest"
}
$env:JAVA_HOME = "$toolDir/jdk"
$env:ANDROID_HOME = "$toolDir/android-sdk"
$env:GRADLE_USER_HOME = "$toolDir/gradle-cache"
# SDK licenses are shown by sdkmanager; run this script interactively to accept them.
& "$toolDir/android-sdk/cmdline-tools/latest/bin/sdkmanager.bat" --sdk_root=$env:ANDROID_HOME 'platforms;android-35' 'build-tools;35.0.0' 'platform-tools'
if ($LASTEXITCODE -ne 0) { throw 'SDK installation failed' }
if (!(Test-Path "$env:ANDROID_HOME/platforms/android-35/android.jar")) { throw 'SDK platform missing. Accept the SDK license when prompted and rerun.' }
"sdk.dir=$($env:ANDROID_HOME.Replace('\','/'))" | Set-Content "$root/local.properties"
& "$toolDir/gradle-8.11.1/bin/gradle.bat" -p $root wrapper --gradle-version 8.11.1 --no-validate-url
if ($LASTEXITCODE -ne 0) { throw 'Gradle setup failed' }
