{
  description = "esp32 dev shell";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    flake-utils.url = "github:numtide/flake-utils";

    esp-idf.url = "github:mirrexagon/nixpkgs-esp-dev";
  };
  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      treefmt-nix,
      esp-idf,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ esp-idf.overlays.default ];
          config.permittedInsecurePackages = [
            "python3.13-ecdsa-0.19.1"
          ];
        };
        idf = esp-idf.packages.${system}.esp-idf-xtensa.override (final: {
          toolsToInclude = final.toolsToInclude ++ [
            "esp-clang"
          ];
        });
        project = "esp32-nix";
      in
      {
        devShells.default =
          pkgs.mkShell.override
            {
              stdenv = pkgs.llvmPackages_latest.libcxxStdenv;
            }
            {
              # export CLANGD_QUERY_DRIVER=`which xtensa-esp32-elf-g++`
              name = project;
              LSP_SERVER = "clangd";
              CMAKE_EXPORT_COMPILE_COMMANDS = 1;
              IDF_TOOLCHAIN = "clang";
              packages = with pkgs; [
                idf
                cmake
              ];
              shellHook = ''
                ROOT_DIR=$(pwd)

                PROJECT_DIRS="glove car"

                for dir in $PROJECT_DIRS; do
                  if [ -d "$ROOT_DIR/$dir" ]; then
                    echo "Checking $dir..."
                    cd "$ROOT_DIR/$dir"

                    if [ ! -d "build" ]; then
                      echo "Build dir missing in $dir, running idf.py reconfigure..."
                      idf.py reconfigure
                    fi

                    # Symlink build/compile_commands.json to the sub-project root (e.g., car/compile_commands.json)
                    if [ -f "build/compile_commands.json" ]; then
                      ln -sf build/compile_commands.json .
                      echo "Linked $dir/compile_commands.json"
                    else
                      echo "Error: compile_commands.json not found in $dir/build"
                    fi
                  fi
                done


              '';
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
