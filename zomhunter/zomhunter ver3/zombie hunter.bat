@echo off
g++ gametest.cpp -o gametest.exe -L. -lraylib -lopengl32 -lgdi32 -lwinmm
if %errorlevel% equ 0 (
    gametest.exe
) else (
    echo Bien dich that bai!
)
pause