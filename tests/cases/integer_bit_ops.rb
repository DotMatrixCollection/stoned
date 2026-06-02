# Integer bitwise operations
p 0b1010 & 0b1100   # AND  = 0b1000 = 8
p 0b1010 | 0b1100   # OR   = 0b1110 = 14
p 0b1010 ^ 0b1100   # XOR  = 0b0110 = 6
p ~0b1010           # NOT  = -(0b1010 + 1) = -11

# Bit shifts
p 1 << 4    # 16
p 16 >> 2   # 4
p 0b10110000 >> 4   # 0b1011 = 11

# Practical flag operations
READ    = 1 << 0
WRITE   = 1 << 1
EXECUTE = 1 << 2

perms = READ | WRITE
p perms
p (perms & READ) != 0
p (perms & EXECUTE) != 0

perms |= EXECUTE
p (perms & EXECUTE) != 0
perms &= ~WRITE
p (perms & WRITE) != 0

# Integer#[], bit indexing
n = 0b10110
p n[0]   # LSB
p n[1]
p n[2]
p n[3]
p n[4]

# to_s with base
p 255.to_s(2)
p 255.to_s(16)
p 8.to_s(2)
