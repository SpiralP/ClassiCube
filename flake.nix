{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { nixpkgs, ... }:
    let
      inherit (nixpkgs) lib;

      # patchelf + buildFlags = "linux": only Linux targets can build
      systems = [ "x86_64-linux" "aarch64-linux" ];

      makePackage = ({ debug, system }:
        let
          pkgs = import nixpkgs { inherit system; };
          stdenv =
            if debug
            then pkgs.stdenvAdapters.keepDebugInfo pkgs.stdenv
            else pkgs.stdenv;
        in
        stdenv.mkDerivation {
          name = "classicube";

          src = ./.;

          postPatch = ''
            substituteInPlace src/Platform_Posix.c \
              --replace-fail '"/usr/share/fonts"' '"${pkgs.liberation_ttf}/share/fonts"'
          '';

          buildInputs = with pkgs; [
            libX11
            libXi
            libGL
          ];

          nativeBuildInputs = with pkgs; [
            makeWrapper
          ];

          # makeFlags = [ "CFLAGS+=-DCC_BUILD_GLMODERN" ];
          buildFlags = [ "linux" ];

          installPhase = ''
            install -Dm755 ClassiCube -t $out/bin
          '';

          postFixup = ''
            patchelf \
              --add-needed ${pkgs.curl.out}/lib/libcurl.so \
              --add-needed ${pkgs.openal.out}/lib/libopenal.so \
              $out/bin/ClassiCube
          '';

          enableParallelBuilding = true;
          meta.mainProgram = "ClassiCube";
          hardeningDisable = [ "all" ];
        });
    in
    {
      packages = lib.genAttrs systems (system: {
        default = makePackage { debug = false; inherit system; };
      });
      devShells = lib.genAttrs systems (system: {
        default = makePackage { debug = true; inherit system; };
      });
    };
}
