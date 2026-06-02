# sprintf / format / % string formatting
p sprintf("%d", 42)
p sprintf("%05d", 42)
p sprintf("%.2f", 3.14159)
p sprintf("%s and %s", "foo", "bar")
p sprintf("%10s", "right")
p sprintf("%-10s|", "left")

# format is an alias for sprintf
p format("0x%X", 255)
p format("%+d %+d", 5, -3)
p format("%e", 12345.6789)
p format("%g", 12345.6789)
p format("%g", 0.00012345)

# % operator
p "Hello, %s!" % "world"
p "%d items at $%.2f each" % [3, 4.5]
p "%08.3f" % 3.14

# Multiple types
p sprintf("%d %s %f %s", 1, "two", 3.0, :four)

# Named references (Ruby 2.x+)
p sprintf("%{name} is %{age}", name: "Alice", age: 30)
