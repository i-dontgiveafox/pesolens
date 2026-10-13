@echo off
setlocal
cd /d "%~dp0"
title Pesolens
echo PesoLens project launcher
echo.
echo Project:
echo   %CD%
echo.
echo Resume checkpoint:
echo   KAGGLE_ROBOFLOW_CHECKPOINT.md
echo.
echo Current blocker:
echo   The latest Roboflow/Kaggle export is missing the 200 peso class.
echo   Fix Roboflow and rerun the class remap audit before training.
echo.
echo Starting Codex resume...
echo.
codex resume
if errorlevel 1 (
  echo.
  echo codex resume exited with an error. You can still open this folder in Codex
  echo and say: Read KAGGLE_ROBOFLOW_CHECKPOINT.md and continue PesoLens.
  echo.
  pause
)
