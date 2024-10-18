import re
from typing import List, Set, Optional
from dataclasses import dataclass

@dataclass
class Instruction:
    address: int
    opcode: str
    rd: Optional[str] = None
    rs1: Optional[str] = None
    rs2: Optional[str] = None
    imm: Optional[int] = None

def parse_instruction(line: str) -> Optional[Instruction]:
    # Regular expression to match instruction components
    pattern = r'^\s*([0-9a-fA-F]+):\s+([0-9a-fA-F]+)\s+(\w+)\s*(.*)'
    match = re.match(pattern, line)
    if not match:
        return None

    address = int(match.group(1), 16)
    opcode = match.group(3)
    args = match.group(4).split(',')

    inst = Instruction(address=address, opcode=opcode)

    if opcode in ['ld', 'lw', 'lh', 'lb', 'sd', 'sw', 'sh', 'sb']:
        inst.rd = args[0].strip()
        imm_reg = args[1].strip().split('(')
        inst.imm = int(imm_reg[0]) if imm_reg[0] else 0
        inst.rs1 = imm_reg[1].rstrip(')')
    elif opcode in ['beq', 'bne', 'blt', 'bge', 'bltu', 'bgeu']:
        inst.rs1 = args[0].strip()
        inst.rs2 = args[1].strip()
        # Handle cases where the immediate is a symbol
        imm_match = re.search(r'([0-9a-fA-F]+)', args[2])
        if imm_match:
            inst.imm = int(imm_match.group(1), 16)
    elif opcode == 'csrr':
        inst.rd = args[0].strip()
        inst.imm = parse_immediate(args[1].strip())
    elif opcode == 'csrw':
        inst.rs1 = args[0].strip()
        inst.imm = parse_immediate(args[1].strip())
    elif opcode in ['ori', 'addi', 'add']:
        inst.rd = args[0].strip()
        inst.rs1 = args[1].strip()
        if len(args) > 2:
            inst.imm = parse_immediate(args[2].strip())
            if inst.imm is None:
                inst.rs2 = args[2].strip()

    print(f"Parsed instruction: {inst}")
    return inst

def parse_immediate(imm_str: str) -> Optional[int]:
    try:
        return int(imm_str, 0)
    except ValueError:
        # If it's not a valid integer, return None
        return None

def analyze_riscv_assembly(filename: str) -> None:
    with open(filename, 'r') as file:
        assembly = file.readlines()

    instructions = [inst for inst in map(parse_instruction, assembly) if inst is not None]

    start_address, end_address = find_code_snippet_boundaries(instructions)
    if start_address is None or end_address is None:
        print("Error: Could not find code snippet boundaries.")
        return

    print(f"Code snippet boundaries: 0x{start_address:08x} - 0x{end_address:08x}")

    if not is_code_snippet_self_contained(instructions, start_address, end_address):
        print("Warning: Code snippet is not self-contained.")

    public_registers = find_public_registers(instructions)
    speculatively_exposed_registers = find_speculatively_exposed_registers(instructions)

    print(f"Public registers: {public_registers}")
    print(f"Speculatively exposed registers: {speculatively_exposed_registers}")

def find_code_snippet_boundaries(instructions: List[Instruction]) -> tuple[Optional[int], Optional[int]]:
    start_address = None
    end_address = None

    for inst in instructions:
        if inst.opcode == 'csrw' and inst.rs1 == '0x802':
            print(f"Found start boundary: 0x{inst.address:08x}")
            start_address = inst.address
        elif inst.opcode == 'csrr' and inst.imm == 0x802:
            print(f"Found end boundary: 0x{inst.address:08x}")
            end_address = inst.address
            if start_address is not None:
                break

    return start_address, end_address

def is_code_snippet_self_contained(instructions: List[Instruction], start_address: int, end_address: int) -> bool:
    for inst in instructions:
        if inst.opcode in ['beq', 'bne', 'blt', 'bge', 'bltu', 'bgeu', 'j', 'jal', 'jalr']:
            if inst.imm and (inst.imm < start_address or inst.imm > end_address):
                print(f"Non-self-contained jump found at 0x{inst.address:08x} to 0x{inst.imm:08x}")
                return False
    return True

def find_public_registers(instructions: List[Instruction]) -> Set[str]:
    public_registers = set()
    for inst in instructions:
        if inst.opcode in ['ld', 'lw', 'lh', 'lb', 'sd', 'sw', 'sh', 'sb']:
            public_registers.add(inst.rs1)
            print(f"Public register found: {inst.rs1} in instruction: {inst}")
    return public_registers

def find_speculatively_exposed_registers(instructions: List[Instruction]) -> Set[str]:
    exposed_registers = set()
    in_branch = False
    for inst in instructions:
        if inst.opcode in ['beq', 'bne', 'blt', 'bge', 'bltu', 'bgeu']:
            print(f"Entering speculative region at 0x{inst.address:08x}")
            in_branch = True
        elif in_branch:
            if inst.opcode in ['ld', 'lw', 'lh', 'lb', 'sd', 'sw', 'sh', 'sb']:
                exposed_registers.add(inst.rs1)
                print(f"Speculatively exposed register found: {inst.rs1} in instruction: {inst}")
            elif inst.opcode == 'csrr' and inst.imm == 0x802:
                print(f"Exiting speculative region at 0x{inst.address:08x}")
                in_branch = False
    return exposed_registers

if __name__ == "__main__":
    analyze_riscv_assembly("riscv_assembly.txt")
