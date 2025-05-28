{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    rust-overlay.url = "github:oxalica/rust-overlay";
  };

  outputs = { 
    nixpkgs, 
    rust-overlay, 
    ... 
  }:
    let
      overlays = [ (import rust-overlay) ];
      forAllSystems = function:
        nixpkgs.lib.genAttrs [
          "x86_64-linux"
          "aarch64-linux"
        ] (system:
          function (import nixpkgs {
            inherit system;
            overlays = overlays;
            config.allowUnfree = true;
            pkgs = import nixpkgs { inherit system overlays; };
          }));
    in
    {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
  nativeBuildInputs = with pkgs;[ 
    # Only tested in x11
    # for building streamer
    openssl
    zlib
    pkg-config
    fontconfig
    clangStdenv
    curl
    nasm
    yasm
    vulkan-headers
    mesa
    unzip
    ffmpeg
    pipewire
    libunwind
    libva
    libdrm
    libclang
    alsa-lib
    vulkan-loader
    vulkan-helper
    xorg.libX11
    xorg.libXrandr
    rustPlatform.bindgenHook
    x264
    # for running streamer
    xorg.libXcursor
    xorg.libXi
    libxkbcommon
    libGL
    (rust-bin.fromRustupToolchainFile ./rust-toolchain.toml)
      ];

 shellHook = ''
  export LIBCLANG_PATH="${pkgs.libclang.lib}/lib"
  export AS=nasm
'';
  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath (with pkgs;[
            libxkbcommon
            libGL
            ]);
};

});
      overlays.default = final: prev: {};
    };
}
