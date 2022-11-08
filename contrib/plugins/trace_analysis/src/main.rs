use std::fs;
use std::io::{self, BufReader, Read};

use capstone::arch::arm::ArmOperandType;
use capstone::arch::arm64::{Arm64Operand, Arm64InsnDetail, Arm64OpMem};
use capstone::arch::x86::{X86Operand, X86InsnDetail, X86OpMem, X86OperandType};
use capstone::arch::x86::X86OperandType::*;
use capstone::{prelude::*, RegAccessType};
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

#[derive(Debug)]
pub struct Breakdown {
    pub tot: usize,
    pub user: usize,
    pub sys: usize,

    // Standard
    pub tot_load: usize,
    pub tot_store: usize,
    pub with_br: usize,
    pub with_sse: usize,
    pub with_crypto: usize,

    // Freak cases
    pub with_both: usize,
    pub mem_branch: usize,
    pub has_multi_mem: usize,

    pub byte_sizes: [usize; 13]

    
}

impl Breakdown {
    fn update (value: &mut Breakdown, is_user: bool, byte_len: usize, 
                        inst_loads: usize, inst_stores: usize, 
                        has_br: bool, has_both_mem: bool,
                        has_mem: bool, has_multi_mem: bool) {
        value.tot += 1;
        value.byte_sizes[byte_len] += 1;

        if is_user {
            value.user += 1;
        } else { 
            value.sys += 1;
        }

        value.tot_load += inst_loads;
        value.tot_store += inst_stores;

        if has_br {
            value.with_br += 1;
        }

        if has_both_mem {
            value.with_both += 1;
        }

        if has_multi_mem {
            value.has_multi_mem += 1;
        }

        if has_mem && has_br {
            value.mem_branch += 1;
        }
    }
}

impl Default for Breakdown {
    fn default () -> Breakdown{
        Breakdown{tot: 0, user: 0, sys:0, 
            tot_load: 0, tot_store: 0, with_br: 0, with_sse: 0, with_crypto: 0, 
            with_both: 0, mem_branch: 0, has_multi_mem: 0, byte_sizes: [0usize; 13]}
    }
    
}


/// Print register names
fn reg_names(cs: &Capstone, regs: &[RegId]) -> String {
    let names: Vec<String> = regs.iter().map(|&x| cs.reg_name(x).unwrap()).collect();
    names.join(", ")
}

/// Print instruction group names
fn group_names(cs: &Capstone, regs: &[InsnGroupId]) -> String {
    let mut names: Vec<String> = regs.iter().map(|&x| cs.group_name(x).unwrap()).filter(|s| !s.starts_with("mode")).collect();
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
    for _ in 0..n_bytes {
        let mut buf = [0u8; 1];
        f.read_exact(&mut buf).unwrap();
        insts_bytes.push(u8::from_le_bytes(buf));
    }

    return TraceEntryX86 { p_pc, n_insts, is_user, insts_bytes };
}

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
// println!("{:4}operands: {}", "", ops.len());
// for op in ops {
//     println!("{:8}{:?}", "", op);
// }



