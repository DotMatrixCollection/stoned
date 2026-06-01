p 5.between?(1, 10)
p 0.between?(1, 10)
p 10.between?(1, 10)

p 5.clamp(1, 10)
p 0.clamp(1, 10)
p 15.clamp(1, 10)

p 3.14.between?(1.0, 5.0)
p 3.14.clamp(0.0, 3.0)

p "c".between?("a", "e")
p "z".between?("a", "e")
p "c".clamp("a", "e")
p "z".clamp("a", "e")
