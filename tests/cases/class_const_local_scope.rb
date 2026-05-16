class Outer
  class Foo
    def who; "outer"; end
  end
  module Inner
    class Foo < Object
      def who; "inner"; end
    end
  end
end
puts Outer::Foo.new.who
puts Outer::Inner::Foo.new.who
puts Outer::Inner::Foo.superclass
