call \"C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat\" -arch=amd64
cd /d D:\livio\NovaIIM
cmake -B build -DNOVA_BUILD_TESTS=ON
cmake --build build --target test_nova_sdk
build\output\test\test_nova_sdk.exe
