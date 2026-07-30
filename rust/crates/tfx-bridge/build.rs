fn main() {
    cxx_build::bridge("src/lib.rs")
        .std("c++17")
        .compile("tfx-bridge-cxx");

    println!("cargo:rerun-if-changed=src/lib.rs");
}
