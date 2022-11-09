
use std::fs;
use std::io::{self, BufReader, Read};


use capstone::arch::arm64::{Arm64Operand, Arm64InsnDetail, Arm64OpMem, Arm64OperandType};
use capstone::arch::x86::{X86Operand, X86InsnDetail, X86OpMem, X86OperandType};
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
    pub tot_load: [usize; 2],
    pub tot_store: [usize; 2],

    // type
    pub is_br: [usize; 2],
    pub is_mem: [usize; 2],
    pub is_fp: [usize; 2],
    pub is_crypto: [usize; 2],
    pub is_priviledge: [usize; 2],

    // Freak cases
    pub with_both: [usize; 2],
    pub mem_branch: [usize; 2],
    pub has_multi_mem: [usize; 2],

    pub byte_sizes: [[usize; 13]; 2]
    
}

impl Breakdown {
    fn print_stats(&self) {
        println!("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}", self.tot, self.user, self.sys, 
                self.tot_load[0], self.tot_load[1], self.tot_store[0], self.tot_store[1], 
                self.is_br[0], self.is_br[1],
                self.is_mem[0], self.is_mem[1],
                self.is_fp[0], self.is_fp[1],
                self.is_crypto[0], self.is_crypto[1],
                self.is_priviledge[0], self.is_priviledge[1],
                self.with_both[0], self.with_both[1],
                self.mem_branch[0], self.mem_branch[1],
                self.has_multi_mem[0], self.has_multi_mem[1],
        )
    }
    fn print_byte_dist(&self, idx: usize) {
        println!("{},{},{},{},{},{},{},{},{},{},{},{}",
                self.byte_sizes[idx][1],
                self.byte_sizes[idx][2],
                self.byte_sizes[idx][3],
                self.byte_sizes[idx][4],
                self.byte_sizes[idx][5],
                self.byte_sizes[idx][6],
                self.byte_sizes[idx][7],
                self.byte_sizes[idx][8],
                self.byte_sizes[idx][9],
                self.byte_sizes[idx][10],
                self.byte_sizes[idx][11],
                self.byte_sizes[idx][12])
 
    }
    fn update (value: &mut Breakdown, is_user: bool, byte_len: usize, 
               inst_loads: usize, inst_stores: usize, 
               is_br: bool, is_priviledge: bool, is_mem: bool, is_fp:bool, is_crypto: bool,
               has_both_mem: bool, has_mem: bool, has_multi_mem: bool) {

        value.tot += 1;

        let idx = if is_user {
            value.user += 1;
            0
        } else {
            value.sys += 1;
            1
        };

        value.byte_sizes[idx][byte_len] += 1;
        value.tot_load[idx] += inst_loads;
        value.tot_store[idx] += inst_stores;

        if is_br {
            value.is_br[idx] += 1;
        }

        if is_mem {
            value.is_mem[idx] += 1;
        }

        if is_fp {
            value.is_fp[idx] += 1;
        }

        if is_crypto {
            value.is_crypto[idx] += 1;
        }

        if is_priviledge {
            value.is_priviledge[idx] += 1;
        }

        if has_both_mem {
            value.with_both[idx] += 1;
        }

        if has_mem && is_br {
            value.mem_branch[idx] += 1;
        }

        if has_multi_mem {
            value.has_multi_mem[idx] += 1;
        }

        
    }
}

