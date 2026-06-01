a = [10, 20, 30, 40]

p a.at(1)
p a.at(-1)
p a.at(9)
p a.fetch(2)
p a.fetch(9, :missing)
p a.fetch(9) { |idx| "bad #{idx}" }

begin
  a.fetch(9)
rescue => e
  puts e.class
end

p a.slice(1)
p a.slice(1, 2)
p a.slice(1..2)
p a.slice(-2, 4)
p a.slice(10)
p a.slice(10, 2)

p a.respond_to?(:at)
p a.respond_to?(:fetch)
p a.respond_to?(:slice)
p Array.instance_methods.include?(:slice)
