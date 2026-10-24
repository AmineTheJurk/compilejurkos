@echo off
setlocal enabledelayedexpansion

title JurkOS Git Push (Git Bash Edition)

echo ========================================================
echo           JurkOS Automated Git Push Script
echo ========================================================
echo.

:: Change directory to the repository folder where this script resides
cd /d "%~dp0"

:: 1. Try to locate Git Bash (bash.exe)
set "GIT_BASH="

:: Check standard 64-bit Program Files
if exist "%ProgramFiles%\Git\bin\bash.exe" (
    set "GIT_BASH=%ProgramFiles%\Git\bin\bash.exe"
    goto :found_bash
)

:: Check ProgramW6432
if defined ProgramW6432 (
    if exist "%ProgramW6432%\Git\bin\bash.exe" (
        set "GIT_BASH=%ProgramW6432%\Git\bin\bash.exe"
        goto :found_bash
    )
)

:: Check 32-bit Program Files
if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" (
    set "GIT_BASH=%ProgramFiles(x86)%\Git\bin\bash.exe"
    goto :found_bash
)

:: Check Local AppData User Install
if exist "%LOCALAPPDATA%\Programs\Git\bin\bash.exe" (
    set "GIT_BASH=%LOCALAPPDATA%\Programs\Git\bin\bash.exe"
    goto :found_bash
)

:: Check PATH for bash.exe from Git
for /f "delims=" %%i in ('where bash.exe 2^>nul') do (
    if exist "%%i" (
        set "GIT_BASH=%%i"
        goto :found_bash
    )
)

:found_bash
if defined GIT_BASH (
    echo [*] Found Git Bash at: "%GIT_BASH%"
    echo [*] Launching execution inside Git Bash environment...
    echo.
    "%GIT_BASH%" --login -i -c "./push.sh"
    goto :end
)

:: 2. Fallback if Git Bash binary is not found: run directly via cmd git
echo [!] Git Bash not found in standard paths.
echo [*] Falling back to standard Git command line...
echo.

echo [*] Initializing Git repository...
git init

echo [*] Adding all files to staging...
git add -A

echo [*] Setting remote origin...
git remote remove origin 2>nul
git remote add origin https://github.com/AmineTheJurk/compilejurkos/

echo.
set /p commitname="Enter commit message (default: 'Update JurkOS repository'): "
if "%commitname%"=="" set commitname="Update JurkOS repository"

echo [*] Committing changes with message: "%commitname%"...
git commit -m "%commitname%" 2>nul || ver >nul

echo [*] Renaming branch to main...
git branch -M main

echo [*] Force pushing to origin main...
git push -u origin main --force

echo.
if %errorlevel% equ 0 (
    echo ========================================================
    echo  [SUCCESS] Pushed to https://github.com/AmineTheJurk/compilejurkos/
    echo ========================================================
) else (
    echo ========================================================
    echo  [ERROR] Git push failed. Please check your credentials or network.
    echo ========================================================
)

:end
echo.
pause
