p = proc { 1 }
l = -> { 2 }

puts p.class
puts p.instance_of?(Proc)
puts p.is_a?(Proc)
puts p.respond_to?(:call)
puts p.respond_to?(:lambda?)
puts l.lambda?
puts l.respond_to?(:call)
puts l.respond_to?(:nope)

class Proc
  def visible_proc
    1
  end

  private

  def hidden_proc
    2
  end
end

puts p.respond_to?(:visible_proc)
puts p.respond_to?(:hidden_proc)
puts p.respond_to?(:hidden_proc, true)
puts p.visible_proc

begin
  puts p.hidden_proc
rescue NoMethodError => e
  puts e.class
end

begin
  puts p.public_send(:hidden_proc)
rescue NoMethodError => e
  puts e.class
end

puts p.send(:hidden_proc)
