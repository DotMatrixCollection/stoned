def bold(s); "[#{s}]"; end
x = "world"
text = <<~TEXT
  hello #{bold("there")}
  #{x.upcase}
TEXT
puts text.strip
