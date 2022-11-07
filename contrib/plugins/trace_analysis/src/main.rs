use std::fs;
use std::io::{self, BufReader, Read};

use capstone::prelude::*;

pub struct TraceEntryARM {
    pub p_pc :u64,
    pub n_insts: usize,
    pub insts: Vec<u32>
}

pub struct TraceEntryX86 {
    pub p_pc: u64,
    pub n_insts: usize,
    pub insts_bytes: Vec<u8>
}

pub fn get_next_trace_arm(f: &mut BufReader::<fs::File>) -> TraceEntryARM {
    // 1. read the physical address of the PC.
    let mut buf_pc = [0u8; 8];
    f.read_exact(&mut buf_pc).unwrap();
    let p_pc = u64::from_le_bytes(buf_pc);

    // 2. read number of instructions
    let mut buf_n_inst = [0u8; 2];
    f.read_exact(&mut buf_n_inst).unwrap();
    let n_insts : usize = u16::from_le_bytes(buf_n_inst) as usize;

    let mut buf_n_bytes = [0u8; 2];
    f.read_exact(&mut buf_n_bytes).unwrap();
    let n_bytes: usize = u16::from_le_bytes(buf_n_bytes) as usize;

    // 3. read the instruction one by one.
    let mut insts = Vec::with_capacity(n_insts);
    for _ in 0..n_insts {
        let mut buf = [0u8; 4];
        f.read_exact(&mut buf).unwrap();
        insts.push(u32::from_le_bytes(buf));
    }
    assert!(n_bytes == n_insts * 4);

    return TraceEntryARM { p_pc, n_insts, insts };
}

pub fn get_next_trace_x86(f: &mut BufReader::<fs::File>) -> TraceEntryX86 {
    // 1. read the physical address of the PC.
    let mut buf_pc = [0u8; 8];
    f.read_exact(&mut buf_pc).unwrap();
    let p_pc = u64::from_le_bytes(buf_pc);

    // 2. read number of instructions
    let mut buf_n_inst = [0u8; 2];
    f.read_exact(&mut buf_n_inst).unwrap();
    let n_insts : usize = u16::from_le_bytes(buf_n_inst) as usize;

    let mut buf_n_bytes = [0u8; 2];
    f.read_exact(&mut buf_n_bytes).unwrap();
    let n_bytes: usize = u16::from_le_bytes(buf_n_bytes) as usize;

    let mut insts_bytes: Vec<u8> = Vec::with_capacity(n_bytes);

    // 3. read the instruction one by one.
    let mut handle = f.take(n_bytes as u64);
    handle.read(&mut insts_bytes);

    return TraceEntryX86 { p_pc, n_insts, insts_bytes };
}

fn main() -> Result<(), io::Error> {

    let arch = std::env::args().nth(1).expect("no arch was given");
    let path_src = std::path::PathBuf::from(std::env::args().nth(2).expect("no path given"));

    let f = fs::File::open(path_src)?;

    let mut buf = BufReader::new(f);

    let mut s = String::new();
    if arch == "x86" {
        println!("Logging X86:");
        let cs = Capstone::new()
            .x86()
            .mode(arch::x86::ArchMode::Mode64)
            .detail(true)
            .build()
            .unwrap();

        loop {
            io::stdin().read_line(&mut s).unwrap();
            let t = get_next_trace_x86(&mut buf);

            println!("PC: {:#016x}, instruction count: {}", t.p_pc, t.n_insts);
            let d = cs.disasm_all(&t.insts_bytes, 0).unwrap();
            for l in d.iter() {
                println!("{}", l);
            }
            println!("--------------------");
        }
    } else {
        println!("Logging ARM:");
        let cs = Capstone::new()
            .arm64()
            .mode(arch::arm64::ArchMode::Arm)
            .detail(true)
            .build()
            .unwrap();

        loop {
            io::stdin().read_line(&mut s).unwrap();
            let t = get_next_trace_arm(&mut buf);

            println!("PC: {:#016x}, instruction count: {}", t.p_pc, t.n_insts);

            for inst in t.insts.iter() {
                println!("{}", inst);
                let d = cs.disasm_all(&inst.to_le_bytes(), 0).unwrap();
                for l in d.iter() {
                    println!("{}", l);
                }
            }
            println!("--------------------");
        }
    }

    return Ok(());
}