impl Default for Breakdown {
    fn default () -> Breakdown{
        Breakdown{tot: 0, user: 0, sys:0, 
            tot_load: [0;2], tot_store: [0;2], 
            is_br: [0;2], is_priviledge: [0;2], is_mem: [0;2], is_fp: [0;2], is_crypto: [0;2],  
            with_both: [0;2], mem_branch: [0;2], has_multi_mem: [0;2], byte_sizes: [[0usize; 13]; 2]}
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
#![warn(unreachable_code)]

    let arch = std::env::args().nth(1).expect("no arch was given");
    let path_src = std::path::PathBuf::from(std::env::args().nth(2).expect("no path given"));

    let f = fs::File::open(path_src)?;

    let mut buf = BufReader::new(f);

    let mut curr_inst = 0usize;
    let mut map: HashMap<String, Breakdown> = HashMap::new();
    let mut map_bytes: HashMap<usize, Breakdown> = HashMap::new();
    let mut map_mnem_mem: HashMap<String, Breakdown> = HashMap::new();
    let mut map_mnem_br: HashMap<String, Breakdown> = HashMap::new();
    let mut map_mnem_alu: HashMap<String, Breakdown> = HashMap::new();
    let mut map_mnem_priv: HashMap<String, Breakdown> = HashMap::new();
    let mut map_mnem_fp: HashMap<String, Breakdown> = HashMap::new();
    let mut map_mnem_crypto: HashMap<String, Breakdown> = HashMap::new();
    let mut map_mnem_others_spec: HashMap<String, Breakdown> = HashMap::new();

    fn print_stats(_map_groups: &HashMap<String, Breakdown>, _map_bytes: &HashMap<usize, Breakdown> , hash_list: &[(&str, &HashMap<String, Breakdown>); 7]) {
        println!("Groups:");
        for (groups, breakdown) in _map_groups {
            println!("{groups:?},{:?}", breakdown);
        }
        println!("Mem Bytes:");
        for (groups, breakdown) in _map_bytes {
            println!("{groups:?},{:?}", breakdown);
        }

        println!("Stats:");
        for hash_table in hash_list {
            println!("{}:", hash_table.0);
            for (groups, breakdown) in hash_table.1 {
                        print!("{groups:?},");
                        breakdown.print_stats();
            }
        }

        println!("ByteDist:");
        for hash_table in hash_list {
            println!("{}:", hash_table.0);
            for (groups, breakdown) in hash_table.1 {
                print!("{groups:?},user");
                breakdown.print_byte_dist(0);
                print!("{groups:?},kernel");
                breakdown.print_byte_dist(1);
            }
        }
    }


    if arch == "x86" {
        
        println!("Logging X86:");
        let cs = Capstone::new()
            .x86()
            .mode(arch::x86::ArchMode::Mode64)
            .detail(true)
            .build()
            .unwrap();

        let branch_groups = ["jump", "ret", "branch_relative", "call"];
        let fp_groups = ["sse1", "sse2", "sse41", "sse42", "ssse3", "fpu"];
        let crypto_groups = ["adx", "aes", "pclmul"];
        let others_groups = ["not64bitmode", "fsgsbse"];

        loop {
            let t = get_next_trace_x86(&mut buf);
            let d = cs.disasm_all(&t.insts_bytes, 0).unwrap();

            for i in d.iter() {
                if (curr_inst % 100000000) == 0 {
                    let hash_list = [
                        ("Mem", &map_mnem_mem),
                        ("Br", &map_mnem_br),
                        ("Alu", &map_mnem_alu),
                        ("Priv", &map_mnem_priv),
                        ("Fp", &map_mnem_fp),
                        ("Crypto", &map_mnem_crypto),
                        ("Others", &map_mnem_others_spec)];
 
                    println!("Insts[{}]", curr_inst);
                    print_stats(&map, &map_bytes, &hash_list);
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

                for op in ops {
                    match op.op_type {
                        X86OperandType::Mem(_) => {
                            match op.access {
                                Some(RegAccessType::ReadOnly) => inst_loads += 1,
                                Some(RegAccessType::WriteOnly) => inst_stores += 1,
                                Some(RegAccessType::ReadWrite) => {
                                    inst_loads += 1;
                                    inst_stores += 1;
                                },
                                _ => {
                                    // For some reason ins and movzx din't have operation
                                    if mnemonic.contains("ins") || mnemonic.contains("movzx") {
                                        inst_loads += 1;
                                        inst_stores += 1;
                                    } else {
                                        println!("Did not find what kind of memory operation: {:?}", detail);
                                        println!("{}", i);
                                        let output: &[(&str, String)] = &[
                                            ("insn id:", format!("{:?}", i.id().0)),
                                            ("bytes:", format!("{:?}", i.bytes())),
                                            ("read regs:", reg_names(&cs, detail.regs_read())),
                                            ("write regs:", reg_names(&cs, detail.regs_write())),
                                            ("insn groups:", group_names(&cs, detail.groups())),
                                        ];

                                        for &(ref name, ref message) in output.iter() {
                                            println!("{:4}{:12} {}", "", name, message);
                                        }

                                        for op in arch_detail.operands() {
                                            println!("{:8}{:?}", "", op);
                                        }
                                    }
                                }
                            }
                        },
                        X86OperandType::Reg(_) => {
//                            with_reg += 1;
                        },
                        _ => (),
                    }
                }

                let has_multi_mem = inst_loads + inst_stores >= 2;
                let has_mem = inst_loads + inst_stores >= 1;
                let mut is_br = false;
                let mut is_mem = false;
                let mut is_fp= false;
                let mut is_crypto = false;
                let mut is_priviledge = false;
                let mut is_others_special = false;

                for cate in branch_groups {
                    if group.contains(cate) {
                        is_br = true;
                    }
                }

                for cate in fp_groups {
                    if group.contains(cate) {
                        is_fp = true;
                    }
                }

                if group.contains("mmx") {
                    is_fp = true;
                }

                for cate in others_groups {
                    if group.contains(cate) {
                        is_others_special = true;
                    }
                }

                for cate in crypto_groups{
                    if group.contains(cate) {
                        is_crypto = true;
                    }
                }

                if mnemonic.contains("mov") || mnemonic.contains("lea") || 
                    mnemonic.contains("ins") || mnemonic.contains("stosd") {
                    is_mem = true;
                }
                

                if group.contains("priviledge")  {
                    is_priviledge = true;
                }

                let has_both_mem = inst_loads >= 1 && inst_stores >= 1;

                let mut group_key: String = "".to_string();
                let value : &mut Breakdown;

                if is_priviledge {
                    group_key = "priviledge".to_string();
                    value = map_mnem_priv.entry(mnemonic).or_insert(Breakdown::default());
                } else if is_fp {
                    group_key = "fp".to_string();
                    value = map_mnem_fp.entry(mnemonic).or_insert(Breakdown::default());
                } else if is_crypto {
                    group_key = "crypto".to_string();
                    value = map_mnem_crypto.entry(mnemonic).or_insert(Breakdown::default());
                } else if is_br {
                    group_key = "br".to_string();
                    value = map_mnem_br.entry(mnemonic).or_insert(Breakdown::default());
                } else if is_mem {
                    group_key = "mov".to_string();
                    value = map_mnem_mem.entry(mnemonic).or_insert(Breakdown::default());
                } else if is_others_special {
                    group_key = "others".to_string();
                    value = map_mnem_others_spec.entry(mnemonic).or_insert(Breakdown::default());
                } else {
                    group_key = "logic".to_string();
                    value = map_mnem_alu.entry(mnemonic).or_insert(Breakdown::default());
                }

                let value_group = map.entry(group_key).or_insert(Breakdown::default());
                let value_byte = map_bytes.entry(byte_len).or_insert(Breakdown::default());
                Breakdown::update(value_group, is_user, byte_len, 
                    inst_loads, inst_stores, 
                    is_br, is_priviledge, is_mem, is_fp, is_crypto, 
                    has_both_mem, has_mem, has_multi_mem);
                Breakdown::update(value_byte, is_user, byte_len, 
                    inst_loads, inst_stores, 
                    is_br, is_priviledge, is_mem, is_fp, is_crypto, 
                    has_both_mem, has_mem, has_multi_mem);
                Breakdown::update(value, is_user, byte_len, 
                    inst_loads, inst_stores, 
                    is_br, is_priviledge, is_mem, is_fp, is_crypto, 
                    has_both_mem, has_mem, has_multi_mem);
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

        let branch_groups = ["return", "branch_relative", "call", "jump"];
        let fp_groups = ["neon", "fparmv8"];
        let crypto_groups = ["crypto"];
        let others_groups = ["pointer"];

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
