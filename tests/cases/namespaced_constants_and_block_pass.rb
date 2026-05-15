module Outer
end

class Outer::Base
  def self.kind
    "base"
  end
end

class Outer::Child < Outer::Base
end

module Outer::Inner
  VALUE = 3
end

class Outer::Oops < ::StandardError
end

class Forwarder
  def target(*args)
    yield(*args)
  end

  def wrapper(old, *args, &block)
    send old, *args, &block
  end
end

puts Outer::Child.superclass == Outer::Base
puts(::Outer::Child.kind)
puts Outer::Inner::VALUE
puts Outer::Oops.superclass == StandardError
puts Forwarder.new.wrapper(:target, 1, 2, 3) { |a, b, c| "#{a}-#{b}-#{c}" }
