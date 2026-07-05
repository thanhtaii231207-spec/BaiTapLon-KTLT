@echo off
g++ gametest.cpp -o gametest -lraylib -lopengl32 -lgdi32 -lwinmm
gametest.exe
pause