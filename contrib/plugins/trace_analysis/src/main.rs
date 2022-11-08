use std::fs;
use std::io::{self, BufReader, Read};

use capstone::prelude::*;
use std::collections::HashMap;

pub struct TraceEntryARM {
    pub p_pc :u64,
    pub n_insts: usize,
    pub is_user: bool,
    pub insts: Vec<u32>
}

pub struct TraceEntryX86 {
    pub p_pc: u64,
    pub n_insts: usize,
    pub is_user: bool,
    pub insts_bytes: Vec<u8>
}

/// Print register names
fn reg_names(cs: &Capstone, regs: &[RegId]) -> String {
    let names: Vec<String> = regs.iter().map(|&x| cs.reg_name(x).unwrap()).collect();
    names.join(", ")
}

/// Print instruction group names
fn group_names(cs: &Capstone, regs: &[InsnGroupId]) -> String {
    let mut names: Vec<String> = regs.iter().map(|&x| cs.group_name(x).unwrap()).collect();
    names.sort();
    names.join(", ")
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

    let is_user = (p_pc & (1 << 63)) != 0;

    // 3. read the instruction one by one.
    let mut insts = Vec::with_capacity(n_insts);
    for _ in 0..n_insts {
        let mut buf = [0u8; 4];
        f.read_exact(&mut buf).unwrap();
        insts.push(u32::from_le_bytes(buf));
    }
    assert!(n_bytes == n_insts * 4);

    return TraceEntryARM { p_pc, n_insts, is_user, insts };
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

    let is_user = (p_pc & (1 << 63)) != 0;

    // 3. read the instruction one by one.
    // let mut handle = f.take(n_bytes as u64);
    // handle.read(&mut insts_bytes);
    for _ in 0..n_bytes {
        let mut buf = [0u8; 1];
        f.read_exact(&mut buf).unwrap();
        insts_bytes.push(u8::from_le_bytes(buf));
    }

    return TraceEntryX86 { p_pc, n_insts, is_user, insts_bytes };
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

//        let mut map : [HashMap<String, u32>; 13] = [HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(), HashMap::new(),];
        let mut map : HashMap<String, u32> = HashMap::new();
        loop {
            io::stdin().read_line(&mut s).unwrap();
            let t = get_next_trace_x86(&mut buf);

            println!("PC: {:#016x}, is_user: {}, instruction count: {}", t.p_pc, t.is_user, t.n_insts);
            let d = cs.disasm_all(&t.insts_bytes, 0).unwrap();
            for i in d.iter() {
                println!("{}", i);

                let detail: InsnDetail = cs.insn_detail(&i).expect("Failed to get insn detail");
                let arch_detail: ArchDetail = detail.arch_detail();
                let ops = arch_detail.operands();

                let g: String = group_names(&cs, detail.groups());
                let byte_len = i.bytes().len();

                //let output: &[(&str, String)] = &[
                //    ("insn id:", format!("{:?}", i.id().0)),
                //    ("bytes:", format!("{:?}", i.bytes())),
                //    ("read regs:", reg_names(&cs, detail.regs_read())),
                //    ("write regs:", reg_names(&cs, detail.regs_write())),
                //    ("insn groups:", g),
                //];

                // for &(ref name, ref message) in output.iter() {
                //     println!("{:4}{:12} {}", "", name, message);
                // }

                *map.entry(g).or_insert(0) += 1u32;

                println!("{:4}operands: {}", "", ops.len());
                for op in ops {
                    println!("{:8}{:?}", "", op);
                }

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

        let mut map : HashMap<String, u32> = HashMap::new();
        loop {
            io::stdin().read_line(&mut s).unwrap();
            let t = get_next_trace_arm(&mut buf);

            println!("PC: {:#016x}, is_user: {}, instruction count: {}", t.p_pc, t.is_user, t.n_insts);

            for inst in t.insts.iter() {
                println!("{}", inst);
                let d = cs.disasm_all(&inst.to_le_bytes(), 0).unwrap();
                for i in d.iter() {
                    println!("{}", i);

                    let detail: InsnDetail = cs.insn_detail(&i).expect("Failed to get insn detail");
                    let arch_detail: ArchDetail = detail.arch_detail();
                    let ops = arch_detail.operands();

                    let g = group_names(&cs, detail.groups());
                    *map.entry(g).or_insert(0) += 1u32;
                    // let output: &[(&str, String)] = &[
                    //     ("insn id:", format!("{:?}", i.id().0)),
                    //     ("bytes:", format!("{:?}", i.bytes())),
                    //     ("read regs:", reg_names(&cs, detail.regs_read())),
                    //     ("write regs:", reg_names(&cs, detail.regs_write())),
                    //     ("insn groups:", groups),
                    // ];

                    // for &(ref name, ref message) in output.iter() {
                    //     println!("{:4}{:12} {}", "", name, message);
                    // }

                    println!("{:4}operands: {}", "", ops.len());
                    for op in ops {
                        println!("{:8}{:?}", "", op);
                    }
                }
            }
            println!("--------------------");
        }
    }

    return Ok(());
}
