#objcopy: -O srec
#name: MRI floating point constants
#as: -M

# Test MRI floating point constants

S0.*
S118....(123456789ABCDEF03F800000412000004120000042)|(F0DEBC9A785634120000803F000020410000204100).*
S10.....(C80000)|(00C842).*
#pass
