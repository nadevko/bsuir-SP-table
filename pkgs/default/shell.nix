{
  mkShell,
  llvmPackages,
  cmake,
  sdl3,
  sdl3-ttf,
  fontconfig,
  lib,
  writeText,
  clang-tools,
}:
mkShell.override { inherit (llvmPackages) stdenv; } rec {
  packages = [
    llvmPackages.lldb
    cmake
    sdl3.dev
    sdl3-ttf
    fontconfig.dev
  ];
  LD_LIBRARY_PATH = lib.makeLibraryPath packages;
  vscode-settings = writeText "settings.json" (
    builtins.toJSON {
      "clangd.path" = "${clang-tools}/bin/clangd";
      "cmake.buildType" = "Debug";
      "cmake.debugConfig" = {
        type = "lldb-dap";
        request = "launch";
        name = "Debug with LLDB DAP";
        program = "\${command:cmake.launchTargetPath}";
        args = [ ];
        stopOnEntry = false;
      };
    }
  );
  shellHook = ''
    mkdir .vscode
    cat ${vscode-settings} > .vscode/settings.json
  '';
}
