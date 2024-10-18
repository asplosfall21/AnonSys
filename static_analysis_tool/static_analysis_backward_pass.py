import re
from typing import List, Set, Optional, Dict
from dataclasses import dataclass, field
import sys

@dataclass
class Instruction:
    address: int
    opcode: str
    rd: Optional[str] = None
    rs1: Optional[str] = None
    rs2: Optional[str] = None
    imm: Optional[int] = None
    sources: List['Instruction'] = field(default_factory=list)
    previous: 'Instruction' = None
    is_branch: bool = False
    is_jump: bool = False
    is_load: bool = False
    is_store: bool = False

    def __hash__(self):
        return hash(self.address)  # Use only the address for hashing

    def __eq__(self, other):
        if not isinstance(other, Instruction):
            return NotImplemented
        return self.address == other.address

registers = set([f'x{i}' for i in range(32)] +
                [f'a{i}' for i in range(8)] +
                [f's{i}' for i in range(12)] +
                [f't{i}' for i in range(7)] +
                ['zero', 'ra', 'sp', 'gp', 'tp', 'fp'])
    
def is_register(operand: str) -> bool:
    # Remove any leading/trailing whitespace and potential commas
    cleaned_operand = operand.strip().rstrip(',')
    # Check if the cleaned operand is in our set of register names
    return cleaned_operand in registers

def parse_instruction(line: str) -> Optional[Instruction]:
    pattern = r'^\s*([0-9a-fA-F]+):\s+([0-9a-fA-F]+)\s+(\w+)\s*(.*)'
    match = re.match(pattern, line)
    if not match:
        return None

    address = int(match.group(1), 16)
    opcode = match.group(3)
    args = match.group(4).split(',')

    inst = Instruction(address=address, opcode=opcode)

    if opcode in ['ld', 'lw', 'lh', 'lb', 'lbu', 'lhu', 'lwu', 'flw']:
        inst.rd = args[0].strip()
        imm_reg = args[1].strip().split('(')
        inst.imm = int(imm_reg[0]) if imm_reg[0] else 0
        inst.rs1 = imm_reg[1].rstrip(')')
        inst.is_load = True
    elif opcode in ['sd', 'sw', 'sh', 'sb', 'fsw']:
        inst.rs2 = args[0].strip()
        imm_reg = args[1].strip().split('(')
        inst.imm = int(imm_reg[0]) if imm_reg[0] else 0
        inst.rs1 = imm_reg[1].rstrip(')')
        inst.is_store = True
    elif opcode in ['beq', 'bne', 'blt', 'bge', 'bltu', 'bgeu']:
        inst.rs1 = args[0].strip()
        inst.rs2 = args[1].strip()
        imm_match = re.search(r'([0-9a-fA-F]+)', args[2])
        if imm_match:
            inst.imm = int(imm_match.group(1), 16)
        inst.is_branch = True
    elif opcode in ['bnez', 'beqz']:
        inst.rs1 = args[0].strip()
        imm_match = re.search(r'([0-9a-fA-F]+)', args[1])
        if imm_match:
            inst.imm = int(imm_match.group(1), 16)
        inst.is_branch = True
    elif opcode in ['j', 'jal', 'jalr']:
        if opcode != 'j':
            inst.rd = args[0].strip()
        if opcode == 'jalr':
            inst.rs1 = args[1].strip()
        else:
            imm_match = re.search(r'([0-9a-fA-F]+)', args[-1])
            if imm_match:
                inst.imm = int(imm_match.group(1), 16)
        inst.is_jump = True
    elif opcode == 'csrr':
        inst.rd = args[0].strip()
        inst.imm = parse_immediate(args[1].strip())
    elif opcode == 'csrw':
        inst.rs1 = args[0].strip()
        inst.imm = parse_immediate(args[1].strip())
    elif opcode in ['csrrs', 'csrrc']:
        inst.rd = args[0].strip()
        inst.rs1 = args[1].strip()
        inst.imm = parse_immediate(args[2].strip())
    elif opcode in ['li','lui', 'auipc']:
        inst.rd = args[0].strip()
        inst.imm = parse_immediate(args[1].strip())
    elif opcode in ['addi', 'slti', 'sltiu', 'xori', 'ori', 'andi', 'slli', 'srli', 'srai']:
        inst.rd = args[0].strip()
        inst.rs1 = args[1].strip()
        inst.imm = parse_immediate(args[2].strip())
    elif opcode in ['add', 'addw', 'sub', 'sll', 'slt', 'sltu', 'xor', 'srl', 'sra', 'or', 'and', 'fadd', 'fsub', 'fmul', 'fdiv', 'flt']:
        inst.rd = args[0].strip()
        inst.rs1 = args[1].strip()
        if is_register(args[2].strip()):
            inst.rs2 = args[2].strip()
        else:
            inst.imm = parse_immediate(args[2].strip())
    elif opcode in ['mv', 'fmv', 'sext']:  # pseudo-instruction for addi rd, rs1, 0
        inst.rd = args[0].strip()
        inst.rs1 = args[1].strip()
        inst.imm = 0
    elif opcode == 'ret':
        inst.rd = 'zero'
    else:
        print(f"Unknown opcode: {opcode}")
        exit(1)

    return inst

