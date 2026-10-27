// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
//
// Compiles the engine (../../engine) with Quod (../../src, ../../gen) in it: on the web one slot (a page plays one
// Quod), elsewhere all eight with the resampler and the clock, as the Audio Unit has them. The C wants clang (or
// clang-cl on Windows: MSVC's own cl has no __atomic builtins). On a Mac, Apple's clang has no WebAssembly: the
// web build takes Homebrew's LLVM when CC_wasm32_unknown_unknown isn't set.
use std::{env, path::{Path, PathBuf}};

fn main() {
    let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("../..");
    let engine = root.join("engine");
    let wasm = env::var("CARGO_CFG_TARGET_ARCH").unwrap() == "wasm32";
    let mut b = cc::Build::new();
    b.include(&engine).include(root.join("src")).include(root.join("gen")).opt_level(2).warnings(false);
    if wasm {
        for (var, tool) in [("CC_wasm32_unknown_unknown", "clang"), ("AR_wasm32_unknown_unknown", "llvm-ar")] {
            let brew = Path::new("/opt/homebrew/opt/llvm/bin").join(tool);
            if env::var_os(var).is_none() && brew.exists() {
                b.compiler_or_archiver(var, &brew);
            }
        }
        b.flag("-ffreestanding").file(engine.join("quod_slot0.c"));
    } else {
        for i in 0..8 {
            b.file(engine.join(format!("quod_slot{i}.c")));
        }
        b.file(engine.join("quod_slots.c")).file(engine.join("quod_resample.c")).file(engine.join("quod_clock.c"));
        b.flag_if_supported("-Wno-everything");
    }
    b.compile("quod");
    println!("cargo:rerun-if-changed={}", engine.display());
    println!("cargo:rerun-if-changed={}", root.join("src").display());
    println!("cargo:rerun-if-changed={}", root.join("gen").display());
}

trait Tool {
    fn compiler_or_archiver(&mut self, var: &str, path: &Path) -> &mut Self;
}
impl Tool for cc::Build {
    fn compiler_or_archiver(&mut self, var: &str, path: &Path) -> &mut Self {
        if var.starts_with("CC") { self.compiler(path) } else { self.archiver(path) }
    }
}
