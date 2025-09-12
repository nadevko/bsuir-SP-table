{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    n = {
      url = "path:/home/nadevko/Workspace/nabiki";
      inputs.nixpkgs.follows = "nixpkgs";
    };

    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      nixpkgs,
      treefmt-nix,
      n,
      ...
    }@inputs:
    n nixpkgs.legacyPackages.x86_64-linux.dotnet-sdk_9.meta.platforms (
      platform:
      let
        treefmt = treefmt-nix.lib.evalModule pkgs {
          programs.nixfmt.enable = true;
          programs.nixfmt.strict = true;
          programs.prettier.enable = true;
          programs.cmake-format.enable = true;
        };
        pkgs = nixpkgs.legacyPackages.${platform};
      in
      {
        devShells.default = pkgs.mkShell.override { inherit (pkgs.llvmPackages) stdenv; } rec {
          packages = with pkgs; [
            llvmPackages.lldb
            cmake
            sdl3.dev
            sdl3-ttf
            fontconfig.dev
          ];
          LD_LIBRARY_PATH = nixpkgs.lib.makeLibraryPath packages;
          vscode-settings = pkgs.writeText "settings.json" (
            builtins.toJSON {
              "clangd.path" = "${pkgs.clang-tools}/bin/clangd";
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
        };
        formatter = treefmt.config.build.wrapper;
        checks.treefmt = treefmt.config.build.wrapper;
        packages = n.lib.readPackages {
          path = ./pkgs;
          overrides.inputs = inputs;
          inherit pkgs;
        };
        legacyPackages = n.lib.readLegacyPackages {
          path = ./pkgs;
          overrides.inputs = inputs;
          inherit pkgs;
        };
      }
    );
}
