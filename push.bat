@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo           JurkOS Automated Git Push Script
echo ========================================================
echo.

echo [*] Initializing Git repository...
git init

echo [*] Adding all files to staging...
git add .

echo [*] Setting remote origin...
git remote remove origin 2>nul
git remote add origin https://github.com/AmineTheJurk/compilejurkos/

echo.
set /p commitname="Enter commit message: "
if "%commitname%"=="" set commitname="Update JurkOS repository"

echo [*] Committing changes with message: "%commitname%"...
git commit -m "%commitname%"

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

pause
