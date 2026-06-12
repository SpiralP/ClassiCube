{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { nixpkgs, ... }: {
    packages.x86_64-linux.default =
      let
        pkgs = import nixpkgs { system = "x86_64-linux"; };
        debug = false;

        defaultAttrs = ({
          name = "classicube";

          src = ./.;
          patches = [ ];

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
      if debug
      then
        (pkgs.enableDebugging {
          inherit (pkgs) stdenv;
          override = ({ stdenv }: stdenv.mkDerivation);
        })
          (
            (defaultAttrs // {
              hardeningDisable = [ "all" ];
            })
          )
      else
        pkgs.stdenv.mkDerivation defaultAttrs;
  };
}
