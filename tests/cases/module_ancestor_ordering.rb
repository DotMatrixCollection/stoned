module M1; end
module M2; end
module M3
  include M1
  include M2
end

class Base
  include M3
end

class Child < Base; end

p Child.ancestors
