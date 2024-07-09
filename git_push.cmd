@echo off
REM 获取当前日期和时间并格式化为 YYYY-MM-DD HH:MM
for /f "tokens=2 delims==" %%i in ('"wmic os get localdatetime /value"') do set datetime=%%i
set date=%datetime:~0,4%-%datetime:~4,2%-%datetime:~6,2%
set time=%datetime:~8,2%:%datetime:~10,2%

REM 打印当前日期和时间
echo data and time: %date% %time%

REM 执行 Git 命令
git add .
git commit -m "%date% %time%"
git push

REM 提示完成
echo Git complete
pause
