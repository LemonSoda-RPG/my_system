@echo off
REM 获取当前日期并格式化为 YYYY-MM-DD
for /f "tokens=2 delims==" %%i in ('"wmic os get localdatetime /value"') do set datetime=%%i
set date=%datetime:~0,4%-%datetime:~4,2%-%datetime:~6,2%

REM 打印当前日期
echo 当前日期: %date%

REM 执行 Git 命令
git add .
git commit -m "%date%"
git push

REM 提示完成
echo Git 命令执行完成
pause
