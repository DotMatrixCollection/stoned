p = proc { 1 }
l = -> { 2 }

puts p.class
puts p.instance_of?(Proc)
puts p.is_a?(Proc)
puts p.respond_to?(:call)
puts p.respond_to?(:lambda?)
p p.respond_to?(:parameters)
p p.respond_to?(:to_proc)
p p.respond_to?(:source_location)
p p.respond_to?(:binding)
p Proc.instance_methods.include?(:call)
p Proc.instance_methods.include?(:parameters)
p Proc.instance_methods.include?(:source_location)
p Proc.instance_methods.include?(:binding)
p p.to_proc.equal?(p)
loc = p.source_location
p loc[0].end_with?("callable_reflection.rb")
p loc[1] > 0
captured = 7
p proc { captured }.binding.local_variable_get(:captured)
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