fn main() -> Result<(), io::Error> {

    let arch = std::env::args().nth(1).expect("no arch was given");
    let path_src = std::path::PathBuf::from(std::env::args().nth(2).expect("no path given"));

    let f = fs::File::open(path_src)?;

    let mut buf = BufReader::new(f);

    let mut curr_inst = 0usize;
    let mut map : HashMap<String, Breakdown> = HashMap::new();
    let mut map_mnem : HashMap<String, Breakdown> = HashMap::new();
    let mut map_bytes : HashMap<usize, Breakdown> = HashMap::new();
    if arch == "x86" {
        
        println!("Logging X86:");
        let cs = Capstone::new()
            .x86()
            .mode(arch::x86::ArchMode::Mode64)
            .detail(true)
            .build()
            .unwrap();

        let branch_groups = ["jump", "return", "branch_relative", "call"];
        let sse_groups = ["sse1", "sse2", "sse41", "sse42", "ssse3"];
        let crypto_groups = ["adx", "aes"];

        loop {
            let t = get_next_trace_x86(&mut buf);
            let d = cs.disasm_all(&t.insts_bytes, 0).unwrap();

            for i in d.iter() {
                if curr_inst % 10000000 == 0 {
                    println!("Insts[{}]", curr_inst);
                    println!("Groups:");
                    for (groups, breakdown) in &map {
                        println!("{groups:?},{:?}", breakdown);
                    }
                    println!("Mem Bytes:");
                    for (groups, breakdown) in &map_mnem {
                        println!("{groups:?},{:?}", breakdown);
                    }
                    println!("Mnemonic:");
                    for (groups, breakdown) in &map_bytes {
                        println!("{groups:?},{:?}", breakdown);
                    }
                }

                curr_inst += 1usize;
                
                let detail: InsnDetail = cs.insn_detail(&i).expect("Failed to get insn detail");
                let mnemonic = i.mnemonic().unwrap().to_string();
                let arch_detail = detail.arch_detail();
                let x86_detail = arch_detail.x86().unwrap();
                let ops = x86_detail.operands();

                let g = group_names(&cs, detail.groups());
                let group = &g.clone();
                let byte_len = i.bytes().len();

                let is_user = t.is_user;
                let mut inst_loads = 0;
                let mut inst_stores = 0;
                let mut with_reg = 0;

                for op in ops {
                    match op.op_type {
                        X86OperandType::Mem(_) => {
                            match op.access.unwrap() {
                                RegAccessType::ReadOnly => inst_loads += 1,
                                RegAccessType::WriteOnly => inst_stores += 1,
                                RegAccessType::ReadWrite => {
                                    inst_loads += 1;
                                    inst_stores += 1;
                                },
                            }
                        },
                        X86OperandType::Reg(_) => {
                            with_reg += 1;
                        },
                        _ => (),
                    }
                }

                let has_multi_mem = inst_loads + inst_stores >= 2;
                let has_mem = inst_loads + inst_stores >= 1;
                let mut has_br = false;
                let mut has_sse = false;
                let mut has_crypto = false;

                for cate in branch_groups {
                    if group.contains(cate) {
                        has_br = true;
                    }
                }
                for cate in sse_groups {
                    if group.contains(cate) {
                        has_sse = true;
                    }
                }
                for cate in crypto_groups{
                    if group.contains(cate) {
                        has_crypto = true;
                    }
                }

                let has_both_mem = inst_loads >= 1 && inst_stores >= 1;

                let mut groupKey: String = "".to_string();
                if has_sse {
                    groupKey = "sse".to_string();
                } else if has_crypto {
                    groupKey = "crypto".to_string();
                } else {
                    groupKey = group.clone();
                }

                let valueGroup = map.entry(groupKey).or_insert(Breakdown::default());
                let valueByte = map_bytes.entry(byte_len).or_insert(Breakdown::default());
                let value_mnem= map_mnem.entry(mnemonic).or_insert(Breakdown::default());

                Breakdown::update(valueGroup, is_user, byte_len, inst_loads, inst_stores, has_br, has_both_mem, has_mem, has_multi_mem);
                Breakdown::update(valueByte, is_user, byte_len, inst_loads, inst_stores, has_br, has_both_mem, has_mem, has_multi_mem);
                Breakdown::update(valueGroup, is_user, byte_len, inst_loads, inst_stores, has_br, has_both_mem, has_mem, has_multi_mem);
           }
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
            let t = get_next_trace_arm(&mut buf);

            for inst in t.insts.iter() {
                let d = cs.disasm_all(&inst.to_le_bytes(), 0).unwrap();
                for i in d.iter() {


                }
            }
        }
    }

    return Ok(());
}
