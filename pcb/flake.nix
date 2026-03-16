{
  description = "pcb dev shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      treefmt-nix,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };

        shellPackages = with pkgs; (if pkgs.stdenv.isDarwin then [ ] else [ kicad ]);
      in
      {
        devShells.default = pkgs.mkShell {
          packages = shellPackages;
        };

        formatter = treefmt-nix.lib.mkWrapper pkgs {
          projectRootFile = "flake.nix";
          programs = {
            clang-format.enable = true;
            cmake-format.enable = true;
            prettier.enable = true;
          };
        };
      }
    );
}
