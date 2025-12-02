{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  hardeningDisable = [ "all" ];
  nativeBuildInputs = with pkgs; [
    pkg-config
  ];
  packages = with pkgs; [
    clang-tools
    fuse3
    gdb
    pkg-config
    valgrind
    libuuid
    libical
    scdoc
  ];
}
