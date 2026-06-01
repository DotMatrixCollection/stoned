module M1; end
module M2; end
module M3; end

class Base
  include M1
end

class Child < Base
  include M2
  prepend M3
end

puts Child.included_modules.inspect
puts Base.included_modules.inspect
puts M1.included_modules.inspect

module Outer
  include M3
end
class Foo
  include Outer
end
puts Foo.included_modules.inspect
