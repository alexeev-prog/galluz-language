{ pkgs ? import <nixpkgs> {} }:
let
  # Оверлей для LLVM 14
  llvm14Overlay = self: super: {
    llvmPackages_14 = super.llvmPackages_14;
  };
  
  # Применяем overlay
  pkgsWithLLVM14 = import <nixpkgs> {
    overlays = [ llvm14Overlay ];
  };
  
  libs = with pkgs; [
    boost
    cmake
    libffi
    stb
    entt
    ncurses
    pkg-config
    gdb
    catch2
    nodejs
    catch2_3
    boehmgc
    valgrind
    libxml2
    gcc
  ];
  
  llvm14Libs = with pkgsWithLLVM14.llvmPackages_14; [
    clang
    clang-tools
    libclang
    llvm
    lld
    lldb
  ];
  
  allLibs = libs ++ llvm14Libs;
  
in
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cppcheck
    codespell
    conan
    doxygen
    gtest
    lcov
    vcpkg
    vcpkg-tool
  ] ++ (with pkgsWithLLVM14.llvmPackages_14; [
    clang-tools
  ]);
  
  buildInputs = allLibs;
  
  shellHook = ''
    echo "morning.lang Dev Shell with LLVM 14"
    
    export CC=${pkgsWithLLVM14.llvmPackages_14.clang}/bin/clang
    export CXX=${pkgsWithLLVM14.llvmPackages_14.clang}/bin/clang++
    
    export CXXFLAGS="-I${pkgs.gcc}/include/c++/${pkgs.gcc.version} -I${pkgs.glibc}/include"
    
    echo "LLVM version: $(${pkgsWithLLVM14.llvmPackages_14.llvm}/bin/llvm-config --version)"
  '';
}
