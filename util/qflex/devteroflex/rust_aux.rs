#![crate_name = "rust_aux"]

#[no_mangle]
pub extern "C" fn rust_aux_init() {
  println!("Hello from Rust!");
}
