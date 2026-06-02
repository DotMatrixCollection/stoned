# Regexp options: case-insensitive, multiline, extended
p "Hello" =~ /hello/i ? true : false
p "Hello" =~ /hello/ ? true : false

# /m makes . match newlines
multi = "line1\nline2"
p multi =~ /line1.line2/m ? true : false
p multi =~ /line1.line2/ ? true : false

# /x allows whitespace and comments in pattern
phone = /
  \d{3}  # area code
  [-.]   # separator
  \d{4}  # number
/x
p "555-1234" =~ phone ? true : false
p "555.1234" =~ phone ? true : false
p "5551234"  =~ phone ? true : false

# Combined flags
p "HELLO\nWORLD" =~ /hello.*world/im ? true : false

# Regexp.new with options
r = Regexp.new("foo", Regexp::IGNORECASE)
p r === "FOO"
p r === "bar"

# options method
p /hello/i.options == Regexp::IGNORECASE
p /hello/m.options == Regexp::MULTILINE
p /hello/im.options == (Regexp::IGNORECASE | Regexp::MULTILINE)

# Named captures
m = "John 30".match(/(?<name>\w+) (?<age>\d+)/)
p m[:name]
p m[:age]
