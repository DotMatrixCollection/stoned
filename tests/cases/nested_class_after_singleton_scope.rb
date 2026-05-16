class OuterSingletonLeak
  class << self
    def marker
      "outer"
    end
  end

  class Inner
    def write
      "inner"
    end

    alias print write
  end
end

puts OuterSingletonLeak::Inner.new.print