def parse_immediate(imm_str: str) -> Optional[int]:
    try:
        return int(imm_str, 0)
    except ValueError:
        return None

def build_instruction_graph(instructions: List[Instruction]) -> Dict[int, Instruction]:
    inst_dict = {inst.address: inst for inst in instructions}
    for i, inst in enumerate(instructions):
        if i > 0:
            inst.previous = instructions[i-1]
        if inst.is_branch or inst.is_jump:
            if inst.imm is not None:
                target = inst_dict.get(inst.imm)
                if target:
                    target.sources.append(inst)
    return inst_dict

@dataclass
class BackwardState: # Represent a path in the backward analysis
    first_inst: bool = True
    current_inst: Instruction = None
    executed_inst: List[Instruction] = field(default_factory=list) # executed_inst[i] is the i-th executed instruction on that path
    executed_inst_dep_rg: List[frozenset[str]] = field(default_factory=list) # executed_inst_dep_rg[i] is the dependency set of executed_inst[i]
    dep_reg: frozenset[str] = field(default_factory=frozenset) # dependency set of the current path i.e. which current registers might be leaked

    def __hash__(self):
        return hash((self.first_inst, self.current_inst, self.dep_reg))

def backward_taint_analysis(inst_dict: Dict[int, Instruction], trans: Instruction) -> Set[str]:
    # print(f"Backward taint analysis for {trans.address:08x}, {trans.opcode} {trans.rs1}")

    if trans.is_load or trans.is_store:
        init_state = BackwardState(first_inst=True, current_inst=trans, dep_reg=frozenset({trans.rs1}))
    elif trans.is_branch:
        init_state = BackwardState(first_inst=True, current_inst=trans, dep_reg=frozenset({trans.rs1, trans.rs2}))
    else:
        print(f"Error: only load, store and branch instructions can be transmitters")
        exit(1)

    # Initialize the state list with the initial state
    states = list()
    states.append(init_state)
    
    final_dependencies = list()
    
    while states:
        st = states.pop()
        #print(f"{st.current_inst.address:08x}, {st.current_inst.opcode}, {st.dep_reg}")
        inst = st.current_inst

        if inst in st.executed_inst: # If the instruction has been already executed on this path...
            index = st.executed_inst.index(inst)
            if st.executed_inst_dep_rg[index] == st.dep_reg: # ...check that the dependency set is the same, if it is we've been through a loop that does not change the dependency set, then we can skip it
                continue
            # else:
            #     print(f"Instruction {inst.address:08x}, {inst.opcode} already executed but state is different")
            #     print(f"st.executed_inst_dep_rg[index]: {st.executed_inst_dep_rg[index]}")
            #     print(f"st.dep_reg: {st.dep_reg}")

        new_dep_reg = set(st.dep_reg)
        st.executed_inst.insert(0, inst)
        st.executed_inst_dep_rg.insert(0, st.dep_reg)
        
        if inst.is_load or inst.is_store: # If the instruction is a load or a store, the address it accesses is leaked
            if (inst.rs1 in new_dep_reg) and (not st.first_inst):
                new_dep_reg.remove(inst.rs1)

        # if inst.is_branch:
        #     if inst.rs1 in new_dep_reg and not st.first_inst:
        #         new_dep_reg.remove(inst.rs1)
        #     if inst.rs2 in new_dep_reg and not st.first_inst:
        #         new_dep_reg.remove(inst.rs2)
        
        rd = inst.rd
        if rd in new_dep_reg: # If the destination register is in the dependency set, it transforms the dependency set
            new_dep_reg.remove(rd)
            if inst.is_load or inst.is_store: # If it is a load or store, we are encountering a Spectre gadget
                print(f"Error: Spectre gadget detected")
                exit(1)
            else: # Otherwise, the instruction transforms the dependency set. Immediate values are public and not leaked.
                if inst.rs1:
                    new_dep_reg.add(inst.rs1)
                if inst.rs2:
                    new_dep_reg.add(inst.rs2)
        
        if inst.opcode in ['csrrs']: # Reached the first instruction of the code snippet
            final_dependencies.append((new_dep_reg, st.executed_inst))
            continue
        
        if new_dep_reg: # If the dependency set is not empty, propagate the analysis to the previous instructions
            for src in inst.sources:
                new_st = BackwardState(first_inst=False, current_inst=src, dep_reg=frozenset(new_dep_reg), executed_inst=st.executed_inst.copy(), executed_inst_dep_rg=st.executed_inst_dep_rg.copy())
                states.append(new_st)
            if inst.previous:
                new_st = BackwardState(first_inst=False, current_inst=inst.previous, dep_reg=frozenset(new_dep_reg), executed_inst=st.executed_inst.copy(), executed_inst_dep_rg=st.executed_inst_dep_rg.copy())
                states.append(new_st)
         
    return final_dependencies

