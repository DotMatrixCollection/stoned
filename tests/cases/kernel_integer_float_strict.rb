$stdout.sync = true

# Integer() and Float() must raise ArgumentError on invalid strings

# Integer() accepts valid integer strings
puts Integer("42")        # 42
puts Integer("0xff")      # 255
puts Integer("0b1010")    # 10
puts Integer("  99  ")    # 99 (trims whitespace)

# Integer() rejects float strings
begin
  Integer("3.14")
rescue ArgumentError => e
  puts "ArgumentError"
end

# Integer() rejects non-numeric
begin
  Integer("abc")
rescue ArgumentError => e
  puts "ArgumentError"
end

# Float() accepts valid float strings
puts Float("3.14")    # 3.14
puts Float("42")      # 42.0
puts Float("  1e3 ") # 1000.0

# Float() rejects non-numeric
begin
  Float("abc")
rescue ArgumentError => e
  puts "ArgumentError"
end

# Float() rejects empty
begin
  Float("")
rescue ArgumentError => e
  puts "ArgumentError"
end
