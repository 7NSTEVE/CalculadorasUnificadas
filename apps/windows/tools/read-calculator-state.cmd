@echo off
setlocal EnableDelayedExpansion
REM read-calculator-state.cmd - Legacy CLI compatibility forwarder
REM Routes state queries directly to cg50_agent.py state

set SCRIPT_DIR=%~dp0
set PYTHON_BIN=python

if exist "D:\DevTools\Python311\python.exe" (
    set PYTHON_BIN="D:\DevTools\Python311\python.exe"
)

%PYTHON_BIN% "%SCRIPT_DIR%cg50_agent.py" state %*
exit /b %ERRORLEVEL%
