@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4459 -wd4456 -wd4201 -wd4100 -wd4189 -wd4211 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DWIN32_HANDMADE=1 -FC -Zi
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib Gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 32-bit build
REM cl %CommonCompilerFlags% W:\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.2  %CommonLinkerFlags%

REM 64-bit build
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% W:\handmade\code\handmade.cpp -Fmhandmade.map /LD /link -incremental:no -opt:ref /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
cl %CommonCompilerFlags% W:\handmade\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd