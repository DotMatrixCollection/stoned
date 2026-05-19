s = "hello world"
s[0, 5] = "goodbye"
p s

s2 = "abcdef"
s2[2] = "X"
p s2

s3 = "hello"
s3[1..3] = "ELL"
p s3

# Return value is the rhs
s4 = "testing"
x = (s4[0, 4] = "done")
p x   # "done"
p s4  # "done testing" — wait, let me calculate: "done" replaces "test", leaves "ing" → "doning"
