$stdout.sync = true

# MatchData#named_captures and MatchData#[] with Range

m = "John 30".match(/(?<name>\w+) (?<age>\d+)/)

# Named group access via symbol/string key (already worked)
puts m[:name]          # John
puts m["age"]          # 30

# named_captures returns hash
puts m.named_captures.inspect  # {"name"=>"John", "age"=>"30"}

# names returns array of capture names
puts m.names.inspect   # ["name", "age"]

# MatchData#[] with Range returns sub-array
m2 = "2024-01-15".match(/(\d{4})-(\d{2})-(\d{2})/)
puts m2[0]             # 2024-01-15  (whole match)
puts m2[1]             # 2024
puts m2[1..3].inspect  # ["2024", "01", "15"]
puts m2[1...3].inspect # ["2024", "01"]

# No named captures → empty hash
m3 = "hello".match(/h(\w+)o/)
puts m3.named_captures.inspect  # {}
puts m3.names.inspect           # []
