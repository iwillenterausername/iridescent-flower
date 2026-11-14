# Contributing

Thanks for trying Iridescent Flower.

## Before opening a PR

- Open an issue for large DSP or architecture changes first.
- Keep builds reproducible on Windows x64 with VS 2022 + CMake.
- Run the offline tests before submitting:
  ```powershell
  .\build.ps1
  & "build\IridescentTests_artefacts\Release\IridescentTests.exe" --vst3 "build\IridescentFlower_artefacts\Release\VST3\Iridescent Flower.vst3"
  ```
- Do not commit `build/`, `dist/`, `.vs/`, generated WAVs or plugin binaries.
- For custom modules, put source under `CustomModules/` and document the DLL ABI in its README.

## DSP changes

- Avoid allocations and locks on the audio thread.
- Keep left/right filter state independent.
- Add a regression test in `Tests/IridescentTests.cpp` for every bug fix.
- Do not reintroduce shared per-module parameters: each rack slot owns a `SlotState`.

## Artwork and fonts

Only submit artwork you have the right to redistribute. The bundled avatars are
project-specific; see `THIRD_PARTY_NOTICES.md`.
