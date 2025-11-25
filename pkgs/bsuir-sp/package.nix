{
  stdenv,
  cmake,
  sdl3,
  sdl3-ttf,
  fontconfig,
}:
stdenv.mkDerivation {
  pname = "bsuir-sp";
  version = "1.0";

  src = ../..;

  buildInputs = [
    sdl3
    sdl3-ttf
    fontconfig
  ];
  nativeBuildInputs = [
    cmake
    sdl3.dev
    fontconfig.dev
  ];
}
