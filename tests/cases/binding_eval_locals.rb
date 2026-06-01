x = 10
y = 20

b = binding
p b.local_variables.sort

b.eval("z = x + y")
p b.eval("z")

def capture_binding(val)
  local = val * 2
  binding
end

b2 = capture_binding(5)
p b2.eval("local")
p b2.eval("val")
