# Vendored by tools/vendor-sdl.sh. Do not edit: run the script.

- `sdl/`: SDL 3.4.18 (https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz, SHA-256 9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3), zlib license (`sdl/LICENSE.txt`): the
  237 sources a Windows build with only the joystick, haptic, HIDAPI, power and sensor
  subsystems compiles, and the 215 files they include.
- `sdl-config/windows/`: that configuration's generated `SDL_build_config.h` (clang, msys2 UCRT64).
- `sdl-db/`: SDL_GameControllerDB c0f11d40b91f57853ce9714c703e119d8243f91c (https://raw.githubusercontent.com/mdqinc/SDL_GameControllerDB/c0f11d40b91f57853ce9714c703e119d8243f91c/gamecontrollerdb.txt, SHA-256 a2299ef257b68c62b9fcce39fc59b5fb3a5a7068214f766a96c21595b55a11c3), zlib
  license (`sdl-db/LICENSE.txt`): its 869 Windows lines of 2295, as one C string, loaded at each start of SDL.
- `gameinput/`: Microsoft's `GameInput.h`, as `gameinput.h` (the name SDL includes), from the NuGet package Microsoft.GameInput 3.5.283 (https://api.nuget.org/v3-flatcontainer/microsoft.gameinput/3.5.283/microsoft.gameinput.3.5.283.nupkg,
  SHA-256 b5988cb8ff9d7009b6ddf6ad4e3ff87e91b00cc17ffcd5d628dabc21c208f100), MIT license (in the header). Microsoft's GameInput runtime itself is not here: it is a
  program's to install, under Microsoft's redistributable license.
