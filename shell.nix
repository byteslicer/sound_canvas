{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cmake
    pkg-config
    git
  ];

  buildInputs = with pkgs; [
    alsa-lib
    freetype
    fontconfig
    expat
    libx11
    libxext
    libxrandr
    libxinerama
    libxcursor
    libxi
    xorgproto
  ];
}
