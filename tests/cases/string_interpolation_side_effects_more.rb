count = 0

def bump(label, value)
  "#{label}=#{value}"
end

msg = "#{bump("a", count += 1)} #{bump("b", count += 1)}"
p msg
p count

items = []
p "push: #{items << "x"}"
p items

name = "Ruby"
p "#{name.downcase}:#{name.upcase}:#{name.length}"
