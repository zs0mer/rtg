{
  description = "BACKBLAST — mouse-aimed recoil platformer";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs {
        inherit system;
      };
    in
    {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "backblast";
        version = "1.0.0";
        src = ./.;
        nativeBuildInputs = with pkgs; [ cmake ];
        doCheck = true;
        checkPhase = "ctest --output-on-failure";
      };

      apps.${system}.default = {
        type = "app";
        meta.description = "Endless mouse-aimed shotgun recoil terminal platformer";
        program = "${self.packages.${system}.default}/bin/backblast";
      };

      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          cmake
          ninja
          gcc
          gdb
          python3
        ];

        shellHook = ''
          echo "BACKBLAST: cmake -S . -B build -G Ninja && cmake --build build"
          echo "Play: ./build/backblast  |  Test: ctest --test-dir build"
        '';
      };
    };
}