def analyze_riscv_assembly(filename: str) -> None:
    with open(filename, 'r') as file:
        assembly = file.readlines()

    instructions = [inst for inst in map(parse_instruction, assembly) if inst is not None]
    inst_dict = build_instruction_graph(instructions)

    start_address, end_address = find_code_snippet_boundaries(instructions)
    if start_address is None or end_address is None:
        print("Error: Could not find code snippet boundaries.")
        return

    print(f"Code snippet boundaries: 0x{start_address:08x} - 0x{end_address:08x}")
    
    if not is_code_snippet_self_contained(instructions, start_address, end_address):
        print("Warning: Code snippet is not self-contained.")
        return
    print("Code snippet is self-contained.")

    speculative_transmitters = find_speculative_transmitters(instructions, start_address, end_address)

    print(f"Number of speculative transmitters: {len(speculative_transmitters)}")

    print("Backward taint analysis...")
    exposed_values_with_path = list()
    for transmitter in speculative_transmitters:
        exposed_values_with_path.extend(backward_taint_analysis(inst_dict, transmitter))

    print("This program will leak the initial value of the following registers:")
    print(f"Number of exposed values: {len(exposed_values_with_path)}")
    exposed_values = set()
    for dep in exposed_values_with_path:
        if isinstance(dep, tuple) and len(dep) == 2:
            print(f"- Register(s): {', '.join(dep[0])}")
            print("  Execution path:")
            for inst in dep[1]:
               print(f"    0x{inst.address:08x}: {inst.opcode} {inst.rd or ''} {inst.rs1 or ''} {inst.rs2 or ''} {inst.imm or ''}")
            print()  # Add a blank line between paths
            exposed_values.update(dep[0])
        else:
            raise ValueError(f"Unexpected dependency format: {dep}")
        
    print("Unique exposed values:")
    for value in exposed_values:
        print(f"- {value}")

def find_code_snippet_boundaries(instructions: List[Instruction]) -> tuple[Optional[int], Optional[int]]:
    start_address = None
    end_address = None

    for inst in instructions:
        if inst.opcode == 'csrrs' and inst.rs1 == '0x802':
            print(f"Found start boundary: 0x{inst.address:08x}")
            start_address = inst.address
        elif inst.opcode == 'csrrc' and inst.rs1 == '0x802':
            print(f"Found end boundary: 0x{inst.address:08x}")
            end_address = inst.address
            if start_address is not None:
                break

    return start_address, end_address

def is_code_snippet_self_contained(instructions: List[Instruction], start_address: int, end_address: int) -> bool:
    for inst in instructions:
        if inst.is_branch or inst.is_jump:
            if inst.address < start_address or inst.address > end_address:
                continue
            if inst.imm and (inst.imm < start_address or inst.imm > end_address):
                print(f"Non-self-contained jump found at 0x{inst.address:08x} to 0x{inst.imm:08x}")
                return False
    return True

def find_speculative_transmitters(instructions: List[Instruction], start_address: int, end_address: int) -> Set[str]:
    transmitters = list()
    for inst in instructions:
        if inst.address < start_address or inst.address > end_address:
            continue
        if inst.is_load or inst.is_store or inst.is_branch:
            transmitters.append(inst)
            print(f"Speculative transmitter found: {inst.address:08x}, {inst.opcode} {inst.rs1}, {inst.rs2}, {inst.imm}")
    return transmitters

if __name__ == "__main__":
    filename = sys.argv[1] if len(sys.argv) > 1 else "memcpy_shm.asm"
    print(f"Analyzing {filename}")
    analyze_riscv_assembly(filename)