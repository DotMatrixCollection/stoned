# :"#{expr}" — interpolated symbol literal
name = :timeout
sym = :"#{name}="
puts sym.inspect
puts sym.class

prefix = "get_"
%w[name age rank].each do |field|
  puts :"#{prefix}#{field}".inspect
end

# Static form still works
puts :"hello".inspect
puts :"hello world".inspect
