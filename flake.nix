{
  description = "ESP32-IDF development shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  };

  outputs =
    {
      self,
      nixpkgs,
      esp-dev,
    }:
    let
      forAllSystems = nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed;
    in
    {
      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          baseShell = esp-dev.devShells.${system}.esp32-idf;
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ baseShell ];

            packages = with pkgs; [
              clang-tools
              cmake
              gdb
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
        }
      );
    };
}
