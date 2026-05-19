puts Array.instance_method(:each).arity
puts Array.instance_method(:include?).arity
puts Enumerator.instance_method(:with_index).arity
puts Array.instance_method(:each).bind([1, 2]).arity
