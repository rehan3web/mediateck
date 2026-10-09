@echo off
setlocal enabledelayedexpansion

echo =====================================================================
echo   Push MediaTek META Tool to GitHub (https://github.com/rehanweb3/mediateck)
echo =====================================================================
echo.

:: 1. Check if git is installed
where git >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Git is not installed or not in PATH!
    echo Please install Git for Windows from https://git-scm.com/
    pause
    exit /b 1
)

:: 2. Initialize Git repo if needed
if not exist ".git" (
    echo [*] Initializing new git repository...
    git init -b main
) else (
    echo [*] Git repository already initialized.
    git branch -M main
)

:: 3. Configure Remote URL
echo [*] Setting remote origin to https://github.com/rehanweb3/mediateck.git ...
git remote remove origin 2>nul
git remote add origin https://github.com/rehanweb3/mediateck.git

:: 4. Add files
echo [*] Staging files...
git add .

:: 5. Commit
echo [*] Committing changes...
git commit -m "feat: Add MediaTek META Mode tool for Moto G73 5G with automated GitHub Release CI/CD workflow" 2>nul
if %ERRORLEVEL% neq 0 (
    echo [INFO] No new changes to commit or working tree clean.
)

:: 6. Create Release Tag
echo [*] Creating version tag v1.0.0...
git tag -d v1.0.0 2>nul
git tag -a v1.0.0 -m "Release v1.0.0: MediaTek META Mode Tool for Motorola Moto G73 5G"

:: 7. Push to GitHub
echo.
echo =====================================================================
echo   Pushing code and tag to GitHub...
echo   (If prompted, please enter your GitHub username and Personal Access Token)
echo =====================================================================
echo.

git push -u origin main
if %ERRORLEVEL% neq 0 (
    echo.
    echo [!] Failed to push main branch. If remote contains existing files, trying push with force:
    set /p PUSH_FORCE="Do you want to force push to overwrite? (Y/N): "
    if /i "!PUSH_FORCE!"=="Y" (
        git push -u origin main --force
    )
)

echo.
echo [*] Pushing release tag v1.0.0 to trigger automated GitHub Release build...
git push origin v1.0.0 --force

echo.
echo =====================================================================
echo   DONE! 
echo   GitHub Actions will now automatically:
echo     1. Compile the tool on windows-latest runner
echo     2. Create mtk_meta_tool-windows-x64.zip
echo     3. Publish a new Release at:
echo        https://github.com/rehanweb3/mediateck/releases
echo =====================================================================
echo.
pause
