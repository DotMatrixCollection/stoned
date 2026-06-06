str = "John 42"
m = str.match(/(?<name>\w+) (?<age>\d+)/)
p m[:name]
p m[:age].to_i
p m.named_captures
