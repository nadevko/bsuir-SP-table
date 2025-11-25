{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    nabiki = {
      url = "github:nadevko/nabiki/v2-alpha";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.treefmt-nix.follows = "treefmt-nix";
    };

    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      treefmt-nix,
      nabiki,
    }:
    let
      private = final: prev: { };

      perPackages =
        pkgs:
        let
          treefmt = treefmt-nix.lib.evalModule pkgs {
            programs.nixfmt.enable = true;
            programs.nixfmt.strict = true;
            programs.prettier.enable = true;
            programs.cmake-format.enable = true;
          };
        in
        rec {
          formatter = treefmt.config.build.wrapper;
          checks.treefmt = treefmt.config.build.wrapper;
          devShells = nabiki.lib.rebase (nabiki.lib.readDevShellsOverlay { } private ./pkgs) pkgs;
          legacyPackages = pkgs.extend (_: _: packages);
          packages = nabiki.lib.rebase self.overlays.default pkgs;
        };
    in
    nabiki.lib.nestAttrs' (
      system: nixpkgs.legacyPackages.${system}
    ) nixpkgs.legacyPackages.x86_64-linux.dotnet-sdk_9.meta.platforms perPackages
    // {
      overlays.default = nabiki.lib.readPackagesOverlay { } private ./pkgs;
    };
}
